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
//   Einmessen (REQ-005), ohne feste Zeiten:
//   Schritt 1 (still): Das Gerät wartet, bis beide IMUs 2 s am Stück ruhig sind (Wackeln = neu zählen).
//     Aus genau diesen 2 s kommen die Referenz (0 Grad, Schwerkraft) und der Gyro-Fehler.
//   Warte-Zustand: bis die Person bestätigt. Die Logik sammelt nichts, führt aber die Lage beider IMUs
//     weiter (Gyro, von der Schwerkraft gestützt).
//   Schritt 2 (frei bewegen): beugen, Oberarm bewegen, Hand drehen, in beliebiger Reihenfolge und beliebig oft.
//     Das Gerät ordnet jede Messung ein:
//       - Handdrehung (Unterarm dreht um seine Knochenachse, Oberarm ruhig): zählt nur für die Knochenachse
//         und den Hand-Wert. Grund: Die Suche der Scharnier-Achse erlaubt nur EINE Drehachse.
//       - sonst (Beugen, Oberarm bewegen, Pausen): zählt für die Suche der Scharnier-Achse.
//     Nur zum EINORDNEN nimmt das Gerät an, dass der Unterarm in Schritt 1 ungefähr senkrecht hing
//     (höchstens 30 Grad schief). Die Achsen selbst (Scharnier-Achse und Knochenachse) werden nur aus der
//     echten Bewegung gerechnet, dafür wird NICHT angenommen, dass der Arm senkrecht hängt.
//   Jede Sekunde prüft einmessenAbschliessen, ob alles reicht. Ist die Achse nicht eindeutig, sammelt das
//   Gerät weiter (oder jointsense.ino bricht nach 30 s ab). Das Gerät rät nie.
//   Die Lage beider IMUs läuft von Schritt 1 bis zum Abschluss durch. Der Arm muss bei der Bestätigung
//   nicht wieder in der Referenz-Lage hängen.
//
//   So ruft jointsense.ino die Funktionen auf:
//     einmessenStart          -> Schritt 1 (still)
//     einmessenStillFertig    -> true = 2 s ruhig, die Logik ist jetzt im Warte-Zustand
//     einmessenBewegungStart  -> Schritt 2 (frei bewegen), nach der Bestätigung
//     einmessenAbschliessen   -> einmal pro Sekunde: true = fertig, Referenz und Achse sind gesetzt
//     einmessenFortschritt    -> Zahlen für die Anzeige (Ruhezeit, Beugung, Oberarm, Hand, Achse)
//     einmessenAbbrechen      -> Abbruch (Wartezeit, IMU, 30 s um), Referenz und Achse bleiben

// Gründe, warum ein Einmessen abgebrochen wurde (für die Meldung im Seriellen Monitor).
#define EINMESSEN_GRUND_KEINER  0   // kein Abbruch
#define EINMESSEN_GRUND_BEUGUNG 1   // Beugung in Schritt 2 zu klein
#define EINMESSEN_GRUND_RUHIG   2   // Arm war in Schritt 1 nicht 2 s am Stück ruhig (30 s um)
#define EINMESSEN_GRUND_IMU     3   // IMU ausgefallen oder ungültige Werte
#define EINMESSEN_GRUND_OBERARM 4   // Scharnier-Achse nicht eindeutig (z. B. Oberarm still)
#define EINMESSEN_GRUND_HAND    5   // Knochenachse des Unterarms nicht sicher bestimmt (Hand nicht gedreht)
#define EINMESSEN_GRUND_BESTAETIGUNG 6  // Wartezeit abgelaufen, keine Bestätigung (REQ-004).
                                        // Die Logik setzt diesen Grund nie selbst: jointsense.ino gibt
                                        // ihn beim Abbruch an einmessenAbbrechen() mit.

// Ab diesen Werten ist der Fortschritt in Schritt 2 "OK" (AK-8, gleiche Grenzen wie REQ-002).
#define EINMESSEN_BEUGUNG_MIN 30.0f  // Grad: Spannweite der Beugung
#define EINMESSEN_OBERARM_MIN 15.0f  // Grad: Spannweite der Oberarm-Neigung (in beiden Richtungen)
#define EINMESSEN_HAND_MIN    45.0f  // Grad: grösste Drehung der Hand

// So viele Lagen werden in Schritt 2 gemerkt: 2 Extreme der Beugung, 2 Extreme der vorläufigen Beugung
// und 12 neue (10 pro s, jede Sekunde ausgewertet).
#define ELLBOGEN_POSEN 16

// Fortschritt beim Einmessen (für die Anzeige im Seriellen Monitor, REQ-005 AK-4 und AK-8).
struct EinmessFortschritt {
  float ruhigSekunden;  // Schritt 1: Ruhezeit am Stück in s
  float beugungGrad;    // Schritt 2: Spannweite der Beugung in Grad (AK-8)
  float oberarmGrad;    // Schritt 2: kleinere Abweichung des Oberarms in Grad (AK-8)
  float handGrad;       // Schritt 2: grösste Drehung der Hand in Grad (AK-8)
  uint8_t achseProzent; // Schritt 2: 0 bis 100, 100 = Achse eindeutig (AK-8)
};

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
  float ruheZeit;           // zurzeit nicht benutzt (wird nur auf 0 gesetzt), bleibt wegen der Schnittstelle
  float lageZeit;           // wie lange (s) ist die Lage schon unbekannt?
  float lageLetzter;        // zurzeit nicht benutzt (wird nur auf 0 gesetzt), bleibt wegen der Schnittstelle
  float drehungLetzte;      // Verdrehung des Unterarms um seine Achse vor dem Ausfall (Grad)

  // ----- Einmessen (Sammelfächer, nur während dem Einmessen gebraucht) -----
  uint8_t phase;            // 0 = kein Einmessen, 1 = Schritt 1 (still), 2 = Warte-Zustand,
                            // 3 = Schritt 2 (frei bewegen)
  uint8_t grund;            // Abbruchgrund (EINMESSEN_GRUND_...)
  bool ratenGueltig2;       // true = die Geschwindigkeiten vom letzten Zyklus (für lage) sind da
  bool beugGueltig;         // true = posen Platz 0 und 1 enthalten die Lagen mit kleinstem und grösstem Winkel
  uint8_t posenAnzahl;      // nächster freier Platz in posen (neue Lagen ab Platz 2)
  uint8_t zyklen;           // zählt die Messungen bis zur nächsten gemerkten Lage
  uint8_t achseProzent;     // Schritt 2: Achse in Prozent (100 = eindeutig)
  uint16_t anzahl1;         // Anzahl ruhiger Messungen in Schritt 1
  uint16_t anzahlW;         // davon: Anzahl, die zum Gyro-Mittelwert zählen
  uint16_t anzahlG;         // Anzahl Messungen für die Achsen-Suche in Schritt 2
  float ruhigZeit;          // Schritt 1: Ruhezeit am Stück in s
  float summeA1[3];         // Schritt 1: Summe Beschleunigung (danach: Schwerkraft-Richtung der Referenz, Länge 1)
  float summeA2[3];
  float summeW1[3];         // laufender Mittelwert Gyro (= Gyro-Fehler), danach langsam nachgeführt
  float summeW2[3];
  float lage[2][4];         // Drehung von IMU 1 und IMU 2 (Quaternion) seit der Referenz-Lage (Schritt 1)
  float rateP[2][3];        // letzte Drehgeschwindigkeit von IMU 1 und IMU 2 (rad/s)
  float matrixM[21];        // Schritt 2: Sammelmatrix für die Scharnier-Achsen (6x6, obere Hälfte)
  float eigenV[6][6];       // Schritt 2: Eigenvektoren der letzten Sekunde (dort rechnet die Achsen-Suche weiter)
  float dreh[8];            // Schritt 2: [0..2] Summe der Beuge-Drehachse (vorläufige Beugung),
                            // Handdrehung: [3..5] Summe der Drehachse, [6] Drehung mit Vorzeichen (rad), [7] Weg (rad)
  float theta[3];           // Schritt 2, vorläufige Beugung: [0] Winkel, [1] kleinster, [2] grösster (rad)
  float oberMin[2];         // Schritt 2: kleinste Neigung des Oberarms je Richtung (Sinus)
  float oberMax[2];         // Schritt 2: grösste Neigung des Oberarms je Richtung (Sinus)
  float beugMin;            // Schritt 2: kleinster Winkel (Grad, mit der neuesten Achse)
  float beugMax;            // Schritt 2: grösster Winkel
  float beugBest;           // Schritt 2: grösste Spannweite der Beugung bisher (wird nie kleiner)
  float handMax;            // Schritt 2: grösste Drehung der Hand in Grad (wird nie kleiner)
  int16_t posen[ELLBOGEN_POSEN][8];  // Schritt 2: gemerkte Lagen beider IMUs (je 4 Zahlen mal 16000)
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

// Beginnt Schritt 1 (still). Ruhezeit 0, Sammelfächer leer.
// Referenz und Achse von vorher bleiben, bis ein Einmessen erfolgreich ist.
void einmessenStart(EllbogenZustand &z);

// true, sobald in Schritt 1 2000 ms am Stück ruhig waren (AK-3). Die Logik ist dann im Warte-Zustand
// und sammelt nichts mehr für Schritt 1.
bool einmessenStillFertig(const EllbogenZustand &z);

// Beginnt Schritt 2 (frei bewegen), nach der Bestätigung im Warte-Zustand. Merkt die Ausgangslage.
void einmessenBewegungStart(EllbogenZustand &z);

// Füllt f mit dem Fortschritt (AK-4 und AK-8). Nicht benutzte Werte sind 0. Rechnet fast nichts.
void einmessenFortschritt(const EllbogenZustand &z, EinmessFortschritt &f);

// Versucht abzuschliessen (AK-10). Höchstens einmal pro 1000 ms aufrufen: rechnet die Achse
// und den Fortschritt neu (dauert auf dem Uno einige 10 ms, AK-17).
// true  = alle vier Werte OK, neue Referenz und Achse gesetzt, Einmessen beendet.
// false = noch nicht genug, Schritt 2 läuft weiter (Referenz und Achse unverändert).
bool einmessenAbschliessen(EllbogenZustand &z);

// Bricht ein laufendes Einmessen ab (Wartezeit, IMU, 30 s um). grund: EINMESSEN_GRUND_...
// Referenz und Achse bleiben wie vorher.
void einmessenAbbrechen(EllbogenZustand &z, uint8_t grund);

// Für AK-13: der erste nicht erfüllte Wert (Beugung, Oberarm, Hand; Achse unter 100 % = Oberarm).
// EINMESSEN_GRUND_KEINER, wenn alles erfüllt ist.
uint8_t einmessenFehlenderGrund(const EllbogenZustand &z);

// Sagt, warum das letzte Einmessen abgebrochen wurde (EINMESSEN_GRUND_...).
// jointsense.ino braucht diese Funktion zurzeit nicht (sie kennt den Grund schon beim Abbruch).
uint8_t einmessenGrund(const EllbogenZustand &z);

// true, wenn Referenz, Achse UND Lage bekannt sind.
//   abweichungGrad = Ellbogenwinkel minus Referenz, positiv = Überstreckung, negativ = Beugung.
// Sonst false (kein Alarm erlaubt) und abweichungGrad bleibt unverändert.
bool ellbogenAbweichung(const EllbogenZustand &z, float &abweichungGrad);

// Wird nach einem IMU-Ausfall aufgerufen: Referenz, Achse und Gyro-Fehler bleiben,
// die Lage gilt als unbekannt (höchstens 5 s nach der Wiederkehr wieder bekannt).
void ellbogenLageVerwerfen(EllbogenZustand &z);

// ---------------------------------------------------------------------
// Bestätigung beim Einmessen (REQ-004)
// ---------------------------------------------------------------------

// true für ein Zeichen vom Seriellen Monitor, das als Bestätigung zählt:
// Leerzeichen ' ', Zeilenende '\n' oder '\r'. Alle anderen Zeichen: false.
bool istBestaetigungsZeichen(char zeichen);

// true, wenn seit wartenSeitMs mindestens maxMs Millisekunden vergangen sind.
// Stimmt auch, wenn millis() dazwischen überläuft (nach ca. 49 Tagen wieder bei 0).
bool wartezeitAbgelaufen(unsigned long wartenSeitMs, unsigned long jetztMs, unsigned long maxMs);

#endif
