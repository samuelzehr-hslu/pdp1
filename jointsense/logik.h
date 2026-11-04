// logik.h
// Die reine Logik von JointSense: Winkel berechnen, Grenzwert prüfen, Taste entprellen.
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

#endif
