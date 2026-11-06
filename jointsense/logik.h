// logik.h
// Die reine Logik von JointSense: Winkel berechnen, Grenzwert prüfen, Taste entprellen,
// Ellbogenwinkel um die Scharnier-Achse (Gyro + Beschleunigung).
//
// Wichtig: Diese Datei (und logik.cpp) benutzen KEINE Arduino-Befehle
// (kein millis(), kein Serial, kein Wire, kein digitalRead).
// Die Zeit und die Messwerte bekommen die Funktionen als Parameter.
// Darum kann man diese Logik auch auf dem PC testen, z. B. mit:
//   g++ -std=c++11 -Wall -Wextra -I jointsense jointsense/logik.cpp meinTest.cpp
//
// Die Einstellungen (Grenzwert, Hysterese, Entprellzeit ...) stehen weiterhin
// in jointsense.ino und werden beim Aufruf mitgegeben.

#ifndef LOGIK_H
#define LOGIK_H

#include <stdint.h>   // für uint32_t (32-Bit-Zahl ohne Vorzeichen, gleich wie millis())


// ---------------------------------------------------------------------
// Zustand der Grenzwert-Prüfung
// ---------------------------------------------------------------------
// Diese zwei Werte muss sich die Grenzwert-Prüfung von Messung zu Messung merken.
// Anlegen so:   GrenzwertZustand grenzZustand = {false, 0};
struct GrenzwertZustand {
  bool warnungAktiv;     // true = Motor soll gerade vibrieren
  uint32_t zuWeitSeit;   // seit wann (in ms) ist der Arm zu weit? 0 = gerade nicht zu weit
};

// ---------------------------------------------------------------------
// Zustand der Taste
// ---------------------------------------------------------------------
// Anlegen so:   TasteZustand tasteZustand = {false, 0};
struct TasteZustand {
  bool warGedrueckt;         // war die Taste beim letzten Mal gedrückt?
  uint32_t letzteAenderung;  // wann (in ms) hat sich der Zustand zuletzt geändert?
};


// ---------------------------------------------------------------------
// Zustand einer IMU (für die Ausfall-Erkennung)
// ---------------------------------------------------------------------
// Anlegen so:   ImuStatus imu1 = {0, false};
struct ImuStatus {
  uint8_t fehlerInFolge;   // wie viele Messungen in Folge sind fehlgeschlagen?
  bool ausgefallen;        // true = IMU gilt als ausgefallen
};

// ---------------------------------------------------------------------
// Zustand des Fehlermusters (Vibration bei Ausfall)
// ---------------------------------------------------------------------
// Anlegen so:   FehlerMusterZustand fehlerMuster = {false, 0};
struct FehlerMusterZustand {
  bool aktiv;              // läuft das Muster gerade (Ausfall dauert an)?
  uint32_t startMs;        // Zeitpunkt (ms), an dem der Ausfall begann
};

// ---------------------------------------------------------------------
// Winkelmessung unabhängig von Drehung (REQ-002, Runde 2)
// ---------------------------------------------------------------------
// Idee (einfach erklärt):
//   Das Gerät merkt sich laufend, wie die Unterarm-IMU zur Oberarm-IMU gedreht ist
//   (eine "relative Drehung", gespeichert als Quaternion q mit 4 Zahlen).
//   Jeder Messzyklus dreht q mit den beiden Gyroskopen weiter (Oberarm und Unterarm
//   zählen gleich viel). Die Schwerkraft (Beschleunigungsmesser) zieht q langsam zurecht,
//   damit nichts wegläuft (Drift).
//   Der Ellbogenwinkel ist dann der Winkel, um den sich die Knochenachse des Unterarms
//   um die Scharnier-Achse des Ellbogens gedreht hat. Drehen von Hand, Schulter oder
//   Schwenken ändern diesen Winkel nicht (auch nicht, wenn man gleichzeitig beugt).
//   Einmessen: Phase 1 (still) misst Referenz und Gyro-Fehler. In Phase 2 (beugen und
//   Oberarm mitbewegen) sucht das Gerät die Scharnier-Achse in beiden IMUs.
//   Ist die Achse nicht eindeutig, bricht es ab ("Oberarm"), es rät nie.
//   Phase 3 (Hand drehen): Die Drehung des Unterarms um seine Knochenachse wird gemessen.
//   So weiss das Gerät die Knochenachse, ohne anzunehmen, dass der Arm senkrecht hängt.

// Gründe, warum ein Einmessen abgebrochen wurde (für die Meldung im Seriellen Monitor).
#define EINMESSEN_GRUND_KEINER  0   // kein Abbruch
#define EINMESSEN_GRUND_BEUGUNG 1   // Beugung in Phase 2 zu klein
#define EINMESSEN_GRUND_RUHIG   2   // Arm war in Phase 1 nicht ruhig
#define EINMESSEN_GRUND_IMU     3   // IMU ausgefallen oder ungültige Werte
#define EINMESSEN_GRUND_OBERARM 4   // Scharnier-Achse nicht eindeutig (z. B. Oberarm still)
#define EINMESSEN_GRUND_HAND    5   // Knochenachse des Unterarms nicht sicher bestimmt (Hand nicht gedreht)

// Die Anzahl Zwischenwerte (alle 0,25 s), die beim Einmessen für die Beuge-Prüfung gemerkt werden.
#define ELLBOGEN_ZWISCHENWERTE 17

// Eine Messung einer IMU, in den Achsen des Sensors (so wie der MPU6050 sie liefert).
struct ImuMessung {
  float ax, ay, az;   // Beschleunigung in g (Erdbeschleunigung = 1 g)
  float gx, gy, gz;   // Drehgeschwindigkeit in Grad pro Sekunde
};

// Alles, was sich die Winkelmessung merken muss.
// Anlegen so:   EllbogenZustand ellbogen;  ellbogenZuruecksetzen(ellbogen);
// Die Felder nicht von Hand ändern, nur über die Funktionen unten.
struct EllbogenZustand {
  // ----- Laufende Messung -----
  bool achseBekannt;        // true = Referenz und Scharnier-Achse sind eingemessen
  bool lageBekannt;         // true = der aktuelle Winkel ist bekannt
  bool ratenGueltig;        // true = die Drehgeschwindigkeiten vom letzten Zyklus sind gespeichert
  uint8_t lageAnzahl;       // wie viele stimmige Schwerkraft-Werte in Folge beim Wiederfinden?
  float q[4];               // relative Drehung Unterarm-IMU -> Oberarm-IMU (Quaternion, Länge 1)
  float qRef[4];            // dieselbe Drehung in der Referenz-Lage
  float achse1[3];          // Scharnier-Achse in den Achsen von IMU 1 (Länge 1)
  float knochen2[3];        // Knochenachse des Unterarms in den Achsen von IMU 2 (Länge 1)
  float knochen0[3];        // Knochenachse des Unterarms in der Referenz-Lage, in den Achsen von IMU 1
  float bias1[3];           // Gyro-Fehler (Offset) von IMU 1 in Grad pro Sekunde
  float bias2[3];           // Gyro-Fehler (Offset) von IMU 2
  float rate1[3];           // letzte Drehgeschwindigkeit IMU 1 (ohne Fehler, rad/s)
  float rate2[3];           // letzte Drehgeschwindigkeit IMU 2
  float winkel;             // Ellbogenwinkel minus Referenz in Grad (positiv = Überstreckung)
  float ruheZeit;           // wie lange (s) ruhen beide IMUs schon?
  float lageZeit;           // wie lange (s) ist die Lage schon unbekannt?
  float lageLetzter;        // letzter Winkel aus der Schwerkraft (zum Wiederfinden der Lage)
  float drehungLetzte;      // Verdrehung des Unterarms um seine Achse vor dem Ausfall (Grad)

  // ----- Einmessen (Sammelfächer, nur während dem Einmessen gebraucht) -----
  uint8_t phase;            // 0 = kein Einmessen, 1 = Phase Still, 2 = Phase Bewegung
  uint8_t grund;            // Abbruchgrund (EINMESSEN_GRUND_...)
  uint8_t zwischenAnzahl;   // Anzahl gespeicherter Zwischenwerte
  bool ratenGueltig2;       // true = die Geschwindigkeiten vom letzten Zyklus von Phase 2 sind da
  uint16_t anzahl1;         // Anzahl Messungen in Phase 1
  uint16_t anzahlW;         // Anzahl Messungen in Phase 1, die zum Gyro-Mittelwert zählen
  uint16_t anzahlG;         // Anzahl brauchbarer Messungen in Phase 2
  float summeA1[3];         // Phase 1: Summe Beschleunigung (danach: Schwerkraft-Richtung, Länge 1)
  float summeA2[3];
  float summeW1[3];         // Phase 1: laufender Mittelwert Gyro (= Gyro-Fehler)
  float summeW2[3];
  float summeBetrag1;       // Phase 1: Summe vom Betrag der Drehgeschwindigkeit
  float summeBetrag2;
  float lage[2][4];         // Phase 2: Drehung von IMU 1 und IMU 2 seit Beginn von Phase 2 (Quaternion)
  float rateP[2][3];        // Phase 2: letzte Drehgeschwindigkeit von IMU 1 und IMU 2 (rad/s)
  float matrixM[21];        // Phase 2: Sammelmatrix für die Scharnier-Achsen (6x6, obere Hälfte)
  float dreh[8];            // Phase 3: Richtung, Summe der Drehachse von IMU 2, Weg von IMU 1 und IMU 2 (rad)
  float zeit2;              // Phase 2: vergangene Zeit in s
  float kippGlatt[3];       // Phase 2: geglättete Schwerkraft-Richtung von IMU 1
  float kippCosMin;         // Phase 2: kleinster Cosinus der Neigung des Oberarms (1 = keine Neigung)
  int16_t zwischen[ELLBOGEN_ZWISCHENWERTE][8];  // Zwischenwerte der beiden Drehungen (mal 16000)
};


// ---------------------------------------------------------------------
// Funktionen
// ---------------------------------------------------------------------

// Rechnet die Neigung einer IMU in Grad aus den drei Beschleunigungswerten (x, y, z).
float neigungAusBeschleunigung(float x, float y, float z);

// Ellbogenwinkel = Neigung Unterarm - Neigung Oberarm (in Grad).
float ellbogenWinkel(float neigungOberarm, float neigungUnterarm);

// Referenz beim Einmessen = Mittelwert = summe / anzahl.
// Bei anzahl 0 oder kleiner wird 0 zurückgegeben (statt durch 0 zu teilen).
float referenzBerechnen(float summe, int anzahl);

// Prüft, ob der Arm zu weit gestreckt ist (mit Hysterese und Entprellung).
//   zustand        : wird von der Funktion gelesen UND aktualisiert
//   abweichungGrad : wie viele Grad der Arm über der Referenz ist
//   jetztMs        : aktuelle Zeit in ms (auf dem Arduino: millis())
//   grenzwertGrad  : ab so vielen Grad wird gewarnt
//   hystereseGrad  : so viele Grad muss der Arm zurück, bis die Warnung aufhört
//   entprellMs     : so lange muss der Arm zu weit sein, bis gewarnt wird
// Rückgabe: true = Motor soll vibrieren.
// Sicherheit: Ist die Abweichung keine gültige Zahl (NaN oder unendlich),
// gibt die Funktion false zurück (Motor aus) und setzt den Zustand zurück.
bool grenzwertPruefen(GrenzwertZustand &zustand, float abweichungGrad, uint32_t jetztMs,
                      float grenzwertGrad, float hystereseGrad, uint32_t entprellMs);

// Gibt true zurück, wenn die Taste gerade eben neu gedrückt wurde (nur einmal pro Druck).
//   zustand     : wird von der Funktion gelesen UND aktualisiert
//   istGedrueckt: ist die Taste jetzt gerade gedrückt?
//   jetztMs     : aktuelle Zeit in ms (auf dem Arduino: millis())
//   prellMs     : so lange nach einer Änderung werden weitere Änderungen ignoriert
bool tasteNeuGedrueckt(TasteZustand &zustand, bool istGedrueckt, uint32_t jetztMs,
                       uint32_t prellMs);

// Merkt sich, ob eine IMU ausgefallen ist (REQ-001).
//   status        : wird gelesen UND aktualisiert
//   messungOk     : war die Messung erfolgreich?
//   schwelleFehler: ab so vielen Fehlschlägen in Folge gilt die IMU als ausgefallen
//                   (im Betrieb: 3; eine Schwelle von 0 wird wie 1 behandelt)
// Eine erfolgreiche Messung setzt alles zurück. Der Zähler bleibt bei 255 stehen.
// Rückgabe: true = die IMU gilt nach diesem Update als ausgefallen.
bool imuStatusAktualisieren(ImuStatus &status, bool messungOk, uint8_t schwelleFehler);

// Fehlermuster bei Ausfall: 3 kurze Stösse (100 ms an, 100 ms aus, ...), Start alle 5000 ms.
//   zustand: wird gelesen UND aktualisiert
//   ausfall: gilt gerade irgendeine IMU als ausgefallen?
//   jetztMs: aktuelle Zeit in ms
// Rückgabe: true = der Motor soll jetzt wegen des Fehlermusters vibrieren.
// Ist ausfall false, wird der Zustand zurückgesetzt und false zurückgegeben.
bool fehlerMusterMotorAn(FehlerMusterZustand &zustand, bool ausfall, uint32_t jetztMs);

// Entscheidet, ob der Motor vibrieren soll.
// Bei Ausfall zählt NUR das Fehlermuster (Ausfall hat Vorrang vor Überstreckung).
// Ohne Ausfall zählt zuWeit (Ergebnis von grenzwertPruefen).
bool motorSollAn(bool sensorAusfall, bool zuWeit, bool fehlerMusterAn);

// ---------------------------------------------------------------------
// Funktionen für die Winkelmessung (REQ-002)
// ---------------------------------------------------------------------

// Löscht alles: keine Referenz, keine Achse, keine Lage (wie nach dem Einschalten).
void ellbogenZuruecksetzen(EllbogenZustand &z);

// Verarbeitet EINEN Messzyklus (Winkel nachführen, Drift korrigieren, beim Einmessen sammeln).
//   oberarm / unterarm : Messung von IMU 1 / IMU 2
//   dtSekunden         : Zeit seit dem letzten Aufruf in Sekunden
// Rückgabe: true = Zyklus verwendet.
// false und z UNVERÄNDERT, wenn ein Wert NaN oder unendlich ist, dtSekunden <= 0 oder > 0,5 ist,
// eine Beschleunigung grösser als 16 g oder eine Drehgeschwindigkeit grösser als 2000 Grad/s ist.
bool ellbogenAktualisieren(EllbogenZustand &z, const ImuMessung &oberarm,
                           const ImuMessung &unterarm, float dtSekunden);

// Beginnt Phase 1 (Still). Referenz und Achse von vorher bleiben gültig.
void einmessenStart(EllbogenZustand &z);

// Beginnt Phase 2 (Bewegung), nach 2000 ms Phase 1.
void einmessenBewegungStart(EllbogenZustand &z);

// Beginnt Phase 3 (Hand drehen), nach 6000 ms. Danach folgt nach 1500 ms einmessenAbschliessen.
void einmessenDrehungStart(EllbogenZustand &z);

// Schliesst das Einmessen ab. minBeugungGrad: so gross muss die Beugung (Spannweite) sein.
// true  = neue Referenz und Achse gesetzt, die Lage ist bekannt.
// false = Abbruch (Beugung zu klein oder Arm nicht ruhig): alte Referenz und Achse bleiben.
bool einmessenAbschliessen(EllbogenZustand &z, float minBeugungGrad);

// Bricht ein laufendes Einmessen ab (z. B. IMU ausgefallen). Referenz und Achse bleiben.
void einmessenAbbrechen(EllbogenZustand &z);

// Sagt, warum das letzte Einmessen abgebrochen wurde (EINMESSEN_GRUND_...).
// Nur für die Meldung im Seriellen Monitor.
uint8_t einmessenGrund(const EllbogenZustand &z);

// true, wenn Referenz, Achse UND Lage bekannt sind.
//   abweichungGrad = Ellbogenwinkel minus Referenz, positiv = Überstreckung, negativ = Beugung.
// Sonst false (kein Alarm erlaubt) und abweichungGrad bleibt unverändert.
bool ellbogenAbweichung(const EllbogenZustand &z, float &abweichungGrad);

// Wird nach einem IMU-Ausfall aufgerufen: Referenz, Achse und Gyro-Fehler bleiben,
// die Lage gilt als unbekannt (höchstens 5 s nach der Wiederkehr wieder bekannt).
void ellbogenLageVerwerfen(EllbogenZustand &z);

#endif
