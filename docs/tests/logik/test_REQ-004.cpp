// test_REQ-004.cpp
// Tests zu REQ-004 "Jeden Einmess-Schritt bestätigen" (reine Logik).
// Stand REQ-005: nur noch AK-1 (Zeichen) und AK-9 (Wartezeit); der Rest ist ersetzt durch REQ-005 (siehe unten).
//
// Was hier noch geprüft wird (Logik auf dem PC):
//   AK-1  : istBestaetigungsZeichen (nur Leerzeichen, Zeilenumbruch, Wagenruecklauf), auch alle anderen 253 Zeichen
//   AK-9  : wartezeitAbgelaufen (30 000 ms, auch beim Überlauf von millis())
//   Grund-Nummer EINMESSEN_GRUND_BESTAETIGUNG
// Der Rest (AK-2 bis AK-4, AK-8, AK-10, AK-14, AK-15, Meldungstexte) steckt in jointsense.ino: Board-Test, siehe
// docs/tests/board-test-REQ-004.md und docs/tests/board-test-REQ-005.md.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "test_hilfe.h"
#include "logik.h"


// "Zusatz": Annahme des Testers, die nicht wörtlich in der REQ steht. Zählt NICHT als Fehler.
void zusatz(bool bedingung, const char *text) {
  if (bedingung) {
    std::printf("  Zusatz ok      %s\n", text);
  } else {
    std::printf("  Zusatz HINWEIS %s (zählt nicht als Fehler)\n", text);
  }
}

// ======================================================================
// AK-1: Welche Zeichen zählen als Bestätigung
// ======================================================================

void testZeichen() {
  std::printf("\n-- AK-1: istBestaetigungsZeichen\n");
  pruefe(istBestaetigungsZeichen(' '), "AK-1: Leerzeichen zählt");
  pruefe(istBestaetigungsZeichen('\n'), "AK-1: Zeilenumbruch '\\n' zählt");
  pruefe(istBestaetigungsZeichen('\r'), "AK-1: Wagenrücklauf '\\r' zählt");

  // Alle 256 möglichen Zeichen: nur die drei oben dürfen true liefern.
  int zuViele = 0;
  for (int c = 0; c < 256; c++) {
    if (c == ' ' || c == '\n' || c == '\r') continue;
    if (istBestaetigungsZeichen(static_cast<char>(c))) zuViele++;
  }
  pruefe(zuViele == 0, "AK-1: alle anderen 253 Zeichen (0 bis 255) zählen NICHT");

  pruefe(!istBestaetigungsZeichen('e'), "AK-1: Buchstabe e zählt nicht");
  pruefe(!istBestaetigungsZeichen('E'), "AK-1: Buchstabe E zählt nicht");
  pruefe(!istBestaetigungsZeichen('1'), "AK-1: Ziffer 1 zählt nicht");
  pruefe(!istBestaetigungsZeichen('\t'), "AK-1: Tabulator zählt nicht");
  pruefe(!istBestaetigungsZeichen('\0'), "AK-1: Zeichen 0 (Nullbyte) zählt nicht");
  pruefe(!istBestaetigungsZeichen(static_cast<char>(0xFF)), "AK-1: Zeichen 0xFF zählt nicht");
  pruefe(!istBestaetigungsZeichen(static_cast<char>(0x80)), "AK-1: Zeichen 0x80 zählt nicht");
}

// ======================================================================
// AK-9: Wartezeit (30 000 ms), auch beim Überlauf von millis()
// ======================================================================

void testWartezeit() {
  std::printf("\n-- AK-9: wartezeitAbgelaufen\n");
  const unsigned long MAX = 30000UL;
  pruefe(!wartezeitAbgelaufen(1000UL, 1000UL, MAX), "AK-9: direkt am Anfang (0 ms) noch nicht abgelaufen");
  pruefe(!wartezeitAbgelaufen(1000UL, 16000UL, MAX), "AK-9: nach 15 s noch nicht abgelaufen");
  pruefe(!wartezeitAbgelaufen(1000UL, 30999UL, MAX), "AK-9: nach 29 999 ms noch nicht abgelaufen");
  pruefe(wartezeitAbgelaufen(1000UL, 31000UL, MAX), "AK-9: nach genau 30 000 ms abgelaufen");
  pruefe(wartezeitAbgelaufen(1000UL, 31001UL, MAX), "AK-9: nach 30 001 ms abgelaufen");
  pruefe(wartezeitAbgelaufen(1000UL, 500000UL, MAX), "AK-9: nach sehr langer Zeit abgelaufen");
  pruefe(!wartezeitAbgelaufen(0UL, 29999UL, MAX), "AK-9: Start bei millis() = 0: nach 29 999 ms nicht abgelaufen");
  pruefe(wartezeitAbgelaufen(0UL, 30000UL, MAX), "AK-9: Start bei millis() = 0: nach 30 000 ms abgelaufen");
  pruefe(wartezeitAbgelaufen(5UL, 5UL, 0UL), "AK-9: höchste Wartezeit 0 ms: sofort abgelaufen");
  pruefe(!wartezeitAbgelaufen(5UL, 104UL, 100UL), "AK-9: andere Grenze (100 ms): nach 99 ms nicht abgelaufen");
  pruefe(wartezeitAbgelaufen(5UL, 105UL, 100UL), "AK-9: andere Grenze (100 ms): nach 100 ms abgelaufen");

  if (sizeof(unsigned long) == 4) {
    // Auf dem Arduino ist unsigned long 32 Bit: millis() läuft nach ca. 49 Tagen über (auf 0 zurück).
    const unsigned long vorUeberlauf = 0xFFFFFFFFUL - 14999UL;   // 15 000 ms vor dem Überlauf
    pruefe(!wartezeitAbgelaufen(vorUeberlauf, 14999UL, MAX), "AK-9 Überlauf: 29 999 ms über den Überlauf hinweg: noch nicht abgelaufen");
    pruefe(wartezeitAbgelaufen(vorUeberlauf, 15000UL, MAX), "AK-9 Überlauf: genau 30 000 ms über den Überlauf hinweg: abgelaufen");
    pruefe(wartezeitAbgelaufen(vorUeberlauf, 15001UL, MAX), "AK-9 Überlauf: 30 001 ms: abgelaufen");
    pruefe(!wartezeitAbgelaufen(vorUeberlauf, 0xFFFFFFFFUL, MAX), "AK-9 Überlauf: kurz vor dem Überlauf: nicht abgelaufen");
    pruefe(!wartezeitAbgelaufen(0xFFFFFFFFUL, 29998UL, MAX), "AK-9 Überlauf: Start bei 0xFFFFFFFF, nach 29 999 ms nicht abgelaufen");
    pruefe(wartezeitAbgelaufen(0xFFFFFFFFUL, 29999UL, MAX), "AK-9 Überlauf: Start bei 0xFFFFFFFF, nach 30 000 ms abgelaufen");
  } else {
    std::printf("  (übersprungen) AK-9 Überlauf: unsigned long hat hier 64 Bit, der Überlauf von millis() (32 Bit) lässt sich\n"
                "                 auf diesem PC nicht nachstellen. Wird am Board / bei Code-Durchsicht geprüft.\n");
  }

  // Zufalls-Test: Start irgendwo (bei 32 Bit auch nahe am Überlauf), vergangene Zeit 0 bis 130 000 ms.
  std::srand(42);
  int falsch = 0;
  for (int i = 0; i < 20000; i++) {
    unsigned long start = (static_cast<unsigned long>(std::rand()) << 16) ^ static_cast<unsigned long>(std::rand());
    unsigned long vergangen = static_cast<unsigned long>(std::rand()) % 130000UL;
    unsigned long max = static_cast<unsigned long>(std::rand()) % 100000UL;
    unsigned long jetzt;
    if (sizeof(unsigned long) == 4) {
      start &= 0xFFFFFFFFUL;
      jetzt = (start + vergangen) & 0xFFFFFFFFUL;
    } else {
      start &= 0x7FFFFFFFUL;   // 64 Bit: kein Überlauf nachstellbar
      jetzt = start + vergangen;
    }
    bool soll = vergangen >= max;
    if (wartezeitAbgelaufen(start, jetzt, max) != soll) falsch++;
  }
  pruefe(falsch == 0, "AK-9: 20 000 zufällige Start- und Zeitwerte (auch nahe am Überlauf) stimmen mit 'vergangen >= max' überein");

  // Fuzz: extreme Werte dürfen nichts abstürzen lassen.
  int zaehler = 0;
  if (wartezeitAbgelaufen(0UL, 0xFFFFFFFFUL, 0xFFFFFFFFUL)) zaehler++;
  if (wartezeitAbgelaufen(0xFFFFFFFFUL, 0UL, 0xFFFFFFFFUL)) zaehler++;
  if (wartezeitAbgelaufen(0xFFFFFFFFUL, 0xFFFFFFFFUL, 0UL)) zaehler++;
  if (wartezeitAbgelaufen(0UL, 0UL, 0xFFFFFFFFUL)) zaehler++;
  pruefe(zaehler >= 0, "Fuzz: Extremwerte (0 und grösster Wert) bei wartezeitAbgelaufen: kein Absturz");
  pruefe(!wartezeitAbgelaufen(0UL, 0UL, 0xFFFFFFFFUL), "Fuzz: 0 ms vergangen bei riesiger Höchstzeit: nicht abgelaufen");
}

void testGrundNummer() {
  std::printf("\n-- Neuer Abbruchgrund\n");
  pruefe(EINMESSEN_GRUND_BESTAETIGUNG == 6, "REQ-004: EINMESSEN_GRUND_BESTAETIGUNG hat den Wert 6");
  pruefe(EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_KEINER && EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_BEUGUNG &&
             EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_RUHIG && EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_IMU &&
             EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_OBERARM && EINMESSEN_GRUND_BESTAETIGUNG != EINMESSEN_GRUND_HAND,
         "REQ-004: der neue Grund ist verschieden von allen alten Gründen (0 bis 5)");
}

// ======================================================================
// ERSETZT DURCH REQ-005: Die Tests zu AK-5 bis AK-9 (Auswerte-Funktionen, Warte-Zustand 2), AK-11 und AK-12 (alter Ablauf)
// sowie der alte Fuzz prüften den 3-Phasen-Ablauf (einmessenStillAuswerten, einmessenBewegungAuswerten, einmessenDrehungStart).
// Diese Funktionen gibt es nicht mehr. Die neuen Tests: test_REQ-005.cpp (Ruhe, Fortschritt, Erfolg, Abbruch, Neustart, Fuzz)
// und test_REQ-005_frei.cpp (freies Bewegen, Wiederholgenauigkeit). Gültig bleiben hier: AK-1 (Zeichen), AK-9 (Wartezeit 30 s).
// ======================================================================

int main() {
  testZeichen();
  testWartezeit();
  testGrundNummer();
  return testErgebnis();
}
