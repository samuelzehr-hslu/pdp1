// test_hilfe.h  (Prototyp 2 Flex, REQ-008)
// Wie docs/tests/logik/test_hilfe.h, plus ein paar Hilfen für Zeit, Zufall und den Spannungsteiler.
// Eigene Kopie, damit "#include "test_hilfe.h"" in diesem Ordner nichts mit Prototyp 1 vermischt.
//
//   pruefe(bedingung, "Text");                 // ok / FEHLER
//   pruefeNahe(ist, soll, toleranz, "Text");   // Kommazahlen
//   zusatz(bedingung, "Text");                 // Zusatz-Hinweis: kann NIE zu "nicht bestanden" führen
//   return testErgebnis();                     // 0 = alles bestanden

#ifndef TEST_HILFE_FLEX_H
#define TEST_HILFE_FLEX_H

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstdint>

static int anzahlTests = 0;
static int anzahlFehler = 0;

inline void pruefe(bool bedingung, const char* text) {
  anzahlTests++;
  if (bedingung) {
    std::printf("  ok      %s\n", text);
  } else {
    anzahlFehler++;
    std::printf("  FEHLER  %s\n", text);
  }
}

inline void pruefeNahe(double ist, double soll, double toleranz, const char* text) {
  bool nahe = std::fabs(ist - soll) <= toleranz;
  if (!nahe) {
    std::printf("          ist = %f, soll = %f, Toleranz = %f\n", ist, soll, toleranz);
  }
  pruefe(nahe, text);
}

// Zusatz: gehört nicht zu den Akzeptanzkriterien (oder die REQ lässt es offen). Zählt nicht als Fehler.
inline void zusatz(bool bedingung, const char* text) {
  std::printf("  %s  ZUSATZ: %s\n", bedingung ? "ok     " : "hinweis", text);
}

inline int testErgebnis() {
  std::printf("  %d von %d Tests bestanden\n", anzahlTests - anzahlFehler, anzahlTests);
  return anzahlFehler == 0 ? 0 : 1;
}

// ---------- Zufall (mit festem Startwert, damit der Test wiederholbar ist) ----------

// Zufallszahl zwischen von und bis (Kommazahl)
inline double zufall(double von, double bis) {
  return von + (bis - von) * (std::rand() / static_cast<double>(RAND_MAX));
}

// Ganze Zufallszahl zwischen von und bis (beide eingeschlossen)
// (Zwei rand()-Aufrufe zusammen, weil rand() auf Windows nur bis 32767 geht.)
inline int zufallInt(int von, int bis) {
  uint32_t r = (static_cast<uint32_t>(std::rand()) * 32768u) ^ static_cast<uint32_t>(std::rand());
  uint64_t spanne = static_cast<uint64_t>(static_cast<int64_t>(bis) - static_cast<int64_t>(von)) + 1u;
  return static_cast<int>(static_cast<int64_t>(von) + static_cast<int64_t>(r % spanne));
}

// ---------- Spannungsteiler (Physik, aus README: 3V3 - Flex - A0 - 22 kOhm - GND) ----------

// Welchen ADC-Wert (12 Bit) liefert ein Flex mit Widerstand "ohm"? (Umkehrung der Formel aus AK-2)
inline int adcAusOhm(double ohm) {
  return static_cast<int>(std::lround(4095.0 * 22000.0 / (ohm + 22000.0)));
}

#endif
