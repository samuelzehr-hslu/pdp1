// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Alle Pin-Nummern und I2C-Adressen stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").
// Die reine Logik (Winkel, Grenzwert, Taste entprellen, Ausfall) steht in logik.h / logik.cpp.
// Diese Datei hier macht nur den Ablauf und redet mit der Hardware.
//
// So funktioniert es (REQ-002):
//   1. Zwei IMUs (Oberarm und Unterarm) messen Beschleunigung UND Drehgeschwindigkeit (Gyro).
//   2. Der Ellbogenwinkel zählt nur die Drehung um die Scharnier-Achse des Ellbogens.
//      Andere Drehungen (Hand drehen, Schulter drehen, Arm schwenken) ändern ihn nicht.
//   3. Einmessen (Taste kurz drücken): 2 s Arm still gestreckt (hängend) = Referenz (0 Grad).
//      Ein Vibrationsstoss meldet "jetzt bewegen": 4 s Arm 2 Mal beugen und strecken
//      und dabei den Oberarm leicht mitbewegen (kleine Kreise), damit das Gerät die
//      Scharnier-Achse findet. Ist sie nicht eindeutig (z. B. Oberarm still), bricht
//      das Einmessen ab ("Grund: Oberarm"). Sonst gibt es die Bestätigungsvibration.
//      Nach dem Einschalten gibt es KEINE automatische Einmessung.
//   4. Ist der Arm mehr als GRENZWERT_GRAD über der Referenz -> Motor vibriert.
//   5. Antwortet eine IMU 3 Mal in Folge nicht (REQ-001), gilt sie als ausgefallen:
//      Die Überstreckungs-Warnung ist dann aus, der Motor zeigt ein Fehlermuster
//      (3 kurze Stösse alle 5 Sekunden) und der Serielle Monitor meldet FEHLER.
//      Nach dem Ausfall gibt es keine Warnung, bis die Lage wieder bekannt ist (höchstens 5 s).
//
// Testen ohne Hardware: Wokwi-Simulation, siehe diagram.json im selben Ordner.
//
// Hinweis: F("Text") speichert den Text im Flash statt im knappen RAM.

#include <Wire.h>
#include <math.h>
#include "pins.h"
#include "logik.h"

// ---------- Einstellungen (hier dürft ihr Werte ändern) ----------
const float GRENZWERT_GRAD   = 5.0; // ab so vielen Grad Überstreckung wird gewarnt (QA-03: 0 bis 10)
const float HYSTERESE_GRAD   = 2.0;  // Motor geht erst wieder aus, wenn 2 Grad unter dem Grenzwert
const unsigned long ENTPRELL_MS = 80;  // so lange muss die Überstreckung anliegen (README: 50-100 ms)
const unsigned long MESS_PAUSE_MS = 20; // alle 20 ms messen = 50 Hz (README: 50-100 Hz)
const int MOTOR_STAERKE      = 200;  // PWM-Wert 0 (aus) bis 255 (volle Kraft)
const unsigned long TASTER_PRELL_MS = 50; // so lange "prellt" die Taste nach einer Änderung
const uint8_t FEHLER_SCHWELLE = 3;   // so viele Fehlschläge in Folge = IMU ausgefallen (REQ-001)
const unsigned long FEHLER_MELDUNG_MS = 1000;   // Ausfall wird alle 1000 ms gemeldet
const unsigned long REFERENZ_MELDUNG_MS = 1000; // "Keine Referenz" wird alle 1000 ms gemeldet
const unsigned long ZYKLUS_MELDUNG_MS = 1000;   // "Zyklus max" wird alle 1000 ms gemeldet (AK-15)
const unsigned long IMU_BEREICH_MS = 1000;      // so oft wird der Messbereich der IMUs neu eingestellt

// Einmessen (README: "Bedienung", AK-6 und AK-7)
const unsigned long EINMESS_STILL_MS  = 2000;   // Phase 1: Arm still halten
const unsigned long EINMESS_GESAMT_MS = 6000;   // Phase 1 + Phase 2 (4000 ms Bewegung)
const unsigned long STOSS_BEWEGUNG_MS = 100;    // Vibrationsstoss "jetzt bewegen"
const unsigned long STOSS_BESTAETIGUNG_MS = 150; // Bestätigungsvibration am Ende
const float EINMESS_MIN_BEUGUNG_GRAD = 30.0;    // so gross muss die Beugung beim Einmessen sein

// Messbereich der IMU (MPU6050), siehe Datenblatt:
const float ACC_PRO_G   = 16384.0;  // Rohwerte pro g bei +-2 g (Standard)
const float GYRO_PRO_GRAD = 65.5;   // Rohwerte pro Grad/s bei +-500 Grad/s (Register 0x1B = 0x08)

// ---------- Variablen, die sich während dem Betrieb ändern ----------
EllbogenZustand ellbogen;            // alles zur Winkelmessung (siehe logik.h)
bool referenzGueltig = false;        // true, sobald einmal erfolgreich eingemessen wurde
GrenzwertZustand grenzZustand = {false, 0}; // Motor aus, Arm nicht zu weit (siehe logik.h)
TasteZustand tasteZustand = {false, 0};     // Taste nicht gedrückt (siehe logik.h)
ImuStatus imu1 = {0, false};         // Ausfall-Zustand IMU 1 (Oberarm)
ImuStatus imu2 = {0, false};         // Ausfall-Zustand IMU 2 (Unterarm)
FehlerMusterZustand fehlerMuster = {false, 0}; // Fehlermuster läuft nicht
bool ausfallVorher = false;          // galt im letzten Messzyklus eine IMU als ausgefallen?
unsigned long letzteMessung = 0;     // wann haben wir zuletzt gemessen?
unsigned long letzteAusgabe = 0;     // wann haben wir zuletzt die Abweichung geschrieben?
unsigned long letzteFehlerMeldung = 0;    // wann haben wir zuletzt FEHLER gemeldet?
unsigned long letzteReferenzMeldung = 0;  // wann haben wir zuletzt "Keine Referenz" gemeldet?
unsigned long letzteZyklusMeldung = 0;    // wann haben wir zuletzt "Zyklus max" gemeldet?
unsigned long letzterImuBereich = 0;      // wann haben wir zuletzt den IMU-Messbereich eingestellt?
unsigned long zyklusMaxUs = 0;       // längste Messzeit seit der letzten Meldung (Mikrosekunden)
unsigned long letzteAktualisierungUs = 0; // Zeitpunkt der letzten Winkel-Aktualisierung (micros)
bool hatLetzteAktualisierung = false;     // gibt es schon einen letzten Zeitpunkt?

// Einmessen (läuft ohne Warten, Schritt für Schritt in loop())
uint8_t einmessPhase = 0;            // 0 = aus, 1 = Phase Still, 2 = Phase Bewegung
unsigned long einmessStartMs = 0;    // Zeitpunkt, an dem die Taste gedrückt wurde

// Vibrationsstoss (z. B. 100 ms oder 150 ms), ohne Warten
bool stossAktiv = false;             // läuft gerade ein Vibrationsstoss?
unsigned long stossEndeMs = 0;       // wann endet er?


// Läuft einmal beim Start.
void setup() {
  Serial.begin(9600);
  Wire.begin();                       // I2C starten (SDA = A4, SCL = A5)
  Wire.setClock(400000);              // schneller Bus (400 kHz), damit ein Messzyklus kurz bleibt (AK-15)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);   // IN2 bleibt immer LOW
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus

  pinMode(PIN_TASTER, INPUT_PULLUP);  // Taste gegen GND, nicht gedrückt = HIGH

  ellbogenZuruecksetzen(ellbogen);    // keine Referenz, keine Achse, keine Lage

  // Beide IMUs aufwecken und melden, ob sie antworten
  if (imuStarten(IMU1_ADRESSE)) {
    Serial.println(F("IMU 1 (Oberarm): OK"));
  } else {
    Serial.println(F("IMU 1 (Oberarm): FEHLER - Verkabelung pruefen"));
  }
  if (imuStarten(IMU2_ADRESSE)) {
    Serial.println(F("IMU 2 (Unterarm): OK"));
  } else {
    Serial.println(F("IMU 2 (Unterarm): FEHLER - Verkabelung pruefen"));
  }

  // Kein automatisches Einmessen mehr (AK-9): Einmessen nur per Taste.
  Serial.println(F("Zum Einmessen: Arm strecken und Taste kurz druecken."));
}


// Läuft immer wieder, solange der Arduino Strom hat.
void loop() {
  unsigned long jetzt = millis();

  // Vibrationsstoss beenden, wenn seine Zeit um ist (läuft bei jedem Durchgang)
  stossPruefen(jetzt);

  // Taste: kurzer Druck startet das Einmessen (nur wenn nicht schon eines läuft)
  if (tasteGedrueckt() && einmessPhase == 0) {
    einmessenBeginnen(jetzt);
  }

  // Nur alle MESS_PAUSE_MS messen, sonst gleich wieder von vorne
  if (jetzt - letzteMessung < MESS_PAUSE_MS) {
    return;
  }
  letzteMessung = jetzt;

  // Einmessen: nach 2000 ms beginnt die Bewegungsphase (Phase 2) mit einem Vibrationsstoss
  if (einmessPhase == 1 && (jetzt - einmessStartMs) >= EINMESS_STILL_MS) {
    einmessenBewegungStart(ellbogen);
    einmessPhase = 2;
    stossStarten(jetzt, STOSS_BEWEGUNG_MS);
    Serial.println(F("Jetzt Arm 2 Mal beugen und strecken."));
  }

  // Alle 1000 ms den Messbereich der IMUs neu einstellen (falls eine IMU kurz neu gestartet ist)
  if (jetzt - letzterImuBereich >= IMU_BEREICH_MS) {
    letzterImuBereich = jetzt;
    if (!imu1.ausgefallen) {
      imuBereichSetzen(IMU1_ADRESSE);
    }
    if (!imu2.ausgefallen) {
      imuBereichSetzen(IMU2_ADRESSE);
    }
  }

  // 1. Beide IMUs einzeln messen. Eine fehlende IMU bremst nicht (kurzer Fehler, kein Warten).
  //    Ist eine IMU schon ausgefallen, wecken wir sie vor dem Lesen nochmal auf:
  //    Eine neu eingesteckte IMU startet nämlich im Schlafmodus.
  if (imu1.ausgefallen) {
    imuStarten(IMU1_ADRESSE);
  }
  if (imu2.ausgefallen) {
    imuStarten(IMU2_ADRESSE);
  }
  unsigned long zyklusStartUs = micros();      // ab hier zählt die Zykluszeit (Lesen und Rechnen)
  ImuMessung messung1;
  ImuMessung messung2;
  bool uebersteuert1 = false;
  bool uebersteuert2 = false;
  bool ok1 = imuLesen(IMU1_ADRESSE, messung1, uebersteuert1);
  bool ok2 = imuLesen(IMU2_ADRESSE, messung2, uebersteuert2);

  // 2. Ausfall-Zustand nachführen (logik.cpp)
  bool aus1 = imuStatusAktualisieren(imu1, ok1, FEHLER_SCHWELLE);
  bool aus2 = imuStatusAktualisieren(imu2, ok2, FEHLER_SCHWELLE);
  bool ausfall = aus1 || aus2;

  // Fällt eine Messung aus, während eingemessen wird: Einmessen abbrechen (AK-7 c)
  if (einmessPhase != 0 && (!ok1 || !ok2)) {
    einmessenAbbrechenMelden(EINMESSEN_GRUND_IMU);
  }

  // 3. Winkel nachführen (nur mit zwei guten Messungen, ohne Ausfall und ohne Übersteuerung)
  bool winkelAktualisiert = false;
  bool zyklusVerwendet = false;
  if (ok1 && ok2 && !ausfall) {
    if (uebersteuert1 || uebersteuert2) {
      // Gyro am Anschlag (z. B. sehr schnelles Drehen): Zyklus überspringen.
      // Das ist KEIN Ausfall (sonst gäbe schnelles Drehen einen Fehleralarm, AK-14).
    } else {
      unsigned long jetztUs = micros();
      if (hatLetzteAktualisierung) {
        float dt = (float)(jetztUs - letzteAktualisierungUs) * 0.000001f;
        winkelAktualisiert = ellbogenAktualisieren(ellbogen, messung1, messung2, dt);
        zyklusVerwendet = true;
        if (!winkelAktualisiert && einmessPhase != 0) {
          // Ungültige Werte oder zu lange Pause: Einmessen wäre falsch, also abbrechen (AK-7 c)
          einmessenAbbrechenMelden(EINMESSEN_GRUND_IMU);
        }
      }
      letzteAktualisierungUs = jetztUs;
      hatLetzteAktualisierung = true;
    }
  }
  unsigned long zyklusDauerUs = micros() - zyklusStartUs;
  if (zyklusDauerUs > zyklusMaxUs) {
    zyklusMaxUs = zyklusDauerUs;
  }

  // 4. Beginnt oder endet der Ausfall gerade?
  if (ausfall && !ausfallVorher) {
    // Ausfall beginnt: Entprellung und Hysterese frisch starten (REQ-001, AK-9)
    grenzZustand.warnungAktiv = false;
    grenzZustand.zuWeitSeit = 0;
    stossAktiv = false;               // ein laufender Stoss ist nicht mehr wichtig
    letzteFehlerMeldung = jetzt;
    fehlerMelden();                   // sofort melden
  }
  if (!ausfall && ausfallVorher) {
    // Ausfall ist vorbei: Fehlermuster stoppen, Motor sofort aus, Zustand frisch.
    // Die Lage der IMUs ist jetzt unbekannt (REQ-002, AK-13): keine Warnung, bis sie wieder bekannt ist.
    fehlerMusterMotorAn(fehlerMuster, false, jetzt);
    grenzZustand.warnungAktiv = false;
    grenzZustand.zuWeitSeit = 0;
    ellbogenLageVerwerfen(ellbogen);
    feedbackAusgeben(false);
    Serial.println(F("IMUs wieder da."));
  }
  ausfallVorher = ausfall;

  // 5. Je nach Lage: Fehlermuster, Einmessen, normale Warnung oder Zyklus überspringen
  if (ausfall) {
    // Fail-safe: Keine Überstreckungs-Warnung, nur das Fehlermuster
    bool musterAn = fehlerMusterMotorAn(fehlerMuster, true, jetzt);
    feedbackAusgeben(motorSollAn(true, false, musterAn));

    if (jetzt - letzteFehlerMeldung >= FEHLER_MELDUNG_MS) {
      letzteFehlerMeldung = jetzt;
      fehlerMelden();
    }
  } else if (einmessPhase != 0) {
    // Während dem Einmessen gibt es keine Überstreckungs-Warnung. Der Motor vibriert nur
    // bei den Stössen (Zeichen "jetzt bewegen" und Bestätigung).
    grenzZustand.warnungAktiv = false;
    grenzZustand.zuWeitSeit = 0;
    // Nach 6000 ms ist das Einmessen fertig
    if (zyklusVerwendet && winkelAktualisiert && einmessPhase == 2 &&
        (jetzt - einmessStartMs) >= EINMESS_GESAMT_MS) {
      einmessenFertig(jetzt);
    }
  } else if (ok1 && ok2) {
    // Beide Messungen sind gut: Grenzwert prüfen (nur wenn Referenz UND Lage bekannt sind)
    float abweichung = 0.0;
    bool lageOk = ellbogenAbweichung(ellbogen, abweichung);
    bool zuWeit = false;
    if (referenzGueltig && lageOk) {  // ohne Referenz oder Lage gibt es keine Warnung (AK-9, AK-13)
      zuWeit = grenzwertPruefen(grenzZustand, abweichung, jetzt,
                                GRENZWERT_GRAD, HYSTERESE_GRAD, ENTPRELL_MS);
    } else {
      grenzZustand.warnungAktiv = false;   // Entprellung frisch starten, wenn die Lage wieder da ist
      grenzZustand.zuWeitSeit = 0;
    }
    feedbackAusgeben(motorSollAn(false, zuWeit, false));

    // 4x pro Sekunde den aktuellen Wert auf den Seriellen Monitor schreiben
    if (referenzGueltig && lageOk && (jetzt - letzteAusgabe >= 250)) {
      letzteAusgabe = jetzt;
      Serial.print(F("Abweichung: "));
      zahlSchreiben(abweichung);
      Serial.print(F(" Grad"));
      if (zuWeit) {
        Serial.print(F("  -> VIBRATION"));
      }
      Serial.println();
    }
  }
  // Sonst: Eine Messung ist fehlgeschlagen, aber noch unter der Schwelle (1 oder 2 Mal).
  // Dann wird dieser Zyklus übersprungen: kein Winkel, kein grenzwertPruefen,
  // der Motor bleibt, wie er war (REQ-001, AK-3).

  // 6. Fehlt die Referenz, regelmässig daran erinnern (auch während eines Ausfalls)
  if (!referenzGueltig && (jetzt - letzteReferenzMeldung >= REFERENZ_MELDUNG_MS)) {
    letzteReferenzMeldung = jetzt;
    Serial.println(F("Keine Referenz - Arm strecken und Taste druecken."));
  }

  // 7. Alle 1000 ms die längste Zykluszeit melden (AK-15)
  if (jetzt - letzteZyklusMeldung >= ZYKLUS_MELDUNG_MS) {
    letzteZyklusMeldung = jetzt;
    Serial.print(F("Zyklus max: "));
    Serial.print(zyklusMaxUs);
    Serial.println(F(" us"));
    zyklusMaxUs = 0;
  }
}


// =====================================================================
// SENSOR: IMUs auslesen (bei anderem IMU-Typ nur diesen Teil ändern)
// =====================================================================

// Stellt den Messbereich der IMU (MPU6050) ein: Gyro +-500 Grad/s (Register 0x1B).
// Gibt true zurück, wenn die IMU antwortet.
bool imuBereichSetzen(int adresse) {
  Wire.beginTransmission(adresse);
  Wire.write(0x1B);                   // Register "GYRO_CONFIG"
  Wire.write(0x08);                   // 0x08 = +-500 Grad/s
  int fehler = Wire.endTransmission();
  return fehler == 0;
}

// Weckt eine IMU (MPU6050) auf und stellt den Messbereich ein. Gibt true zurück, wenn sie antwortet.
bool imuStarten(int adresse) {
  Wire.beginTransmission(adresse);
  Wire.write(0x6B);                   // Register "PWR_MGMT_1"
  Wire.write(0x00);                   // 0 = Schlafmodus aus
  int fehler = Wire.endTransmission();
  if (fehler != 0) {
    return false;
  }
  return imuBereichSetzen(adresse);
}

// Liest Beschleunigung (in g) und Drehgeschwindigkeit (in Grad/s) einer IMU.
// Gibt true zurück, wenn die Messung geklappt hat. Nur dann ist "messung" gültig.
// Gibt false zurück, wenn die IMU die Anfrage nicht bestätigt
// oder weniger als die 14 angeforderten Bytes liefert (REQ-001, AK-1).
// uebersteuert wird true, wenn ein Gyro-Rohwert am Anschlag ist (+-32768, AK-14).
bool imuLesen(int adresse, ImuMessung &messung, bool &uebersteuert) {
  uebersteuert = false;
  Wire.beginTransmission(adresse);
  Wire.write(0x3B);                   // Register "ACCEL_XOUT_H" = erster Beschleunigungswert
  if (Wire.endTransmission(false) != 0) {
    return false;                     // IMU hat nicht geantwortet
  }

  // 14 Bytes: Beschleunigung X,Y,Z (6), Temperatur (2), Gyro X,Y,Z (6)
  uint8_t anzahl = Wire.requestFrom((uint8_t)adresse, (uint8_t)14);
  if (anzahl < 14 || Wire.available() < 14) {
    while (Wire.available() > 0) {    // Reste wegwerfen, damit nichts hängen bleibt
      Wire.read();
    }
    return false;                     // zu wenig Daten
  }

  // Jedes Byte einzeln und der Reihe nach lesen (erst High-Byte, dann Low-Byte)
  int16_t ax = bytesZuZahl();
  int16_t ay = bytesZuZahl();
  int16_t az = bytesZuZahl();
  bytesZuZahl();                      // Temperatur brauchen wir nicht
  int16_t gx = bytesZuZahl();
  int16_t gy = bytesZuZahl();
  int16_t gz = bytesZuZahl();

  // Gyro am Anschlag? Dann ist der Wert nicht mehr richtig.
  if (gx == 32767 || gx == -32768 || gy == 32767 || gy == -32768 ||
      gz == 32767 || gz == -32768) {
    uebersteuert = true;
  }

  // Rohwerte in g und Grad pro Sekunde umrechnen
  messung.ax = (float)ax / ACC_PRO_G;
  messung.ay = (float)ay / ACC_PRO_G;
  messung.az = (float)az / ACC_PRO_G;
  messung.gx = (float)gx / GYRO_PRO_GRAD;
  messung.gy = (float)gy / GYRO_PRO_GRAD;
  messung.gz = (float)gz / GYRO_PRO_GRAD;
  return true;
}

// Liest 2 Bytes vom I2C-Bus und macht daraus eine Zahl (High-Byte zuerst).
int16_t bytesZuZahl() {
  uint8_t hoch = Wire.read();
  uint8_t tief = Wire.read();
  return (int16_t)(((uint16_t)hoch << 8) | tief);
}


// =====================================================================
// EINMESSEN (Schritt für Schritt, ohne die Schleife zu blockieren)
// =====================================================================

// Startet das Einmessen: Phase 1 (Still, 2000 ms).
void einmessenBeginnen(unsigned long jetzt) {
  einmessPhase = 1;
  einmessStartMs = jetzt;
  einmessenStart(ellbogen);
  grenzZustand.warnungAktiv = false;
  grenzZustand.zuWeitSeit = 0;
  feedbackAusgeben(false);
  Serial.println(F("Einmessen... Arm ruhig gestreckt halten."));
}

// Schliesst das Einmessen nach 6000 ms ab: Bestätigung oder Abbruch-Meldung.
void einmessenFertig(unsigned long jetzt) {
  bool ok = einmessenAbschliessen(ellbogen, EINMESS_MIN_BEUGUNG_GRAD);
  einmessPhase = 0;
  grenzZustand.warnungAktiv = false;
  grenzZustand.zuWeitSeit = 0;
  if (ok) {
    referenzGueltig = true;
    Serial.println(F("Referenz gesetzt."));
    stossStarten(jetzt, STOSS_BESTAETIGUNG_MS);   // Bestätigungsvibration (README: "Bedienung", Punkt 3)
  } else {
    einmessenAbbruchMelden(einmessenGrund(ellbogen));
  }
}

// Bricht das Einmessen ab (z. B. IMU ausgefallen) und schreibt den Grund.
// Die bisherige Referenz und Achse bleiben, es gibt keine Bestätigungsvibration (AK-7).
void einmessenAbbrechenMelden(uint8_t grund) {
  einmessenAbbrechen(ellbogen);
  einmessPhase = 0;
  stossAktiv = false;
  feedbackAusgeben(false);
  einmessenAbbruchMelden(grund);
}

// Schreibt die Zeile "Einmessen abgebrochen - Grund: ..." auf den Seriellen Monitor.
void einmessenAbbruchMelden(uint8_t grund) {
  Serial.print(F("Einmessen abgebrochen - Grund: "));
  if (grund == EINMESSEN_GRUND_BEUGUNG) {
    Serial.println(F("Beugung"));
  } else if (grund == EINMESSEN_GRUND_RUHIG) {
    Serial.println(F("ruhig"));
  } else if (grund == EINMESSEN_GRUND_OBERARM) {
    Serial.println(F("Oberarm"));
  } else {
    Serial.println(F("IMU"));
  }
}


// =====================================================================
// AKTOR und BEDIENUNG
// =====================================================================

// Schaltet den Vibrationsmotor ein oder aus.
// Läuft gerade ein Vibrationsstoss (Einmessen), bleibt dieser unverändert.
void feedbackAusgeben(bool motorAn) {
  if (stossAktiv) {
    return;
  }
  if (motorAn) {
    analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  } else {
    analogWrite(PIN_MOTOR_PWM, 0);
  }
}

// Startet einen Vibrationsstoss von dauerMs Millisekunden (ohne zu warten).
void stossStarten(unsigned long jetzt, unsigned long dauerMs) {
  stossAktiv = true;
  stossEndeMs = jetzt + dauerMs;
  analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
}

// Beendet den Vibrationsstoss, wenn seine Zeit um ist.
void stossPruefen(unsigned long jetzt) {
  if (stossAktiv && (long)(jetzt - stossEndeMs) >= 0) {
    stossAktiv = false;
    analogWrite(PIN_MOTOR_PWM, 0);
  }
}

// Schreibt eine Zahl mit einer Nachkommastelle auf den Seriellen Monitor (z. B. -12.3).
// Eigene kleine Funktion, weil Serial.print(Kommazahl) viel Platz im Flash braucht.
void zahlSchreiben(float x) {
  long zehntel = (long)(x * 10.0f + (x < 0 ? -0.5f : 0.5f));   // auf 0,1 runden
  if (zehntel < 0) {
    Serial.print('-');
    zehntel = -zehntel;
  }
  Serial.print(zehntel / 10);
  Serial.print('.');
  Serial.print(zehntel % 10);
}

// Schreibt eine FEHLER-Zeile mit dem Namen jeder ausgefallenen IMU auf den Seriellen Monitor.
void fehlerMelden() {
  Serial.print(F("FEHLER: "));
  if (imu1.ausgefallen) {
    Serial.print(F("IMU 1 (Oberarm) "));
  }
  if (imu2.ausgefallen) {
    Serial.print(F("IMU 2 (Unterarm) "));
  }
  Serial.println(F("ausgefallen"));
}

// Gibt true zurück, wenn die Taste gerade eben gedrückt wurde (nur einmal pro Druck).
// Die Entprell-Logik steht in logik.cpp (tasteNeuGedrueckt).
bool tasteGedrueckt() {
  bool istGedrueckt = (digitalRead(PIN_TASTER) == LOW);   // LOW = gedrückt
  return tasteNeuGedrueckt(tasteZustand, istGedrueckt, millis(), TASTER_PRELL_MS);
}
