// test_REQ-004.cpp
// Tests zu REQ-004 "Jeden Einmess-Schritt bestätigen" (reine Logik, Teil 1).
// Teil 2 (Wartezeiten bis 29 s, AK-13) steht in test_REQ-004_warten.cpp.
//
// Was hier geprüft wird (Logik auf dem PC):
//   AK-1  : istBestaetigungsZeichen (nur ' ', '\n', '\r'), AK-1/B-1 auch alle anderen 253 Zeichen
//   AK-5  : einmessenStillAuswerten (ruhig -> weiter, nicht ruhig -> Abbruch, Grund RUHIG)
//   AK-6  : einmessenBewegungAuswerten (Beugung, Oberarm; Reihenfolge Beugung vor Oberarm)
//   AK-7  : einmessenAbschliessen nach den neuen Auswerte-Funktionen (Hand, alles zusammen)
//   AK-9  : wartezeitAbgelaufen (30 000 ms, auch beim Überlauf von millis())
//   AK-11 : Abbruch lässt alte Referenz und Achse stehen; sofort wieder bereit für einen Neustart
//   AK-12 : einmessenAbbrechen in einem Warte-Zustand (IMU-Ausfall) = Abbruch, Referenz bleibt
//   B-1/B-6/B-7: Fuzz (NaN, unendlich, Zufallsreihenfolge der Aufrufe)
// NICHT hier prüfbar (steckt in jointsense.ino): AK-2, AK-3, AK-4, AK-8, AK-10, AK-14 (Zeiten), AK-15, Meldungstexte.
// Die sind im Board-Test (docs/tests/board-test-REQ-004.md) und später in Wokwi.
//
// Die Winkel-Logik wird nicht nachgebaut: der Simulator (sim_ellbogen.h) erzeugt nur die Sensorwerte.
// Die Bestätigung kommt hier "sofort" (0 Wartezeit). Lange Wartezeiten: test_REQ-004_warten.cpp.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"

const float MIN_BEUGUNG = 30.0f;

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
// Einmessen mit den neuen Auswerte-Funktionen (Bestätigung sofort)
// ======================================================================

// Ein ganzes Einmessen wie im Betrieb, aber mit den Prüfungen nach jeder Phase. Verbraucht immer genau 375 Zyklen
// (7,5 s), auch bei Abbruch (der Arm bewegt sich im Test einfach weiter, das Gerät misst weiter).
// abbruchNach: 0 = nie, 1 = von aussen abbrechen im Warte-Zustand 1, 2 = von aussen abbrechen im Warte-Zustand 2
//              (z. B. Wartezeit abgelaufen oder IMU-Ausfall, AK-9 / AK-12).
// Rückgabe: 0 = alles ok, 1 = Still-Prüfung false, 2 = Bewegungs-Prüfung false, 3 = Abschluss false, 4 = von aussen abgebrochen
int einmessenMitAuswerten(ArmSim &sim, EllbogenZustand &z, float minBeugung, int abbruchNach) {
  einmessenStart(z);
  zyklenOhne(sim, z, 100);
  if (!einmessenStillAuswerten(z)) {
    zyklenOhne(sim, z, 275);
    return 1;
  }
  if (abbruchNach == 1) {
    einmessenAbbrechen(z);
    zyklenOhne(sim, z, 275);
    return 4;
  }
  einmessenBewegungStart(z);
  zyklenOhne(sim, z, 200);
  if (!einmessenBewegungAuswerten(z, minBeugung)) {
    zyklenOhne(sim, z, 75);
    return 2;
  }
  if (abbruchNach == 2) {
    einmessenAbbrechen(z);
    zyklenOhne(sim, z, 75);
    return 4;
  }
  einmessenDrehungStart(z);
  zyklenOhne(sim, z, 75);
  if (!einmessenAbschliessen(z, minBeugung)) return 3;
  return 0;
}

// Nach dem Einmessen: 4 s Beugen bis -70 Grad und zurück.
Pose danachBeugen(double s, const Pose &ende) {
  Pose p = ende;
  p.flex = ende.flex - 35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * s / 4.0));
  return p;
}

void testErfolg() {
  std::printf("\n-- AK-5, AK-6, AK-7: normales Einmessen mit Auswerte-Funktionen\n");
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachBeugen), einbauStandard(), fehlerNurRauschen(), 801);
  int stufe = einmessenMitAuswerten(sim, z, MIN_BEUGUNG, 0);
  pruefe(stufe == 0, "AK-5/6/7: Still-, Bewegungs-Prüfung und Abschluss liefern alle true (2 Mal beugen 70 Grad, Oberarm-Kreis 20 Grad)");
  Messreihe m = laufe(sim, z, 8.0, 0.0);
  zeige("danach", m);
  pruefe(m.unbekannt == 0 && m.maxFehler <= 2.5, "AK-7: nach dem Einmessen ist die Lage bekannt und der Winkel stimmt auf 2,5 Grad");

  // Gleiche Bewegung mit Sensorfehlern (Gyro-Offset +-2 Grad/s): muss ebenfalls klappen.
  for (int k = 0; k < 2; k++) {
    EllbogenZustand z2;
    ellbogenZuruecksetzen(z2);
    ArmSim sim2(mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachBeugen), einbauStandard(), fehlerMitOffset(k), 802 + k);
    char text[160];
    std::snprintf(text, sizeof(text), "AK-5/6/7: mit Gyro-Offset +-2 Grad/s und Rauschen (Muster %d) klappen alle Prüfungen", k);
    pruefe(einmessenMitAuswerten(sim2, z2, MIN_BEUGUNG, 0) == 0, text);
  }

  // Gleiche Bewegung ohne die neuen Funktionen (alter Ablauf): einmessenAbschliessen verhält sich wie in REQ-002.
  EllbogenZustand z3;
  ellbogenZuruecksetzen(z3);
  ArmSim sim3(mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachBeugen), einbauStandard(), fehlerNurRauschen(), 801);
  pruefe(einmessenLaufen(sim3, z3, MIN_BEUGUNG), "REQ-004: alter Ablauf ohne Auswerte-Funktionen: einmessenAbschliessen liefert weiter true");
  Messreihe m3 = laufe(sim3, z3, 8.0, 0.0);
  pruefe(m3.unbekannt == 0 && m3.maxFehler <= 2.5, "REQ-004: alter Ablauf: Winkel danach weiter auf 2,5 Grad genau");
}

// Zeitplan für zwei Einmessungen: 0 bis 7,5 s erstes (gültig, bei 0 Grad), dann Arm weich auf -20 Grad (bis 11,5 s),
// ab 11,5 s das zweite Einmessen (Grundlage -20 Grad) mit veränderter Bewegung, ab 25 s Beugen bis -90 Grad.
Bewegung zweiVersuche(double amp2, double pron2, double yaw2, int obArt2, double obGrad2, double hand2) {
  return [=](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t < 9.5) {
      p.flex = -20.0 * weich((t - 7.5) / 2.0);
      return p;
    }
    p.flex = -20.0;
    if (t < 11.5) return p;
    double s = t - 11.5;
    if (s < 2.0) {   // Phase 1 (eventuell nicht ruhig)
      p.pron = pron2 * s;
      p.yaw = yaw2 * s;
      return p;
    }
    p.pron = pron2 * 2.0;
    p.yaw = yaw2 * 2.0;
    if (s < 6.0) {   // Phase 2
      p.flex = -20.0 + beugeKurve(s - 2.0, amp2, 2);
      oberarmBewegen(p, s - 2.0, obArt2, obGrad2);
      return p;
    }
    if (s < 7.5) {   // Phase 3: Hand drehen um hand2 Grad hin und zurück
      double k = 0.5 * (1.0 - std::cos(2.0 * PI_ZAHL * (s - 6.0) / 1.5));
      p.pron = p.pron + hand2 * k;
      return p;
    }
    if (t >= 25.0) p.flex = -20.0 - 35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * (t - 25.0) / 4.0));
    return p;
  };
}

struct Fall {
  const char *name;
  double amp;       // Beugung (Grad) in Phase 2
  double pron;      // Unterarm dreht in Phase 1 (Grad/s)
  double yaw;       // ganzer Arm schwenkt in Phase 1 (Grad/s)
  int obArt;        // Oberarm-Bewegung in Phase 2
  double obGrad;
  double hand;      // Hand-Drehung in Phase 3 (Grad)
  int abbruchNach;  // 0 = nie, 1 / 2 = von aussen abbrechen im Warte-Zustand 1 / 2
  int erwartStufe;  // 0 = klappt, 1 / 2 / 3 = Abbruch in dieser Prüfung, 4 = von aussen abgebrochen
  int erwartGrund;  // -1 = nicht prüfen
  bool pflicht;     // true: muss genau so enden. false: darf auch klappen, dann muss der Winkel stimmen (richtig oder Abbruch)
};

// Zweites Einmessen mit alter Referenz (0 Grad): was muss passieren (AK-5, 6, 7, 11, 12)?
void fallLaufen(const Fall &f) {
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(zweiVersuche(f.amp, f.pron, f.yaw, f.obArt, f.obGrad, f.hand), einbauStandard(), fehlerNurRauschen(), 901);
  int s1 = einmessenMitAuswerten(sim, z, MIN_BEUGUNG, 0);            // t = 0 bis 7,5
  laufe(sim, z, 4.0, 0.0);                                           // t = 7,5 bis 11,5
  int s2 = einmessenMitAuswerten(sim, z, MIN_BEUGUNG, f.abbruchNach);  // t = 11,5 bis 19
  unsigned grund = einmessenGrund(z);
  bool abgebrochen = (s2 != 0);
  bool nochmal = abgebrochen ? einmessenAbschliessen(z, MIN_BEUGUNG) : false;
  double refFlex = abgebrochen ? 0.0 : -20.0;                        // bei Abbruch gilt die alte Referenz (0 Grad)
  Messreihe halten = laufe(sim, z, 6.0, 0.0, refFlex);               // t = 19 bis 25
  Messreihe bewegt = laufe(sim, z, 8.0, 0.0, refFlex);               // t = 25 bis 33
  char text[300];
  pruefe(s1 == 0, "Vorbereitung: erstes Einmessen klappt");
  if (f.pflicht) {
    std::snprintf(text, sizeof(text), "%s: endet in der erwarteten Prüfung (Stufe %d, war %d)", f.name, f.erwartStufe, s2);
    pruefe(s2 == f.erwartStufe, text);
    if (f.erwartGrund >= 0) {
      std::snprintf(text, sizeof(text), "%s: einmessenGrund = %d (war %u)", f.name, f.erwartGrund, grund);
      pruefe(static_cast<int>(grund) == f.erwartGrund, text);
    }
  }
  if (abgebrochen) {
    std::snprintf(text, sizeof(text), "%s: nach dem Abbruch liefert einmessenAbschliessen nie true", f.name);
    pruefe(!nochmal, text);
  }
  std::snprintf(text, sizeof(text), "%s: die Lage bleibt bekannt (kein Unterbruch der Messung)", f.name);
  pruefe(halten.unbekannt == 0 && bewegt.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "%s: %s stimmt der Winkel auf 2,5 Grad (halten und beugen)", f.name,
                abgebrochen ? "alte Referenz und Achse bleiben, danach" : "mit der neuen Referenz");
  zeige("halten", halten);
  zeige("bewegt", bewegt);
  pruefe(halten.maxFehler <= 2.5 && bewegt.maxFehler <= 2.5, text);
}

void testAbbrueche() {
  std::printf("\n-- AK-5, AK-6, AK-7, AK-11, AK-12: Abbruch in jeder Prüfung (alte Referenz bleibt)\n");
  const Fall faelle[] = {
      // name, amp, pron, yaw, obArt, obGrad, hand, abbruchNach, stufe, grund, pflicht
      {"AK-5: Phase 1 nicht ruhig (Unterarm dreht 40 Grad/s)", 70.0, 40.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 0, 1, EINMESSEN_GRUND_RUHIG, true},
      {"AK-5: Phase 1 nicht ruhig (ganzer Arm schwenkt 30 Grad/s)", 70.0, 0.0, 30.0, OBERARM_KREISE, 20.0, 60.0, 0, 1, EINMESSEN_GRUND_RUHIG, true},
      {"AK-6: Beugung nur 10 Grad", 10.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 0, 2, EINMESSEN_GRUND_BEUGUNG, true},
      {"AK-6: Spannweite nur 20 Grad", 20.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 0, 2, EINMESSEN_GRUND_BEUGUNG, true},
      {"AK-6: Oberarm hängt still (Unterarm beugt 70 Grad)", 70.0, 0.0, 0.0, OBERARM_NICHTS, 0.0, 60.0, 0, 2, EINMESSEN_GRUND_OBERARM, true},
      {"AK-6: gar keine Bewegung in Phase 2 (Beugung vor Oberarm: Grund Beugung)", 0.0, 0.0, 0.0, OBERARM_NICHTS, 0.0, 60.0, 0, 2, EINMESSEN_GRUND_BEUGUNG, true},
      {"AK-6: Oberarm nur um die Senkrechte (darf klappen oder Oberarm-Abbruch)", 70.0, 0.0, 0.0, OBERARM_SENKRECHT, 20.0, 60.0, 0, 2, EINMESSEN_GRUND_OBERARM, false},
      {"AK-7: Hand in Phase 3 nicht gedreht", 70.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 0.0, 0, 3, EINMESSEN_GRUND_HAND, true},
      {"AK-7: Hand nur 30 Grad gedreht (darf klappen oder Hand-Abbruch)", 70.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 30.0, 0, 3, EINMESSEN_GRUND_HAND, false},
      {"AK-12: Abbruch von aussen im Warte-Zustand 1 (IMU-Ausfall / Wartezeit)", 70.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 1, 4, -1, true},
      {"AK-12: Abbruch von aussen im Warte-Zustand 2 (IMU-Ausfall / Wartezeit)", 70.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 2, 4, -1, true},
      {"Gegenprobe: alles ok, Einmessen klappt (neue Referenz bei -20 Grad)", 70.0, 0.0, 0.0, OBERARM_KREISE, 20.0, 60.0, 0, 0, -1, true},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) fallLaufen(faelle[i]);
}

// Gründe auch ohne die neuen Funktionen (alter Ablauf, nur einmessenAbschliessen): unverändert wie REQ-002.
void testAlterAblaufGruende() {
  std::printf("\n-- REQ-004: einmessenAbschliessen allein (ohne Auswerte-Funktionen) verhält sich wie in REQ-002\n");
  struct Alt {
    const char *name;
    double amp;
    double pron;
    int obArt;
    double hand;
    int grund;
  };
  const Alt faelle[] = {
      {"nicht ruhig -> Grund ruhig", 70.0, 40.0, OBERARM_KREISE, 60.0, EINMESSEN_GRUND_RUHIG},
      {"Beugung 10 Grad -> Grund Beugung", 10.0, 0.0, OBERARM_KREISE, 60.0, EINMESSEN_GRUND_BEUGUNG},
      {"Oberarm still -> Grund Oberarm", 70.0, 0.0, OBERARM_NICHTS, 60.0, EINMESSEN_GRUND_OBERARM},
      {"Hand nicht gedreht -> Grund Hand", 70.0, 0.0, OBERARM_KREISE, 0.0, EINMESSEN_GRUND_HAND},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    Phase3Einstellung einst(faelle[i].hand, 0);   // stellt am Ende alles zurück
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(mitEinmessenOberarm(poseHaengend(), faelle[i].amp, 2, faelle[i].pron, 0.0, faelle[i].obArt, 20.0, danachHalten),
               einbauStandard(), fehlerNurRauschen(), 950 + static_cast<unsigned long long>(i));
    bool ok = einmessenLaufen(sim, z, MIN_BEUGUNG);
    char text[200];
    std::snprintf(text, sizeof(text), "REQ-004 alter Ablauf: %s", faelle[i].name);
    pruefe(!ok && static_cast<int>(einmessenGrund(z)) == faelle[i].grund, text);
  }
}

// ======================================================================
// AK-11: sofort wieder bereit nach einem Abbruch
// ======================================================================

Bewegung zweiMalEinmessen(int obArtA, double obGradA) {
  return [=](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, obArtA, obGradA)) return p;   // 0 bis 7,5 s: erster Versuch
    if (t < 11.5) return p;                                              // Pause
    if (einmessenPose(p, t, 11.5, 70.0, 2, OBERARM_KREISE, 20.0)) return p;   // 11,5 bis 19 s: zweiter Versuch, gut
    if (t >= 22.0) {
      p.flex = -35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * (t - 22.0) / 4.0));
      p.pron = 40.0 * std::sin(2.0 * PI_ZAHL * (t - 22.0) / 3.0);
    }
    return p;
  };
}

void neustartNachAbbruch(const char *kopf, int obArtA, double obGradA, int abbruchNach, int erwartStufe) {
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(zweiMalEinmessen(obArtA, obGradA), einbauStandard(), fehlerNurRauschen(), 960);
  int s1 = einmessenMitAuswerten(sim, z, MIN_BEUGUNG, abbruchNach);   // t = 0 bis 7,5: scheitert
  bool nochmal = einmessenAbschliessen(z, MIN_BEUGUNG);
  Messreihe leer = laufe(sim, z, 4.0, 0.0);                           // t = 7,5 bis 11,5
  int s2 = einmessenMitAuswerten(sim, z, MIN_BEUGUNG, 0);             // t = 11,5 bis 19: neuer Start
  Messreihe halten = laufe(sim, z, 3.0, 0.0);                         // t = 19 bis 22
  Messreihe bewegt = laufe(sim, z, 8.0, 0.0);                         // t = 22 bis 30
  char text[240];
  std::snprintf(text, sizeof(text), "AK-11 %s: erster Versuch scheitert wie erwartet (Stufe %d, war %d)", kopf, erwartStufe, s1);
  pruefe(s1 == erwartStufe, text);
  std::snprintf(text, sizeof(text), "AK-11 %s: nach dem Abbruch liefert einmessenAbschliessen nie true", kopf);
  pruefe(!nochmal, text);
  std::snprintf(text, sizeof(text), "AK-11 %s: ohne alte Referenz bleibt es bei 'keine Abweichung' (kein Alarm erlaubt)", kopf);
  pruefe(leer.zyklen > 0 && leer.unbekannt == leer.zyklen, text);
  std::snprintf(text, sizeof(text), "AK-11 %s: sofort neu starten klappt (alle drei Prüfungen true)", kopf);
  pruefe(s2 == 0, text);
  std::snprintf(text, sizeof(text), "AK-11 %s: danach stimmt der Winkel auf 2,5 Grad (halten und beugen)", kopf);
  zeige("halten", halten);
  zeige("bewegt", bewegt);
  pruefe(halten.unbekannt == 0 && bewegt.unbekannt == 0 && halten.maxFehler <= 2.5 && bewegt.maxFehler <= 2.5, text);
}

void testNeustart() {
  std::printf("\n-- AK-11: nach einem Abbruch sofort wieder bereit\n");
  neustartNachAbbruch("(Abbruch in Prüfung 2: Oberarm still)", OBERARM_NICHTS, 0.0, 0, 2);
  neustartNachAbbruch("(Abbruch von aussen im Warte-Zustand 1)", OBERARM_KREISE, 20.0, 1, 4);
  neustartNachAbbruch("(Abbruch von aussen im Warte-Zustand 2)", OBERARM_KREISE, 20.0, 2, 4);
}

// ======================================================================
// Fuzz: Zufall, NaN, unendlich, extreme Werte, wilde Reihenfolge der Aufrufe (B-1, B-2, B-6, B-7)
// ======================================================================

float zufallsFloat(float klein) {
  // gleichmässig in -klein bis +klein
  return (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * klein;
}

float extremWert() {
  switch (std::rand() % 10) {
    case 0: return NAN;
    case 1: return INFINITY;
    case 2: return -INFINITY;
    case 3: return 1e30f;
    case 4: return -1e30f;
    case 5: return 0.0f;
    case 6: return 9999.0f;
    case 7: return -9999.0f;
    case 8: return 1e-30f;
    default: return 2001.0f;
  }
}

ImuMessung zufallsMessung() {
  ImuMessung m;
  m.ax = zufallsFloat(1.5f);
  m.ay = zufallsFloat(1.5f);
  m.az = zufallsFloat(1.5f);
  m.gx = zufallsFloat(300.0f);
  m.gy = zufallsFloat(300.0f);
  m.gz = zufallsFloat(300.0f);
  if (std::rand() % 6 == 0) {   // jedes sechste Mal ein Feld kaputt machen
    switch (std::rand() % 6) {
      case 0: m.ax = extremWert(); break;
      case 1: m.ay = extremWert(); break;
      case 2: m.az = extremWert(); break;
      case 3: m.gx = extremWert(); break;
      case 4: m.gy = extremWert(); break;
      default: m.gz = extremWert(); break;
    }
  }
  return m;
}

float zufallsDt() {
  switch (std::rand() % 12) {
    case 0: return 0.0f;
    case 1: return -0.02f;
    case 2: return 0.5f;
    case 3: return 0.51f;
    case 4: return NAN;
    case 5: return INFINITY;
    case 6: return 1e30f;
    default: return 0.02f;
  }
}

float zufallsMinBeugung() {
  switch (std::rand() % 8) {
    case 0: return NAN;
    case 1: return INFINITY;
    case 2: return -INFINITY;
    case 3: return 0.0f;
    case 4: return -30.0f;
    case 5: return 1e30f;
    default: return 30.0f;
  }
}

void testFuzz() {
  std::printf("\n-- Fuzz (B-1, B-2, B-6, B-7): wilde Reihenfolge der Aufrufe, NaN, unendlich, Extremwerte\n");
  std::srand(42);
  int unendlicheAbweichung = 0;
  int abschlussNachAbbruch = 0;
  int nichtEndlichAkzeptiert = 0;
  for (int lauf = 0; lauf < 20; lauf++) {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    bool abgebrochen = false;   // seit dem letzten einmessenStart: Auswerte false oder einmessenAbbrechen
    for (int schritt = 0; schritt < 3000; schritt++) {
      int op = std::rand() % 12;
      if (op == 0) {
        einmessenStart(z);
        abgebrochen = false;
      } else if (op == 1) {
        einmessenBewegungStart(z);
      } else if (op == 2) {
        einmessenDrehungStart(z);
      } else if (op == 3) {
        if (!einmessenStillAuswerten(z)) abgebrochen = true;
      } else if (op == 4) {
        if (!einmessenBewegungAuswerten(z, zufallsMinBeugung())) abgebrochen = true;
      } else if (op == 5) {
        bool ok = einmessenAbschliessen(z, zufallsMinBeugung());
        if (ok && abgebrochen) abschlussNachAbbruch++;
      } else if (op == 6) {
        einmessenAbbrechen(z);
        abgebrochen = true;
      } else {
        ImuMessung o = zufallsMessung();
        ImuMessung u = zufallsMessung();
        float dt = zufallsDt();
        bool nichtGueltig = !std::isfinite(o.ax) || !std::isfinite(o.ay) || !std::isfinite(o.az) || !std::isfinite(o.gx) ||
                            !std::isfinite(o.gy) || !std::isfinite(o.gz) || !std::isfinite(u.ax) || !std::isfinite(u.ay) ||
                            !std::isfinite(u.az) || !std::isfinite(u.gx) || !std::isfinite(u.gy) || !std::isfinite(u.gz) ||
                            !std::isfinite(dt) || dt <= 0.0f || dt > 0.5f;
        bool ok = ellbogenAktualisieren(z, o, u, dt);
        if (ok && nichtGueltig) nichtEndlichAkzeptiert++;   // AK-14 (REQ-002): ungültiger Zyklus muss abgelehnt werden
      }
      float a = 0.0f;
      if (ellbogenAbweichung(z, a) && !std::isfinite(a)) unendlicheAbweichung++;
    }
  }
  pruefe(unendlicheAbweichung == 0, "Fuzz: 60 000 wilde Aufrufe: die Abweichung ist nie NaN oder unendlich (Motor-Entscheidung bleibt sicher)");
  pruefe(abschlussNachAbbruch == 0, "Fuzz: nach einem Abbruch liefert einmessenAbschliessen nie true (bis zum nächsten einmessenStart)");
  pruefe(nichtEndlichAkzeptiert == 0, "Fuzz: Zyklen mit NaN, unendlich oder falschem dt werden abgelehnt");

  // Zusatz (Annahmen des Testers, aus der Grundregel "fail-safe"): zählen nicht als Fehler.
  {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    einmessenStart(z);
    zusatz(!einmessenStillAuswerten(z), "Zusatz: Still-Prüfung ohne einen einzigen Messzyklus liefert false (keine Daten = nicht ruhig bestätigt)");
  }
  {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachHalten), einbauStandard(), fehlerNurRauschen(), 970);
    einmessenStart(z);
    zyklenOhne(sim, z, 100);
    bool still = einmessenStillAuswerten(z);
    einmessenBewegungStart(z);
    zyklenOhne(sim, z, 200);
    zusatz(still && !einmessenBewegungAuswerten(z, NAN), "Zusatz: Mindest-Beugung NaN bei der Bewegungs-Prüfung liefert false (fail-safe)");
  }
  {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachHalten), einbauStandard(), fehlerNurRauschen(), 971);
    zusatz(!einmessenLaufen(sim, z, NAN), "Zusatz: Mindest-Beugung NaN bei einmessenAbschliessen liefert false (fail-safe)");
  }
}

int main() {
  testZeichen();
  testWartezeit();
  testGrundNummer();
  testErfolg();
  testAbbrueche();
  testAlterAblaufGruende();
  testNeustart();
  testFuzz();
  return testErgebnis();
}
