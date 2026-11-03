// test_hilfe.h
// Ganz einfaches Test-Werkzeug, ohne fremde Bibliothek.
//
// So benutzt man es in einer Testdatei (z. B. test_REQ-001.cpp):
//
//   #include "test_hilfe.h"
//   #include "logik.h"          // die echte Logik aus jointsense/
//
//   int main() {
//     pruefe(1 + 1 == 2, "AK-1: Eins plus eins ist zwei");
//     pruefeNahe(0.5, 0.5001, 0.01, "AK-2: Wert stimmt auf 0.01 genau");
//     return testErgebnis();    // 0 = alles bestanden, 1 = mindestens ein Fehler
//   }
//
// Jede Testdatei hat ihr eigenes main(). Gestartet werden alle Tests mit:
//   python docs/tests/alle_tests.py

#ifndef TEST_HILFE_H
#define TEST_HILFE_H

#include <cstdio>
#include <cmath>

static int anzahlTests = 0;
static int anzahlFehler = 0;

// Prüft eine Bedingung. Ist sie falsch, wird der Text als FEHLER ausgegeben.
inline void pruefe(bool bedingung, const char* text) {
  anzahlTests++;
  if (bedingung) {
    std::printf("  ok      %s\n", text);
  } else {
    anzahlFehler++;
    std::printf("  FEHLER  %s\n", text);
  }
}

// Prüft, ob zwei Kommazahlen fast gleich sind (Abstand höchstens "toleranz").
inline void pruefeNahe(double ist, double soll, double toleranz, const char* text) {
  bool nahe = std::fabs(ist - soll) <= toleranz;
  if (!nahe) {
    std::printf("          ist = %f, soll = %f, Toleranz = %f\n", ist, soll, toleranz);
  }
  pruefe(nahe, text);
}

// Am Ende von main() aufrufen: gibt die Zusammenfassung aus.
inline int testErgebnis() {
  std::printf("  %d von %d Tests bestanden\n", anzahlTests - anzahlFehler, anzahlTests);
  return anzahlFehler == 0 ? 0 : 1;
}

#endif
