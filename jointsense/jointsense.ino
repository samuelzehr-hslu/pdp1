// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Alle Pin-Nummern und I2C-Adressen stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").
// Die reine Logik (Winkel, Grenzwert, Taste entprellen, Ausfall) steht in logik.h / logik.cpp.
// Diese Datei hier macht nur den Ablauf und redet mit der Hardware.
//
// So funktioniert es (REQ-002):
//   1. Zwei IMUs (Oberarm und Unterarm) messen Beschleunigung UND Drehgeschwindigkeit (Gyro).
//   2. Der Ellbogenwinkel zählt nur die Drehung um die Scharnier-Achse des Ellbogens.
//      Andere Drehungen (Hand drehen, Schulter drehen, Arm schwenken) ändern ihn nicht.
//   3. Einmessen (REQ-005). Bestätigen heisst: Taste kurz drücken ODER im Seriellen
//      Monitor Enter (oder Leertaste) schicken. Vorher zeigt der Monitor die Neigung beider IMUs.
//      Start mit einer Bestätigung. Schritt 1: Arm hängen lassen und still halten, bis das Gerät
//      2 s am Stück Ruhe gemessen hat (Wackeln = neu zählen, Anzeige "Ruhig: ..."). Das ist die
//      Referenz (0 Grad). Nach 30 s ohne Erfolg: Abbruch ("Grund: ruhig").
//      Dann vibriert es 300 ms ("fertig, warte") und wartet auf die Bestätigung (höchstens 30 s,
//      sonst "Grund: Bestaetigung").
//      Schritt 2: Arm frei bewegen: beugen, Oberarm bewegen, Hand drehen, in beliebiger Reihenfolge.
//      Jede Sekunde zeigt der Monitor den Fortschritt ("Beugung 41/30 OK  Oberarm 9/15 ...").
//      Sobald alles OK ist: "Referenz gesetzt." und Bestätigungsvibration (150 ms).
//      Nach 30 s ohne Erfolg: Abbruch mit dem ersten fehlenden Grund.
//      Bestätigungen während Schritt 1 und 2 werden ignoriert. Bei jedem Abbruch bleibt die alte Referenz.
//      Nach dem Einschalten gibt es KEINE automatische Einmessung.
//   4. Ist der Arm mehr als GRENZWERT_GRAD über der Referenz -> Motor vibriert.
//   5. Antwortet eine IMU 3 Mal in Folge nicht (REQ-001), gilt sie als ausgefallen:
//      Die Überstreckungs-Warnung ist dann aus, der Motor zeigt ein Fehlermuster
//      (3 kurze Stösse alle 5 Sekunden) und der Serielle Monitor meldet FEHLER.
//      Nach dem Ausfall gibt es keine Warnung, bis die Lage wieder bekannt ist (höchstens 5 s).
//
// Aufbau dieser Datei:
//   1. Einstellungen und Variablen
//   2. setup() und loop(): loop() ruft der Reihe nach kleine Funktionen auf
//   3. SCHRITTE VON loop(): diese kleinen Funktionen
//   4. SENSOR: IMUs auslesen
//   5. EINMESSEN
//   6. AKTOR und BEDIENUNG: Motor, Vibrationsstoss, Taste und Enter
//   7. MELDUNGEN: Zeilen für den Seriellen Monitor (Text-Modus) bzw. für das GUI (GUI-Modus)
//
// Testen ohne Hardware: Wokwi-Simulation, siehe diagram.json im selben Ordner.
//
// Hinweis: F("Text") speichert den Text im Flash statt im knappen RAM.

#include <Wire.h>
#include <math.h>
#include "pins.h"
#include "logik.h"

// ---------- Einstellungen (hier dürft ihr Werte ändern) ----------
// Ausgabe-Modus (REQ-006):
//   1 = GUI-Modus: 115200 Baud, nur kurze Datenzeilen (G, E, D, P, Z) für tools/gui/jointsense_live.html
//   0 = Text-Modus wie früher: 9600 Baud, lesbare Texte für den Seriellen Monitor
// Der Schalter wirkt beim Kompilieren (#if): der nicht gewählte Teil kommt gar nicht ins Programm.
#define GUI_MODUS 1

// MODUS_TEXT("GUI-Text", "Monitor-Text") wählt den passenden Text für den eingestellten Modus.
// So stehen beide Texte direkt nebeneinander, und es braucht weniger #if-Blöcke.
// Beispiel: Serial.println(MODUS_TEXT("E,REF_GESETZT", "Referenz gesetzt."));
// Der nicht gewählte Text kommt gar nicht ins Programm (spart Flash).
#if GUI_MODUS
#define MODUS_TEXT(guiText, monitorText) F(guiText)
#else
#define MODUS_TEXT(guiText, monitorText) F(monitorText)
#endif

const float GRENZWERT_GRAD  = 3.0; // ab so vielen Grad Überstreckung wird gewarnt (QA-03: 0 bis 10)
const float HYSTERESE_GRAD   = 2.0;  // Motor geht erst wieder aus, wenn 2 Grad unter dem Grenzwert
const unsigned long ENTPRELL_MS = 80;  // so lange muss die Überstreckung anliegen (README: 50-100 ms)
const unsigned long MESS_PAUSE_MS = 20; // alle 20 ms messen = 50 Hz (README: 50-100 Hz)
const int MOTOR_STAERKE      = 200;  // PWM-Wert 0 (aus) bis 255 (volle Kraft)
const unsigned long TASTER_PRELL_MS = 50; // so lange "prellt" die Taste nach einer Änderung
const uint8_t FEHLER_SCHWELLE = 3;   // so viele Fehlschläge in Folge = IMU ausgefallen (REQ-001)
const unsigned long FEHLER_MELDUNG_MS = 1000;   // Ausfall wird alle 1000 ms gemeldet (Text-Modus)
const unsigned long REFERENZ_MELDUNG_MS = 1000; // "Keine Referenz" wird alle 1000 ms gemeldet (Text-Modus)
const unsigned long ABWEICHUNG_MELDUNG_MS = 250; // "Abweichung" wird alle 250 ms geschrieben (Text-Modus)
const unsigned long ZYKLUS_MELDUNG_MS = 1000;   // "Zyklus max" wird alle 1000 ms gemeldet (AK-15)
const unsigned long IMU_BEREICH_MS = 1000;      // so oft wird der Messbereich der IMUs neu eingestellt

// Einmessen (README: "Bedienung", REQ-005)
const unsigned long EINMESS_MAX_MS = 30000;     // Schritt 1, Warte-Zustand und Schritt 2 dauern je höchstens 30 s
const unsigned long FORTSCHRITT_MS = 1000;      // so oft: Fortschritt anzeigen und (in Schritt 2) auswerten
const unsigned long NEIGUNG_MAX_ALTER_MS = 100; // die angezeigte Neigung ist höchstens so alt (AK-1)
const unsigned long STOSS_WARTEN_MS = 300;      // Warte-Stoss "fertig, warte" nach Schritt 1 (AK-6)
const unsigned long STOSS_BESTAETIGUNG_MS = 150; // Bestätigungsvibration am Ende
const int SERIELL_MAX_ZEICHEN = 64;             // so viele Zeichen werden pro Durchlauf höchstens gelesen
                                                // (so gross ist der Empfangspuffer des Arduino)

// Die Schritte des Einmessens (Werte für die Variable einmessSchritt)
const uint8_t SCHRITT_AUS    = 0;   // kein Einmessen
const uint8_t SCHRITT_STILL  = 1;   // Schritt 1: Arm still halten
const uint8_t SCHRITT_WARTEN = 2;   // nach Schritt 1: wartet auf die Bestätigung
const uint8_t SCHRITT_FREI   = 3;   // Schritt 2: Arm frei bewegen

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

// Einmessen (blockiert nie, läuft Schritt für Schritt in loop())
uint8_t einmessSchritt = SCHRITT_AUS; // in welchem Schritt sind wir? (SCHRITT_...)
unsigned long schrittStartMs = 0;     // Zeitpunkt, an dem der jetzige Schritt begonnen hat
unsigned long letzterFortschrittMs = 0; // Schritt 1: wann haben wir zuletzt "Ruhig: ..." geschrieben?
unsigned long naechsteAuswertungMs = 0; // Schritt 2: wann kommt die nächste Auswertung (jede Sekunde)?

// Neigung beider IMUs für die Anzeige vor dem Einmessen (AK-1, nur Text-Modus)
float neigungOberarm = 0.0;          // Grad, IMU 1
float neigungUnterarm = 0.0;         // Grad, IMU 2
unsigned long neigungMs = 0;         // Zeitpunkt der Messung
bool neigungDa = false;              // gibt es schon eine Messung?

// Vibrationsstoss (z. B. 300 ms oder 150 ms), ohne Warten
bool stossAktiv = false;             // läuft gerade ein Vibrationsstoss?
unsigned long stossEndeMs = 0;       // wann endet er?

#if GUI_MODUS
// Nur im GUI-Modus: Werte für die D-Zeile (REQ-006)
int16_t rohGelesen[6];               // Rohwerte der zuletzt gelesenen IMU (ax, ay, az, gx, gy, gz)
int16_t roh1[6];                     // Rohwerte IMU 1 in diesem Zyklus
int16_t roh2[6];                     // Rohwerte IMU 2 in diesem Zyklus
bool motorIstAn = false;             // wirklicher Zustand am Motor-Pin (auch Stoss und Fehlermuster)
bool abwDa = false;                  // gibt es in diesem Zyklus eine gültige Abweichung?
long abwZehntel = 0;                 // Abweichung in Zehntelgrad (-1800 bis +1800)
#endif


// Läuft einmal beim Start.
void setup() {
#if GUI_MODUS
  Serial.begin(115200);               // GUI-Modus: schnell, damit 50 D-Zeilen pro Sekunde Platz haben
  Serial.println(F("E,START"));
#else
  Serial.begin(9600);
#endif
  Wire.begin();                      // I2C starten (SDA = A4, SCL = A5)
  Wire.setClock(400000);              // schneller Bus (400 kHz), damit ein Messzyklus kurz bleibt (AK-15)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);   // IN2 bleibt immer LOW
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus

  pinMode(PIN_TASTER, INPUT_PULLUP);  // Taste gegen GND, nicht gedrückt = HIGH

  ellbogenZuruecksetzen(ellbogen);    // keine Referenz, keine Achse, keine Lage

  // Beide IMUs aufwecken und melden, ob sie antworten
  if (imuStarten(IMU1_ADRESSE)) {
    Serial.println(MODUS_TEXT("E,IMU1_OK", "IMU 1 (Oberarm): OK"));
  } else {
    Serial.println(MODUS_TEXT("E,IMU1_FEHLER", "IMU 1 (Oberarm): FEHLER - Verkabelung pruefen"));
  }
  if (imuStarten(IMU2_ADRESSE)) {
    Serial.println(MODUS_TEXT("E,IMU2_OK", "IMU 2 (Unterarm): OK"));
  } else {
    Serial.println(MODUS_TEXT("E,IMU2_FEHLER", "IMU 2 (Unterarm): FEHLER - Verkabelung pruefen"));
  }

#if GUI_MODUS
  // Grenzwert und Hysterese in Zehntelgrad melden (G-Zeile), gerundet
  Serial.print(F("G,"));
  Serial.print((long)(GRENZWERT_GRAD * 10.0f + 0.5f));
  Serial.print(',');
  Serial.println((long)(HYSTERESE_GRAD * 10.0f + 0.5f));
#else
  // Kein automatisches Einmessen: Einmessen nur per Taste (oder Enter).
  Serial.println(F("Zum Einmessen: Arm strecken und Taste kurz druecken."));
  Serial.println(F("(oder Enter im Seriellen Monitor)"));
#endif
}


// Läuft immer wieder, solange der Arduino Strom hat.
// Jede Aufgabe steht in einer eigenen kleinen Funktion (Abschnitt "SCHRITTE VON loop()").
// Die Reihenfolge ist wichtig: Sie bestimmt auch die Reihenfolge der Zeilen auf dem Seriellen Monitor.
void loop() {
  unsigned long jetzt = millis();

  // Bei JEDEM Durchgang (auch zwischen den Messungen):
  stossPruefen(jetzt);               // Vibrationsstoss beenden, wenn seine Zeit um ist
  bestaetigungVerarbeiten(jetzt);    // Taste oder Enter: Einmessen starten oder weiterschalten
  einmessZeitPruefen(jetzt);         // Schritt 1 oder Warten zu lange (30 s)? Dann abbrechen

  // Nur alle MESS_PAUSE_MS messen, sonst gleich wieder von vorne
  if (jetzt - letzteMessung < MESS_PAUSE_MS) {
    return;
  }
  letzteMessung = jetzt;

  imusVorbereiten(jetzt);            // Messbereich auffrischen, ausgefallene IMUs aufwecken

  // 1. Beide IMUs einzeln messen. Eine fehlende IMU bremst nicht (kurzer Fehler, kein Warten).
  unsigned long zyklusStartUs = micros();      // ab hier zählt die Zykluszeit (Lesen und Rechnen)
  ImuMessung messung1;
  ImuMessung messung2;
  bool uebersteuert1 = false;
  bool uebersteuert2 = false;
  bool ok1 = imuLesen(IMU1_ADRESSE, messung1, uebersteuert1);
#if GUI_MODUS
  memcpy(roh1, rohGelesen, sizeof(roh1));      // Rohwerte von IMU 1 für die D-Zeile merken
#endif
  bool ok2 = imuLesen(IMU2_ADRESSE, messung2, uebersteuert2);
#if GUI_MODUS
  memcpy(roh2, rohGelesen, sizeof(roh2));      // Rohwerte von IMU 2
  abwDa = false;                               // Abweichung erst gültig, wenn sie unten berechnet wird
#endif

  // 2. Ausfall-Zustand nachführen (logik.cpp). Beide Aufrufe müssen laufen (darum zuerst aus1 und aus2).
  bool aus1 = imuStatusAktualisieren(imu1, ok1, FEHLER_SCHWELLE);
  bool aus2 = imuStatusAktualisieren(imu2, ok2, FEHLER_SCHWELLE);
  bool ausfall = aus1 || aus2;

  // Fällt eine Messung aus, während eingemessen wird (auch beim Warten): abbrechen (REQ-002 AK-7 c, REQ-004 AK-12)
  if (einmessSchritt != SCHRITT_AUS && (!ok1 || !ok2)) {
    einmessenAbbrechenMelden(EINMESSEN_GRUND_IMU);
  }

  // 3. Winkel nachführen: nur mit zwei guten Messungen, ohne Ausfall und ohne Übersteuerung.
  //    Gyro am Anschlag (z. B. sehr schnelles Drehen): Zyklus überspringen.
  //    Das ist KEIN Ausfall (sonst gäbe schnelles Drehen einen Fehleralarm, AK-14).
  bool winkelAktualisiert = false;
  if (ok1 && ok2 && !ausfall && !uebersteuert1 && !uebersteuert2) {
    winkelAktualisiert = winkelNachfuehren(messung1, messung2);
  }

  // Schritt 2 des Einmessens: einmal pro Sekunde auswerten (Achse, Fortschritt, fertig?). Das dauert
  // länger als ein normaler Zyklus und zählt darum mit in "Zyklus max" (AK-17).
  bool auswertungGemacht = false;
  bool einmessenOk = false;
  if (winkelAktualisiert && einmessSchritt == SCHRITT_FREI && (long)(jetzt - naechsteAuswertungMs) >= 0) {
    naechsteAuswertungMs = naechsteAuswertungMs + FORTSCHRITT_MS;
    auswertungGemacht = true;
    einmessenOk = einmessenAbschliessen(ellbogen);
  }
  unsigned long zyklusDauerUs = micros() - zyklusStartUs;
  if (zyklusDauerUs > zyklusMaxUs) {
    zyklusMaxUs = zyklusDauerUs;
  }

  // Einmessen weiterführen (Texte erst nach der Zeitmessung, damit Serial die Zykluszeit nicht verfälscht)
  einmessenWeiterfuehren(jetzt, winkelAktualisiert, auswertungGemacht, einmessenOk);
#if !GUI_MODUS
  neigungMerken(jetzt, ok1 && ok2, messung1, messung2);   // für die Anzeige vor dem Einmessen (AK-1)
#endif

  // 4. Beginnt oder endet der Ausfall gerade?
  ausfallWechselPruefen(jetzt, ausfall);

  // 5. Motor: Fehlermuster, Einmessen oder normale Warnung
  motorSteuern(jetzt, ausfall, ok1 && ok2);

#if !GUI_MODUS
  // 6. Fehlt die Referenz, regelmässig daran erinnern (nur Text-Modus)
  referenzErinnern(jetzt, ausfall);
#endif

  // 7. Alle 1000 ms die längste Zykluszeit melden (AK-15)
  zyklusMelden(jetzt);

#if GUI_MODUS
  // 8. Eine D-Zeile pro Messzyklus (REQ-006). Erst hier, nach der Zykluszeit-Messung.
  bool uebersteuert = (ok1 && uebersteuert1) || (ok2 && uebersteuert2);
  datenZeileSchreiben(jetzt, ok1, ok2, uebersteuert);
#endif
}


// =====================================================================
// SCHRITTE VON loop() (in der Reihenfolge, in der loop() sie aufruft)
// =====================================================================

// Bestätigung (Taste oder Enter/Leertaste): höchstens eine pro Durchlauf (REQ-004, AK-1 und AK-2).
// Wird in JEDEM Durchlauf gelesen, damit Zeichen während Schritt 1 und 2 verworfen werden.
void bestaetigungVerarbeiten(unsigned long jetzt) {
  if (bestaetigungGekommen(jetzt)) {
    if (einmessSchritt == SCHRITT_AUS) {
      einmessenBeginnen(jetzt);               // Start (AK-2)
    } else if (einmessSchritt == SCHRITT_WARTEN) {
      bewegungBeginnen(jetzt);                // weiter mit Schritt 2 (AK-7)
    }
    // In Schritt 1 oder 2: ignorieren und nicht merken (AK-6)
  }
}

// Zeitgrenzen 30 s für Schritt 1 und das Warten (AK-5, AK-6). Stimmt auch beim Überlauf von millis().
// (Die Zeitgrenze von Schritt 2 prüft einmessenWeiterfuehren nach der Auswertung, AK-13.)
void einmessZeitPruefen(unsigned long jetzt) {
  if ((einmessSchritt == SCHRITT_STILL || einmessSchritt == SCHRITT_WARTEN) &&
      wartezeitAbgelaufen(schrittStartMs, jetzt, EINMESS_MAX_MS)) {
    if (einmessSchritt == SCHRITT_STILL) {
      einmessenAbbrechenMelden(EINMESSEN_GRUND_RUHIG);          // nie 2 s am Stück ruhig
    } else {
      einmessenAbbrechenMelden(EINMESSEN_GRUND_BESTAETIGUNG);   // keine Bestätigung
    }
  }
}

// Bereitet die IMUs auf die Messung vor:
//   - alle 1000 ms den Messbereich neu einstellen (falls eine IMU kurz neu gestartet ist)
//   - ist eine IMU schon ausgefallen, wird sie vor dem Lesen nochmal aufgeweckt:
//     Eine neu eingesteckte IMU startet nämlich im Schlafmodus.
void imusVorbereiten(unsigned long jetzt) {
  if (jetzt - letzterImuBereich >= IMU_BEREICH_MS) {
    letzterImuBereich = jetzt;
    if (!imu1.ausgefallen) {
      imuBereichSetzen(IMU1_ADRESSE);
    }
    if (!imu2.ausgefallen) {
      imuBereichSetzen(IMU2_ADRESSE);
    }
  }
  if (imu1.ausgefallen) {
    imuStarten(IMU1_ADRESSE);
  }
  if (imu2.ausgefallen) {
    imuStarten(IMU2_ADRESSE);
  }
}

// Führt den Ellbogenwinkel mit beiden Messungen nach (logik.cpp). Gibt true zurück, wenn es geklappt hat.
// Beim allerersten Mal gibt es noch keine Zeit seit dem letzten Mal: dann wird nur die Zeit gemerkt.
bool winkelNachfuehren(const ImuMessung &messung1, const ImuMessung &messung2) {
  bool winkelAktualisiert = false;
  unsigned long jetztUs = micros();
  if (hatLetzteAktualisierung) {
    float dt = (float)(jetztUs - letzteAktualisierungUs) * 0.000001f;
    winkelAktualisiert = ellbogenAktualisieren(ellbogen, messung1, messung2, dt);

    if (!winkelAktualisiert && einmessSchritt != SCHRITT_AUS) {
      // Ungültige Werte oder zu lange Pause: Einmessen wäre falsch, also abbrechen (AK-7 c)
      einmessenAbbrechenMelden(EINMESSEN_GRUND_IMU);
    }
  }
  letzteAktualisierungUs = jetztUs;
  hatLetzteAktualisierung = true;
  return winkelAktualisiert;
}

// Führt das Einmessen weiter, nachdem gemessen wurde:
//   - Schritt 1 fertig (2 s ruhig)? Dann Warte-Zustand (AK-6)
//   - Schritt 1: jede Sekunde "Ruhig: x.x s von 2.0 s" (AK-4)
//   - Schritt 2: nach der Auswertung Erfolg (AK-10) oder Fortschritts-Zeile (AK-9)
//   - Schritt 2: nach 30 s Abbruch mit dem ersten fehlenden Grund (AK-13)
void einmessenWeiterfuehren(unsigned long jetzt, bool winkelAktualisiert, bool auswertungGemacht,
                            bool einmessenOk) {
  if (winkelAktualisiert && einmessSchritt == SCHRITT_STILL && einmessenStillFertig(ellbogen)) {
    wartenBeginnen(jetzt);                    // Schritt 1 fertig (AK-6)
  }
  if (einmessSchritt == SCHRITT_STILL && jetzt - letzterFortschrittMs >= FORTSCHRITT_MS) {
    letzterFortschrittMs = jetzt;
    ruhigMelden();                            // "Ruhig: x.x s von 2.0 s" (AK-4)
  }
  if (auswertungGemacht) {
    if (einmessenOk) {
      einmessenFertig(jetzt);                 // Erfolg (AK-10)
    } else {
      fortschrittMelden();                    // Fortschritts-Zeile (AK-9)
    }
  }
  if (einmessSchritt == SCHRITT_FREI && wartezeitAbgelaufen(schrittStartMs, jetzt, EINMESS_MAX_MS)) {
    // 30 s um (AK-13): letzte Fortschritts-Zeile noch einmal, dann Abbruch mit dem ersten fehlenden Grund
    uint8_t grund = einmessenFehlenderGrund(ellbogen);
    fortschrittMelden();
    einmessenAbbrechenMelden(grund);
  }
}

#if !GUI_MODUS
// Nur Text-Modus: merkt die Neigung beider IMUs für die Anzeige vor dem Einmessen (AK-1).
// Nur mit zwei guten Messungen, ohne Referenz und wenn kein Einmessen läuft.
// (Im GUI-Modus gibt es diese Anzeige nicht, das GUI zeigt die Rohwerte.)
void neigungMerken(unsigned long jetzt, bool beideOk, const ImuMessung &messung1,
                   const ImuMessung &messung2) {
  if (beideOk && !referenzGueltig && einmessSchritt == SCHRITT_AUS) {
    neigungOberarm = neigungAusBeschleunigung(messung1.ax, messung1.ay, messung1.az);
    neigungUnterarm = neigungAusBeschleunigung(messung2.ax, messung2.ay, messung2.az);
    neigungMs = jetzt;
    neigungDa = true;
  }
}
#endif

// Prüft, ob ein Ausfall gerade beginnt oder endet, und merkt sich den Zustand für das nächste Mal.
void ausfallWechselPruefen(unsigned long jetzt, bool ausfall) {
  if (ausfall && !ausfallVorher) {
    // Ausfall beginnt: Entprellung und Hysterese frisch starten (REQ-001, AK-9)
    grenzZustandZuruecksetzen();
    stossAktiv = false;               // ein laufender Stoss ist nicht mehr wichtig
    letzteFehlerMeldung = jetzt;
    fehlerMelden();                   // sofort melden
  }
  if (!ausfall && ausfallVorher) {
    // Ausfall ist vorbei: Fehlermuster stoppen, Motor sofort aus, Zustand frisch.
    // Die Lage der IMUs ist jetzt unbekannt (REQ-002, AK-13): keine Warnung, bis sie wieder bekannt ist.
    fehlerMusterMotorAn(fehlerMuster, false, jetzt);
    grenzZustandZuruecksetzen();
    ellbogenLageVerwerfen(ellbogen);
    feedbackAusgeben(false);
    Serial.println(MODUS_TEXT("E,IMU_WIEDER_DA", "IMUs wieder da."));
  }
  ausfallVorher = ausfall;
}

// Entscheidet, was der Motor macht:
//   - bei Ausfall: nur das Fehlermuster (fail-safe, keine Überstreckungs-Warnung)
//   - während dem ganzen Einmessen: keine Warnung (AK-16), Motor nur bei Warte-Stoss und Bestätigung
//   - sonst, mit zwei guten Messungen: normale Warnung (grenzwertAuswerten)
// Sonst ist eine Messung fehlgeschlagen, aber noch unter der Schwelle (1 oder 2 Mal). Dann wird
// dieser Zyklus übersprungen: kein Winkel, kein grenzwertPruefen, der Motor bleibt, wie er war
// (REQ-001, AK-3).
void motorSteuern(unsigned long jetzt, bool ausfall, bool beideOk) {
  if (ausfall) {
    bool musterAn = fehlerMusterMotorAn(fehlerMuster, true, jetzt);
    feedbackAusgeben(motorSollAn(true, false, musterAn));

#if !GUI_MODUS
    // Nur im Text-Modus: FEHLER jede Sekunde wiederholen (das GUI sieht den Ausfall in den D-Zeilen)
    if (jetzt - letzteFehlerMeldung >= FEHLER_MELDUNG_MS) {
      letzteFehlerMeldung = jetzt;
      fehlerMelden();
    }
#endif
  } else if (einmessSchritt != SCHRITT_AUS) {
    grenzZustandZuruecksetzen();
    feedbackAusgeben(false);
  } else if (beideOk) {
    grenzwertAuswerten(jetzt);
  }
}

// Normale Warnung: Grenzwert prüfen (nur wenn Referenz UND Lage bekannt sind) und den Motor schalten.
// Danach die Abweichung für die D-Zeile merken (GUI-Modus) bzw. 4x pro Sekunde schreiben (Text-Modus).
void grenzwertAuswerten(unsigned long jetzt) {
  float abweichung = 0.0;
  bool lageOk = ellbogenAbweichung(ellbogen, abweichung);
  bool zuWeit = false;
  if (referenzGueltig && lageOk) {  // ohne Referenz oder Lage gibt es keine Warnung (AK-9, AK-13)
    zuWeit = grenzwertPruefen(grenzZustand, abweichung, jetzt,
                              GRENZWERT_GRAD, HYSTERESE_GRAD, ENTPRELL_MS);
  } else {
    grenzZustandZuruecksetzen();   // Entprellung frisch starten, wenn die Lage wieder da ist
  }
  feedbackAusgeben(motorSollAn(false, zuWeit, false));

#if GUI_MODUS
  // Abweichung für die D-Zeile merken: nur mit Referenz und Lage (sonst bleibt das Feld leer).
  // Unsinnige Werte (NaN, unendlich) gibt es nicht als Zahl, riesige werden auf +-180 Grad begrenzt.
  if (referenzGueltig && lageOk && !isnan(abweichung) && !isinf(abweichung)) {
    float begrenzt = abweichung;
    if (begrenzt > 180.0f) {
      begrenzt = 180.0f;
    }
    if (begrenzt < -180.0f) {
      begrenzt = -180.0f;
    }
    abwZehntel = zehntelRunden(begrenzt);   // auf 0,1 Grad runden
    abwDa = true;
  }
#else
  // 4x pro Sekunde den aktuellen Wert auf den Seriellen Monitor schreiben
  if (referenzGueltig && lageOk && (jetzt - letzteAusgabe >= ABWEICHUNG_MELDUNG_MS)) {
    letzteAusgabe = jetzt;
    Serial.print(F("Abweichung: "));
    zahlSchreiben(abweichung);
    Serial.print(F(" Grad"));
    if (zuWeit) {
      Serial.print(F("  -> VIBRATION"));
    }
    Serial.println();
  }
#endif
}

#if !GUI_MODUS
// Nur Text-Modus: Fehlt die Referenz, jede Sekunde daran erinnern (auch während eines Ausfalls).
// Das GUI sieht die fehlende Referenz in den Status-Bits der D-Zeile.
void referenzErinnern(unsigned long jetzt, bool ausfall) {
  if (!referenzGueltig && (jetzt - letzteReferenzMeldung >= REFERENZ_MELDUNG_MS)) {
    letzteReferenzMeldung = jetzt;
    Serial.println(F("Keine Referenz - Arm strecken und Taste druecken."));
    // Vor dem Einmessen auch die Neigung beider IMUs zeigen (AK-1), nur ohne Ausfall und mit frischer Messung
    if (!ausfall && einmessSchritt == SCHRITT_AUS && neigungDa && (jetzt - neigungMs) <= NEIGUNG_MAX_ALTER_MS) {
      Serial.print(F("Neigung Oberarm: "));
      zahlSchreiben(neigungOberarm);
      Serial.print(F("  Unterarm: "));
      zahlSchreiben(neigungUnterarm);
      Serial.println(F(" Grad"));
    }
  }
}
#endif

// Meldet alle 1000 ms die längste Zykluszeit seit der letzten Meldung (AK-15) und beginnt neu zu zählen.
void zyklusMelden(unsigned long jetzt) {
  if (jetzt - letzteZyklusMeldung >= ZYKLUS_MELDUNG_MS) {
    letzteZyklusMeldung = jetzt;
#if GUI_MODUS
    Serial.print(F("Z,"));
    Serial.println(zyklusMaxUs);
#else
    Serial.print(F("Zyklus max: "));
    Serial.print(zyklusMaxUs);
    Serial.println(F(" us"));
#endif
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
// Im GUI-Modus stehen die Rohwerte danach zusätzlich in rohGelesen (für die D-Zeile).
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

#if GUI_MODUS
  // Rohwerte für die D-Zeile merken (genau so, wie sie aus der IMU kamen)
  rohGelesen[0] = ax;
  rohGelesen[1] = ay;
  rohGelesen[2] = az;
  rohGelesen[3] = gx;
  rohGelesen[4] = gy;
  rohGelesen[5] = gz;
#endif

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

// Startet das Einmessen: Schritt 1 (still, AK-2).
void einmessenBeginnen(unsigned long jetzt) {
  einmessSchritt = SCHRITT_STILL;
  schrittStartMs = jetzt;                 // ab jetzt zählen die 30 s von Schritt 1 (AK-5)
  letzterFortschrittMs = jetzt;
  einmessenStart(ellbogen);
  grenzZustandZuruecksetzen();
  feedbackAusgeben(false);
  Serial.println(MODUS_TEXT("E,SCHRITT1_START", "Schritt 1: Arm haengen lassen und still halten."));
}

// Schritt 1 ist fertig: Warte-Zustand mit 300 ms Warte-Stoss und GENAU EINER Zeile (AK-6).
void wartenBeginnen(unsigned long jetzt) {
  einmessSchritt = SCHRITT_WARTEN;
  schrittStartMs = jetzt;                 // ab jetzt zählt die Wartezeit (höchstens 30 s)
  stossStarten(jetzt, STOSS_WARTEN_MS);
  Serial.println(MODUS_TEXT("E,SCHRITT1_FERTIG",
                            "Schritt 1 fertig. Enter oder Taste fuer Schritt 2 (frei bewegen)."));
}

// Beginnt Schritt 2 (frei bewegen) nach der Bestätigung im Warte-Zustand. Kein Vibrationsstoss (AK-7).
void bewegungBeginnen(unsigned long jetzt) {
  einmessenBewegungStart(ellbogen);
  einmessSchritt = SCHRITT_FREI;
  schrittStartMs = jetzt;                 // ab jetzt zählen die 30 s von Schritt 2 (AK-13)
  naechsteAuswertungMs = jetzt + FORTSCHRITT_MS;
  Serial.println(MODUS_TEXT("E,SCHRITT2_START",
                            "Schritt 2: Arm frei bewegen (beugen, Oberarm bewegen, Hand drehen)."));
}

// Das Einmessen war erfolgreich (AK-10): Meldung und Bestätigungsvibration.
void einmessenFertig(unsigned long jetzt) {
  einmessSchritt = SCHRITT_AUS;
  grenzZustandZuruecksetzen();
  referenzGueltig = true;
  Serial.println(MODUS_TEXT("E,REF_GESETZT", "Referenz gesetzt."));
  stossStarten(jetzt, STOSS_BESTAETIGUNG_MS);   // Bestätigungsvibration (README: "Bedienung", Punkt 3)
}

// Bricht das Einmessen ab (IMU, Wartezeit, 30 s um) und schreibt den Grund.
// Die bisherige Referenz und Achse bleiben, es gibt keine Bestätigungsvibration (AK-15).
// Motor aus, ein laufender Stoss endet. Danach ist das Gerät sofort wieder bereit für einen neuen Start.
void einmessenAbbrechenMelden(uint8_t grund) {
  einmessenAbbrechen(ellbogen, grund);    // Logik beenden (Referenz und Achse bleiben)
  einmessSchritt = SCHRITT_AUS;
  stossAktiv = false;                     // ein laufender Stoss (z. B. Warte-Stoss) endet sofort
  grenzZustandZuruecksetzen();
  feedbackAusgeben(false);
  einmessenAbbruchMelden(grund);
}


// =====================================================================
// AKTOR und BEDIENUNG
// =====================================================================

// Setzt die Grenzwert-Prüfung zurück: keine Warnung, Entprellung beginnt beim nächsten Mal frisch.
void grenzZustandZuruecksetzen() {
  grenzZustand.warnungAktiv = false;
  grenzZustand.zuWeitSeit = 0;
}

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
#if GUI_MODUS
  motorIstAn = motorAn;                   // für Bit 5 der D-Zeile
#endif
}

// Startet einen Vibrationsstoss von dauerMs Millisekunden (ohne zu warten).
void stossStarten(unsigned long jetzt, unsigned long dauerMs) {
  stossAktiv = true;
  stossEndeMs = jetzt + dauerMs;
  analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
#if GUI_MODUS
  motorIstAn = true;
#endif
}

// Beendet den Vibrationsstoss, wenn seine Zeit um ist.
void stossPruefen(unsigned long jetzt) {
  if (stossAktiv && (long)(jetzt - stossEndeMs) >= 0) {
    stossAktiv = false;
    analogWrite(PIN_MOTOR_PWM, 0);
#if GUI_MODUS
    motorIstAn = false;
#endif
  }
}

// Gibt true zurück, wenn eine Bestätigung gekommen ist (REQ-004, AK-1 und AK-2):
//   (a) die Taste wurde gerade eben neu gedrückt (entprellt in logik.cpp, tasteNeuGedrueckt), oder
//   (b) vom Seriellen Monitor kam ein Leerzeichen, '\n' oder '\r' (istBestaetigungsZeichen).
// Alle Zeichen, die gerade im Empfangspuffer liegen, werden gelesen und verworfen, auch die
// anderen. So zählt ein Durchlauf höchstens EINE Bestätigung (z. B. " \r\n" nur einmal).
// Es wird nie auf Zeichen gewartet und kein Text gespeichert (B-1, AK-15).
bool bestaetigungGekommen(unsigned long jetzt) {
  bool zeichenKam = false;
  int anzahl = Serial.available();        // so viele Zeichen liegen JETZT im Puffer
  if (anzahl > SERIELL_MAX_ZEICHEN) {
    anzahl = SERIELL_MAX_ZEICHEN;         // Sicherheit: nie mehr lesen als der Puffer fassen kann
  }
  for (int i = 0; i < anzahl; i++) {
    int zeichen = Serial.read();          // -1 heisst: doch kein Zeichen da
    if (zeichen < 0) {
      break;
    }
    if (istBestaetigungsZeichen((char)zeichen)) {
      zeichenKam = true;
    }
  }

  // Die Taste wird immer gelesen (auch während einer Mess-Phase), damit ein Druck dort verfällt.
  bool istGedrueckt = (digitalRead(PIN_TASTER) == LOW);   // LOW = gedrückt
  bool tasterNeu = tasteNeuGedrueckt(tasteZustand, istGedrueckt, jetzt, TASTER_PRELL_MS);

  return tasterNeu || zeichenKam;
}


// =====================================================================
// MELDUNGEN (Text-Modus: lesbare Zeilen, GUI-Modus: kurze Zeilen aus REQ-006)
// =====================================================================

// Rundet eine Kommazahl auf 0,1 und gibt sie in Zehnteln zurück (z. B. -12.34 -> -123).
long zehntelRunden(float x) {
  return (long)(x * 10.0f + (x < 0 ? -0.5f : 0.5f));
}

// Macht aus einer Kommazahl eine ganze Zahl (abgerundet).
// Unsinnige Werte (negativ, NaN, riesig) werden zu 0. (Die Werte sind höchstens einige 100 Grad.)
long ganzAbgerundet(float wert) {
  long ganz = 0;
  if (wert > 0.0f && wert < 1000.0f) {    // (NaN ist weder > 0 noch < 1000, gibt also 0)
    ganz = (long)wert;                    // (long) schneidet die Nachkommastellen ab = abrunden
  }
  return ganz;
}

// Schreibt die Zeile zum Abbruch des Einmessens mit dem Grund:
//   GUI-Modus:  "E,EINMESS_ABBRUCH_<GRUND>"
//   Text-Modus: "Einmessen abgebrochen - Grund: ..."
void einmessenAbbruchMelden(uint8_t grund) {
  Serial.print(MODUS_TEXT("E,EINMESS_ABBRUCH_", "Einmessen abgebrochen - Grund: "));
  if (grund == EINMESSEN_GRUND_BEUGUNG) {
    Serial.println(MODUS_TEXT("BEUGUNG", "Beugung"));
  } else if (grund == EINMESSEN_GRUND_RUHIG) {
    Serial.println(MODUS_TEXT("RUHIG", "ruhig"));
  } else if (grund == EINMESSEN_GRUND_OBERARM) {
    Serial.println(MODUS_TEXT("OBERARM", "Oberarm"));
  } else if (grund == EINMESSEN_GRUND_HAND) {
    Serial.println(MODUS_TEXT("HAND", "Hand"));
  } else if (grund == EINMESSEN_GRUND_BESTAETIGUNG) {
    Serial.println(MODUS_TEXT("BESTAETIGUNG", "Bestaetigung"));
  } else {
    Serial.println(F("IMU"));
  }
}

#if GUI_MODUS
// ----- nur GUI-Modus -----

// Schritt 1 schreibt die P-Zeile "P,<ruhig_zehntelsek>,0,0,0,0".
void ruhigMelden() {
  EinmessFortschritt f;
  einmessenFortschritt(ellbogen, f);
  Serial.print(F("P,"));
  Serial.print(ganzAbgerundet(f.ruhigSekunden * 10.0f));   // z. B. 14 = 1,4 s
  Serial.println(F(",0,0,0,0"));                           // in Schritt 1 gibt es noch keine Bewegung
}

// Schritt 2 schreibt die P-Zeile "P,<ruhig>,<beugung>,<oberarm>,<hand>,<achse_prozent>".
void fortschrittMelden() {
  EinmessFortschritt f;
  einmessenFortschritt(ellbogen, f);
  Serial.print(F("P,"));
  Serial.print(ganzAbgerundet(f.ruhigSekunden * 10.0f));
  Serial.print(',');
  Serial.print(ganzAbgerundet(f.beugungGrad));
  Serial.print(',');
  Serial.print(ganzAbgerundet(f.oberarmGrad));
  Serial.print(',');
  Serial.print(ganzAbgerundet(f.handGrad));
  Serial.print(',');
  uint8_t achse = f.achseProzent;
  if (achse > 100) {
    achse = 100;                          // Sicherheit: nie mehr als 100 %
  }
  Serial.println(achse);
}

// Meldet den Beginn eines Ausfalls einmal mit "E,AUSFALL".
// Welche IMU ausgefallen ist, steht in den Status-Bits der D-Zeile.
void fehlerMelden() {
  Serial.println(F("E,AUSFALL"));
}

// Schreibt ein Komma und danach die Zahl (ein Feld der D-Zeile).
void feldSchreiben(long wert) {
  Serial.print(',');
  Serial.print(wert);
}

// Schreibt die 6 Rohwerte einer IMU als Felder. Hat die IMU nicht geantwortet: 6 leere Felder.
void rohFelderSchreiben(bool ok, const int16_t roh[6]) {
  for (uint8_t i = 0; i < 6; i++) {
    if (ok) {
      feldSchreiben(roh[i]);
    } else {
      Serial.print(',');                  // leeres Feld
    }
  }
}

// Schreibt die D-Zeile (REQ-006), einmal pro Messzyklus:
// D,<millis>,<6 Rohwerte IMU 1>,<6 Rohwerte IMU 2>,<abw_zehntel>,<status>,<schritt>
void datenZeileSchreiben(unsigned long jetzt, bool ok1, bool ok2, bool uebersteuert) {
  // Status-Bits zusammenbauen (Bit gesetzt = trifft zu)
  uint8_t status = 0;
  if (imu1.ausgefallen)          status |= 1;    // Bit 0: IMU 1 ausgefallen
  if (imu2.ausgefallen)          status |= 2;    // Bit 1: IMU 2 ausgefallen
  if (referenzGueltig)           status |= 4;    // Bit 2: Referenz gültig
  if (ellbogen.lageBekannt)      status |= 8;    // Bit 3: Lage bekannt
  if (grenzZustand.warnungAktiv) status |= 16;   // Bit 4: zu weit (Ergebnis von grenzwertPruefen)
  if (motorIstAn)                status |= 32;   // Bit 5: Motor an
  if (uebersteuert)              status |= 64;   // Bit 6: Gyro übersteuert

  Serial.print('D');
  Serial.print(',');
  Serial.print(jetzt);                    // millis() ohne Vorzeichen (0 bis 4294967295)
  rohFelderSchreiben(ok1, roh1);
  rohFelderSchreiben(ok2, roh2);
  if (abwDa) {
    feldSchreiben(abwZehntel);
  } else {
    Serial.print(',');                    // leeres Feld: keine gültige Abweichung
  }
  feldSchreiben(status);
  feldSchreiben(einmessSchritt);
  Serial.print('\n');
}

#else
// ----- nur Text-Modus -----

// Schreibt in Schritt 1 die Zeile "Ruhig: x.x s von 2.0 s" (AK-4).
void ruhigMelden() {
  EinmessFortschritt f;
  einmessenFortschritt(ellbogen, f);
  Serial.print(F("Ruhig: "));
  zahlSchreiben(f.ruhigSekunden);
  Serial.println(F(" s von 2.0 s"));
}

// Schreibt in Schritt 2 die Fortschritts-Zeile (AK-9),
// z. B. "Beugung 41/30 OK  Oberarm 9/15  Hand 22/45  Achse 60%".
void fortschrittMelden() {
  EinmessFortschritt f;
  einmessenFortschritt(ellbogen, f);
  wertMelden(F("Beugung "), f.beugungGrad, (int)EINMESSEN_BEUGUNG_MIN);
  wertMelden(F("  Oberarm "), f.oberarmGrad, (int)EINMESSEN_OBERARM_MIN);
  wertMelden(F("  Hand "), f.handGrad, (int)EINMESSEN_HAND_MIN);
  Serial.print(F("  Achse "));
  Serial.print(f.achseProzent);
  Serial.print('%');
  if (f.achseProzent >= 100) {
    Serial.print(F(" OK"));
  }
  Serial.println();
}

// Schreibt "<name><wert>/<ziel>" (Wert ganz, abgerundet) und " OK", wenn der Wert reicht.
// Unsinnige Werte (negativ, NaN, riesig) werden als 0 geschrieben.
void wertMelden(const __FlashStringHelper *name, float wert, int ziel) {
  Serial.print(name);
  Serial.print(ganzAbgerundet(wert));
  Serial.print('/');
  Serial.print(ziel);
  if (wert >= ziel) {
    Serial.print(F(" OK"));
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

// Schreibt eine Zahl mit einer Nachkommastelle auf den Seriellen Monitor (z. B. -12.3).
// Eigene kleine Funktion, weil Serial.print(Kommazahl) viel Platz im Flash braucht.
void zahlSchreiben(float x) {
  long zehntel = zehntelRunden(x);        // auf 0,1 runden
  if (zehntel < 0) {
    Serial.print('-');
    zehntel = -zehntel;
  }
  Serial.print(zehntel / 10);
  Serial.print('.');
  Serial.print(zehntel % 10);
}
#endif
