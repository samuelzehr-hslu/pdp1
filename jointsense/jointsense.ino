// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Alle Pin-Nummern und I2C-Adressen stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").
// Die reine Logik (Winkel, Grenzwert, Taste entprellen, Ausfall) steht in logik.h / logik.cpp.
// Diese Datei hier macht nur den Ablauf und redet mit der Hardware.
//
// So funktioniert es:
//   1. Zwei IMUs (Oberarm und Unterarm) messen je ihre Neigung.
//   2. Ellbogenwinkel = Neigung Unterarm - Neigung Oberarm.
//   3. Einmessen: Arm strecken, Taste kurz drücken -> dieser Winkel ist die Referenz (0 Grad).
//   4. Ist der Arm mehr als GRENZWERT_GRAD über der Referenz -> Motor vibriert.
//   5. Antwortet eine IMU 3 Mal in Folge nicht (REQ-001), gilt sie als ausgefallen:
//      Die Überstreckungs-Warnung ist dann aus, der Motor zeigt ein Fehlermuster
//      (3 kurze Stösse alle 5 Sekunden) und der Serielle Monitor meldet FEHLER.
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
const int EINMESS_ANZAHL     = 25;   // so viele Messungen werden beim Einmessen gemittelt
const unsigned long TASTER_PRELL_MS = 50; // so lange "prellt" die Taste nach einer Änderung
const uint8_t FEHLER_SCHWELLE = 3;   // so viele Fehlschläge in Folge = IMU ausgefallen (REQ-001)
const unsigned long FEHLER_MELDUNG_MS = 1000;   // Ausfall wird alle 1000 ms gemeldet
const unsigned long REFERENZ_MELDUNG_MS = 1000; // "Keine Referenz" wird alle 1000 ms gemeldet

// ---------- Variablen, die sich während dem Betrieb ändern ----------
float referenzWinkel = 0.0;          // Winkel bei gestrecktem Arm (wird beim Einmessen gesetzt)
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


// Läuft einmal beim Start.
void setup() {
  Serial.begin(9600);
  Wire.begin();                       // I2C starten (SDA = A4, SCL = A5)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);   // IN2 bleibt immer LOW
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus

  pinMode(PIN_TASTER, INPUT_PULLUP);  // Taste gegen GND, nicht gedrückt = HIGH

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

  // Beim Einschalten automatisch einmessen. Fehlt eine IMU, bricht das Einmessen ab
  // und es gibt noch keine Referenz (dann nur per Taste, siehe einmessen()).
  einmessen();
  Serial.println(F("Zum neu Einmessen: Arm strecken und Taste kurz druecken."));
}


// Läuft immer wieder, solange der Arduino Strom hat.
void loop() {
  if (tasteGedrueckt()) {
    einmessen();
  }

  // Nur alle MESS_PAUSE_MS messen, sonst gleich wieder von vorne
  unsigned long jetzt = millis();
  if (jetzt - letzteMessung < MESS_PAUSE_MS) {
    return;
  }
  letzteMessung = jetzt;

  // 1. Beide IMUs einzeln messen. Eine fehlende IMU bremst nicht (kurzer Fehler, kein Warten).
  //    Ist eine IMU schon ausgefallen, wecken wir sie vor dem Lesen nochmal auf:
  //    Eine neu eingesteckte IMU startet nämlich im Schlafmodus.
  float neigungOberarm = 0.0;
  float neigungUnterarm = 0.0;
  if (imu1.ausgefallen) {
    imuStarten(IMU1_ADRESSE);
  }
  if (imu2.ausgefallen) {
    imuStarten(IMU2_ADRESSE);
  }
  bool ok1 = neigungLesen(IMU1_ADRESSE, neigungOberarm);
  bool ok2 = neigungLesen(IMU2_ADRESSE, neigungUnterarm);

  // 2. Ausfall-Zustand nachführen (logik.cpp)
  bool aus1 = imuStatusAktualisieren(imu1, ok1, FEHLER_SCHWELLE);
  bool aus2 = imuStatusAktualisieren(imu2, ok2, FEHLER_SCHWELLE);
  bool ausfall = aus1 || aus2;

  // 3. Beginnt oder endet der Ausfall gerade?
  if (ausfall && !ausfallVorher) {
    // Ausfall beginnt: Entprellung und Hysterese frisch starten (REQ-001, AK-9)
    grenzZustand.warnungAktiv = false;
    grenzZustand.zuWeitSeit = 0;
    letzteFehlerMeldung = jetzt;
    fehlerMelden();                   // sofort melden
  }
  if (!ausfall && ausfallVorher) {
    // Ausfall ist vorbei: Fehlermuster stoppen, Motor sofort aus, Zustand frisch
    fehlerMusterMotorAn(fehlerMuster, false, jetzt);
    grenzZustand.warnungAktiv = false;
    grenzZustand.zuWeitSeit = 0;
    feedbackAusgeben(false);
    Serial.println(F("IMUs wieder da."));
  }
  ausfallVorher = ausfall;

  // 4. Je nach Lage: Fehlermuster, normale Warnung oder Zyklus überspringen
  if (ausfall) {
    // Fail-safe: Keine Überstreckungs-Warnung, nur das Fehlermuster
    bool musterAn = fehlerMusterMotorAn(fehlerMuster, true, jetzt);
    feedbackAusgeben(motorSollAn(true, false, musterAn));

    if (jetzt - letzteFehlerMeldung >= FEHLER_MELDUNG_MS) {
      letzteFehlerMeldung = jetzt;
      fehlerMelden();
    }
  } else if (ok1 && ok2) {
    // Beide Messungen sind gut: Winkel rechnen und Grenzwert prüfen
    float winkel = ellbogenWinkel(neigungOberarm, neigungUnterarm);
    float abweichung = winkel - referenzWinkel;
    bool zuWeit = false;
    if (referenzGueltig) {            // ohne Referenz gibt es keine Warnung (AK-11)
      zuWeit = grenzwertPruefen(grenzZustand, abweichung, jetzt,
                                GRENZWERT_GRAD, HYSTERESE_GRAD, ENTPRELL_MS);
    }
    feedbackAusgeben(motorSollAn(false, zuWeit, false));

    // 4x pro Sekunde den aktuellen Wert auf den Seriellen Monitor schreiben
    if (referenzGueltig && (jetzt - letzteAusgabe >= 250)) {
      letzteAusgabe = jetzt;
      Serial.print(F("Abweichung: "));
      Serial.print(abweichung, 1);
      Serial.print(F(" Grad"));
      if (zuWeit) {
        Serial.print(F("  -> VIBRATION"));
      }
      Serial.println();
    }
  }
  // Sonst: Eine Messung ist fehlgeschlagen, aber noch unter der Schwelle (1 oder 2 Mal).
  // Dann wird dieser Zyklus übersprungen: kein Winkel, kein grenzwertPruefen,
  // der Motor bleibt, wie er war (AK-3).

  // 5. Fehlt die Referenz, regelmässig daran erinnern (auch während eines Ausfalls)
  if (!referenzGueltig && (jetzt - letzteReferenzMeldung >= REFERENZ_MELDUNG_MS)) {
    letzteReferenzMeldung = jetzt;
    Serial.println(F("Keine Referenz - Arm strecken und Taste druecken."));
  }
}


// =====================================================================
// SENSOR: IMUs auslesen (bei anderem IMU-Typ nur diesen Teil ändern)
// =====================================================================

// Weckt eine IMU (MPU6050) auf. Gibt true zurück, wenn sie antwortet.
bool imuStarten(int adresse) {
  Wire.beginTransmission(adresse);
  Wire.write(0x6B);                   // Register "PWR_MGMT_1"
  Wire.write(0x00);                   // 0 = Schlafmodus aus
  int fehler = Wire.endTransmission();
  return fehler == 0;
}

// Liest die Neigung einer IMU in Grad (nur aus der Erdbeschleunigung).
// Gibt true zurück, wenn die Messung geklappt hat. Nur dann ist "neigung" gültig.
// Gibt false zurück, wenn die IMU die Anfrage nicht bestätigt
// oder weniger als die 6 angeforderten Bytes liefert (REQ-001, AK-1).
// Achtung: Stimmt nur, wenn der Arm sich nicht schnell bewegt.
// Später: Gyroskop dazunehmen (Komplementärfilter, siehe README).
bool neigungLesen(int adresse, float &neigung) {
  Wire.beginTransmission(adresse);
  Wire.write(0x3B);                   // Register "ACCEL_XOUT_H" = erster Beschleunigungswert
  if (Wire.endTransmission(false) != 0) {
    return false;                     // IMU hat nicht geantwortet
  }

  uint8_t anzahl = Wire.requestFrom((uint8_t)adresse, (uint8_t)6);  // 6 Bytes: X, Y, Z (je 2 Bytes)
  if (anzahl < 6 || Wire.available() < 6) {
    while (Wire.available() > 0) {    // Reste wegwerfen, damit nichts hängen bleibt
      Wire.read();
    }
    return false;                     // zu wenig Daten
  }

  // Jedes Byte einzeln und der Reihe nach lesen (erst High-Byte, dann Low-Byte)
  int16_t ax = bytesZuZahl();
  int16_t ay = bytesZuZahl();
  int16_t az = bytesZuZahl();

  // Welche Achse die richtige ist, hängt davon ab, wie die IMU am Arm sitzt!
  // Die Rechnung selbst steht in logik.cpp.
  neigung = neigungAusBeschleunigung(ax, ay, az);
  return true;
}

// Liest 2 Bytes vom I2C-Bus und macht daraus eine Zahl (High-Byte zuerst).
int16_t bytesZuZahl() {
  uint8_t hoch = Wire.read();
  uint8_t tief = Wire.read();
  return (int16_t)(((uint16_t)hoch << 8) | tief);
}


// =====================================================================
// EINMESSEN
// =====================================================================

// Misst die Referenz: Mittelwert über mehrere Messungen bei gestrecktem Arm.
// Schlägt EINE Messung fehl, wird abgebrochen: Die alte Referenz bleibt,
// es gibt keine Bestätigungsvibration (REQ-001, AK-10).
void einmessen() {
  Serial.println(F("Einmessen... Arm ruhig gestreckt halten."));
  float summe = 0.0;
  for (int i = 0; i < EINMESS_ANZAHL; i++) {
    float neigungOberarm = 0.0;
    float neigungUnterarm = 0.0;
    bool ok1 = neigungLesen(IMU1_ADRESSE, neigungOberarm);
    bool ok2 = neigungLesen(IMU2_ADRESSE, neigungUnterarm);
    if (!ok1 || !ok2) {
      Serial.println(F("Einmessen abgebrochen - IMU antwortet nicht."));
      return;
    }
    summe = summe + ellbogenWinkel(neigungOberarm, neigungUnterarm);
    delay(MESS_PAUSE_MS);
  }
  referenzWinkel = referenzBerechnen(summe, EINMESS_ANZAHL);
  referenzGueltig = true;

  Serial.print(F("Referenz gesetzt: "));
  Serial.print(referenzWinkel, 1);
  Serial.println(F(" Grad"));

  // Kurze Bestätigungsvibration (README: "Bedienung", Punkt 3)
  analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  delay(150);
  analogWrite(PIN_MOTOR_PWM, 0);
}


// =====================================================================
// AKTOR und BEDIENUNG
// =====================================================================

// Schaltet den Vibrationsmotor ein oder aus.
void feedbackAusgeben(bool motorAn) {
  if (motorAn) {
    analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  } else {
    analogWrite(PIN_MOTOR_PWM, 0);
  }
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
