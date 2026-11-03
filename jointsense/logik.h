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

#endif
