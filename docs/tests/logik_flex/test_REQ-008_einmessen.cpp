// test_REQ-008_einmessen.cpp  (Prototyp 2 Flex)
// Prüft aus REQ-008: AK-5 (Einmessen, ein Punkt), dazu Rauschen aus den Physik-Annahmen und Fuzz.
// Bedrohungen: B-2, B-6 (falsche Einmessung), B-7 (Einmessen blockieren: 30 s Zeitlimit).
//
// Wie wird getestet? Alle 20 ms (Messzyklus, AK-1) wird einmessenSchritt() aufgerufen.
// "ms" bedeutet immer: Millisekunden seit einmessenStarten().
// Regeln aus der REQ (Klärung 2026-11-17): Verletzt ein Wert die 3 %, beginnt das neue 2-s-Fenster MIT diesem Wert
// (er ist der erste Wert des neuen Fensters). Darum sind die Zeiten nach einem Wackeln genau: Zeit des verletzenden
// Wertes + 2 s (ein Zyklus Spielraum, je nachdem ob "genau 2 s" oder "mehr als 2 s" gerechnet wird).
// Nur beim allerersten Fenster (ab einmessenStarten) bleibt ein Spielraum von 1,98 bis 2,1 s.
// NaN und unendlich werden wie ein ungültiger Wert behandelt (Abbruch).

#include "test_hilfe.h"
#include "logik.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>

static const int LAEUFT = static_cast<int>(EINMESS_LAEUFT);
static const int FERTIG = static_cast<int>(EINMESS_FERTIG);
static const int ABBRUCH = static_cast<int>(EINMESS_ABBRUCH);

typedef std::function<float(uint32_t)> WertFkt;   // Widerstand zur Zeit "ms seit Start"

struct Ende {
  int art;          // 0 = noch am Laufen, FERTIG, ABBRUCH
  uint32_t nachMs;  // wann (ms seit Start)
};

// Ruft alle 20 ms einmessenSchritt auf, bis FERTIG oder ABBRUCH kommt oder maxMs vorbei sind.
static Ende laufen(EinmessZustand& z, uint32_t startMs, uint32_t maxMs, const WertFkt& wert) {
  Ende e = {LAEUFT, 0};
  for (uint32_t ms = 20; ms <= maxMs; ms += 20) {
    int r = static_cast<int>(einmessenSchritt(z, wert(ms), true, startMs + ms));
    if (r != LAEUFT) {
      e.art = r;
      e.nachMs = ms;
      return e;
    }
  }
  return e;
}

static Ende einmessenAb(uint32_t startMs, uint32_t maxMs, const WertFkt& wert, float* referenz) {
  EinmessZustand z{};
  einmessenStarten(z, startMs);
  Ende e = laufen(z, startMs, maxMs, wert);
  if (referenz != nullptr && e.art == FERTIG) *referenz = einmessenReferenz(z);
  return e;
}

int main() {
  std::srand(42);
  float ref = 0.0f;

  // ---------------------------------------------------------------- ruhig
  std::printf("AK-5: ruhige Werte: Referenz = Mittelwert nach 2 s\n");
  {
    Ende e = einmessenAb(1000, 5000, [](uint32_t) { return 20000.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: konstante Werte: Einmessen wird fertig");
    pruefe(e.nachMs >= 1980 && e.nachMs <= 2100, "AK-5: fertig nach etwa 2 s (nicht früher als 1,98 s, nicht später als 2,1 s)");
    pruefeNahe(ref, 20000.0, 40.0, "AK-5: Referenz = 20000 Ohm (Mittelwert)");
  }
  {
    // Rauschen innerhalb 3 %: abwechselnd +1 % und -1 % (Spannweite 2 %)
    Ende e = einmessenAb(5000, 5000, [](uint32_t ms) { return ((ms / 20) % 2) ? 20200.0f : 19800.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: Rauschen +-1 % (Spannweite 2 %) ist ruhig genug");
    pruefeNahe(ref, 20000.0, 100.0, "AK-5: Referenz ist der Mittelwert (20000 Ohm +-0,5 %)");
  }
  {
    // knapp unter 3 %: Spannweite 2,8 %
    Ende e = einmessenAb(0, 5000, [](uint32_t ms) { return ((ms / 20) % 2) ? 20280.0f : 19720.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: Spannweite 2,8 % des Mittelwerts (unter 3 %) ist ruhig genug");
  }
  {
    // klar über 3 %: Spannweite 3,6 % über 5 s: darf nicht fertig werden
    Ende e = einmessenAb(0, 5000, [](uint32_t ms) { return ((ms / 20) % 2) ? 20360.0f : 19640.0f; }, nullptr);
    pruefe(e.art == LAEUFT, "AK-5: Spannweite 3,6 % (über 3 %) wird nicht fertig, aber auch kein Abbruch innerhalb von 5 s");
  }
  {
    // Langsamer Anstieg innerhalb von 3 % (2,5 %), Mittelwert liegt dazwischen
    Ende e = einmessenAb(0, 4000, [](uint32_t ms) { return 20000.0f + 500.0f * (ms > 2000 ? 2000 : ms) / 2000.0f; }, &ref);
    pruefe(e.art == FERTIG && e.nachMs <= 2100, "AK-5: sanfter Anstieg (2,5 % Spannweite) ist ruhig genug");
    pruefeNahe(ref, 20250.0, 60.0, "AK-5: Referenz ist der Mittelwert über das Fenster (20250 Ohm, +-0,3 %)");
  }

  // ---------------------------------------------------------------- Wackeln: 2 s beginnen neu
  std::printf("AK-5: Wackeln = die 2 s beginnen neu, kein Abbruch\n");
  {
    // 5 s wild (20000/22000 im Wechsel), danach ruhig bei 22000
    Ende e = einmessenAb(0, 29000,
                         [](uint32_t ms) { return ms <= 5000 ? (((ms / 20) % 2) ? 22000.0f : 20000.0f) : 22000.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: nach Wackeln und 2 s Ruhe wird das Einmessen fertig (kein Abbruch)");
    pruefe(e.nachMs >= 7020 && e.nachMs <= 7040, "AK-5: das neue Fenster beginnt mit dem verletzenden Wert (5,02 s): fertig bei 7,02 s (bis 7,04 s)");
    pruefeNahe(ref, 22000.0, 50.0, "AK-5: Referenz stammt nur aus der ruhigen Zeit (22000 Ohm, nicht vermischt mit dem Wackeln)");
  }
  {
    // 1,5 s bei 19000, dann Sprung auf 20000 (5 %): die 2 s beginnen beim Sprung neu
    Ende e = einmessenAb(0, 10000, [](uint32_t ms) { return ms <= 1500 ? 19000.0f : 20000.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: Sprung nach 1,5 s: Einmessen wird trotzdem fertig");
    pruefe(e.nachMs >= 3520 && e.nachMs <= 3540, "AK-5: nach dem Sprung beginnen die 2 s neu mit dem Sprung-Wert (1,52 s): fertig bei 3,52 s (bis 3,54 s)");
    pruefeNahe(ref, 20000.0, 60.0, "AK-5: Referenz = 20000 Ohm (Werte vor dem Sprung zählen nicht mit)");
  }
  {
    // ein einzelner Ausreisser in der Mitte
    Ende e = einmessenAb(0, 10000, [](uint32_t ms) { return ms == 1000 ? 26000.0f : 20000.0f; }, &ref);
    pruefe(e.art == FERTIG && e.nachMs >= 3020 && e.nachMs <= 3040, "AK-5: ein einzelner Ausreisser bei 1,0 s: das neue Fenster beginnt mit dem nächsten Wert (1,02 s): fertig bei 3,02 s (bis 3,04 s)");
    pruefeNahe(ref, 20000.0, 40.0, "AK-5: Referenz ohne den Ausreisser (20000 Ohm)");
  }

  // ---------------------------------------------------------------- 30 s Zeitlimit
  std::printf("AK-5: nach 30 s ohne 2 ruhige Sekunden: Abbruch\n");
  {
    Ende e = einmessenAb(1000, 40000, [](uint32_t ms) { return ((ms / 20) % 2) ? 22000.0f : 20000.0f; }, nullptr);
    pruefe(e.art == ABBRUCH, "AK-5: dauerndes Wackeln endet mit Abbruch");
    pruefe(e.nachMs >= 29980 && e.nachMs <= 30200, "AK-5: Abbruch nach 30 s (29,98 s bis 30,2 s)");
  }
  {
    // 27 s wild, dann ruhig: ruhige 2 s passen noch in die 30 s
    Ende e = einmessenAb(0, 40000,
                         [](uint32_t ms) { return ms <= 27000 ? (((ms / 20) % 2) ? 22000.0f : 20000.0f) : 21000.0f; }, &ref);
    pruefe(e.art == FERTIG, "AK-5: Ruhe ab 27 s: 2 s passen noch in die 30 s, Einmessen wird fertig");
    pruefeNahe(ref, 21000.0, 50.0, "AK-5: Referenz 21000 Ohm");
    pruefe(e.nachMs >= 29020 && e.nachMs <= 29040, "AK-5: fertig 2 s nach dem verletzenden Wert (27,02 s): bei 29,02 s (bis 29,04 s)");
  }
  {
    // erst ab 29 s ruhig: nur noch 1 s Zeit: Abbruch
    Ende e = einmessenAb(0, 40000,
                         [](uint32_t ms) { return ms <= 29000 ? (((ms / 20) % 2) ? 22000.0f : 20000.0f) : 21000.0f; }, nullptr);
    pruefe(e.art == ABBRUCH && e.nachMs >= 29980 && e.nachMs <= 30200, "AK-5: Ruhe erst ab 29 s: reicht nicht mehr, Abbruch nach 30 s");
  }

  // ---------------------------------------------------------------- ungültige Werte
  std::printf("AK-5: ungültiger Wert beim Einmessen: sofort Abbruch\n");
  {
    EinmessZustand z{};
    einmessenStarten(z, 1000);
    int r = LAEUFT;
    for (uint32_t ms = 20; ms <= 1000; ms += 20) r = static_cast<int>(einmessenSchritt(z, 20000.0f, true, 1000 + ms));
    pruefe(r == LAEUFT, "AK-5: nach 1 s ruhiger Werte läuft das Einmessen noch");
    r = static_cast<int>(einmessenSchritt(z, -1.0f, false, 1000 + 1020));
    pruefe(r == ABBRUCH, "AK-5: ein ungültiger Wert (nach 1 s) führt sofort zum Abbruch");
  }
  {
    EinmessZustand z{};
    einmessenStarten(z, 5000);
    int r = static_cast<int>(einmessenSchritt(z, -1.0f, false, 5020));
    pruefe(r == ABBRUCH, "AK-5: ungültiger allererster Wert: Abbruch");
  }

  // ---------------------------------------------------------------- Wiederholung
  std::printf("AK-5: Einmessen lässt sich wiederholen (neuer Start vergisst den alten)\n");
  {
    EinmessZustand z{};
    einmessenStarten(z, 1000);
    Ende e1 = laufen(z, 1000, 5000, [](uint32_t) { return 20000.0f; });
    float r1 = einmessenReferenz(z);
    pruefe(e1.art == FERTIG, "AK-5: 1. Einmessen fertig");
    einmessenStarten(z, 20000);
    Ende e2 = laufen(z, 20000, 5000, [](uint32_t) { return 30000.0f; });
    float r2 = einmessenReferenz(z);
    pruefe(e2.art == FERTIG, "AK-5: 2. Einmessen mit demselben Zustand fertig");
    pruefeNahe(r1, 20000.0, 40.0, "AK-5: 1. Referenz 20000 Ohm");
    pruefeNahe(r2, 30000.0, 60.0, "AK-5: 2. Referenz 30000 Ohm (nicht vermischt mit der ersten)");
  }
  {
    EinmessZustand z{};
    einmessenStarten(z, 0);
    Ende e1 = laufen(z, 0, 40000, [](uint32_t ms) { return ((ms / 20) % 2) ? 22000.0f : 20000.0f; });
    pruefe(e1.art == ABBRUCH, "AK-5: 1. Einmessen bricht ab");
    einmessenStarten(z, 50000);
    Ende e2 = laufen(z, 50000, 5000, [](uint32_t) { return 25000.0f; });
    pruefe(e2.art == FERTIG, "AK-5: nach einem Abbruch lässt sich neu einmessen");
    pruefeNahe(einmessenReferenz(z), 25000.0, 50.0, "AK-5: neue Referenz 25000 Ohm");
  }

  // ---------------------------------------------------------------- Physik-Annahmen: Rauschen
  std::printf("Physik (REQ): Exemplar 15 bis 30 kOhm, Rauschen +-10 ADC-Werte: Einmessen klappt\n");
  {
    const double exemplare[] = {15000, 17500, 20000, 22000, 24000, 27000, 30000};
    bool allesFertig = true;
    bool referenzNah = true;
    for (unsigned i = 0; i < sizeof(exemplare) / sizeof(exemplare[0]); i++) {
      double wahr = exemplare[i];
      int adcMitte = adcAusOhm(wahr);
      EinmessZustand z{};
      einmessenStarten(z, 0);
      Ende e = {LAEUFT, 0};
      for (uint32_t ms = 20; ms <= 30000 && e.art == LAEUFT; ms += 20) {
        int adc = adcMitte + zufallInt(-10, 10);
        float r = flexWiderstandOhm(adc);
        int erg = static_cast<int>(einmessenSchritt(z, r, flexWertGueltig(adc), ms));
        if (erg != LAEUFT) { e.art = erg; e.nachMs = ms; }
      }
      if (e.art != FERTIG) allesFertig = false;
      else if (std::fabs(einmessenReferenz(z) - wahr) > 0.02 * wahr) referenzNah = false;
    }
    pruefe(allesFertig, "Physik: mit Rauschen +-10 ADC wird das Einmessen bei jedem Exemplar (15..30 kOhm) fertig");
    pruefe(referenzNah, "Physik: die Referenz liegt höchstens 2 % neben dem wahren Widerstand");
  }

  // ---------------------------------------------------------------- Zusatz: Zeitüberlauf
  std::printf("Zusatz: millis() läuft nach ca. 49 Tagen über (uint32). Nicht Teil der REQ.\n");
  {
    Ende e = einmessenAb(4294966000u, 5000, [](uint32_t) { return 20000.0f; }, nullptr);
    zusatz(e.art == FERTIG && e.nachMs <= 2100, "Zeitüberlauf mitten im Einmessen: wird trotzdem nach 2 s fertig");
  }

  // ---------------------------------------------------------------- Fuzz
  std::printf("Fuzz: Unsinn in einmessenSchritt, nichts stürzt ab, nie eine unsinnige Referenz (B-2, B-6)\n");
  {
    const float seltsam[] = {NAN, INFINITY, -INFINITY, 0.0f, -0.0f, -1.0f, 1e30f, -1e30f, 1e-30f, 20000.0f, 20000.0f, 20000.0f};
    const int n = sizeof(seltsam) / sizeof(seltsam[0]);
    bool artOk = true;
    bool referenzOk = true;
    for (int lauf = 0; lauf < 3000; lauf++) {
      EinmessZustand z{};
      uint32_t t = static_cast<uint32_t>(zufallInt(0, 2000000000));
      einmessenStarten(z, t);
      for (int schritt = 0; schritt < 3000; schritt++) {
        float r = (std::rand() % 4 == 0) ? seltsam[std::rand() % n] : static_cast<float>(zufall(0.0, 200000.0));
        if (std::rand() % 3 == 0) r = 20000.0f + static_cast<float>(zufall(-100.0, 100.0));
        bool gueltig = (std::rand() % 10) != 0;
        t += static_cast<uint32_t>(zufallInt(0, 100));
        int erg = static_cast<int>(einmessenSchritt(z, r, gueltig, t));
        if (erg != LAEUFT && erg != FERTIG && erg != ABBRUCH) artOk = false;
        if (erg == FERTIG) {
          float referenz = einmessenReferenz(z);
          if (!(std::isfinite(referenz) && referenz > 0.0f)) referenzOk = false;
        }
        if (erg != LAEUFT) break;
      }
    }
    pruefe(artOk, "Fuzz: einmessenSchritt liefert nur 0, 1 oder 2");
    pruefe(referenzOk, "Fuzz: nach FERTIG ist die Referenz immer endlich und grösser als 0");
  }
  {
    // NaN und unendlich (als "gültig" gemeldet) werden wie ein ungültiger Wert behandelt: sofort Abbruch, nie eine Referenz
    const float kaputt[] = {NAN, INFINITY, -INFINITY};
    bool alleAbbruch = true;
    bool mitteAbbruch = true;
    for (unsigned i = 0; i < 3; i++) {
      EinmessZustand z{};
      einmessenStarten(z, 0);
      int erg = static_cast<int>(einmessenSchritt(z, kaputt[i], true, 20));
      if (erg != ABBRUCH) alleAbbruch = false;
      // mitten in einem ruhigen Fenster
      EinmessZustand z2{};
      einmessenStarten(z2, 0);
      int r = LAEUFT;
      for (uint32_t ms = 20; ms <= 1000; ms += 20) r = static_cast<int>(einmessenSchritt(z2, 20000.0f, true, ms));
      if (r != LAEUFT) mitteAbbruch = false;
      r = static_cast<int>(einmessenSchritt(z2, kaputt[i], true, 1020));
      if (r != ABBRUCH) mitteAbbruch = false;
    }
    pruefe(alleAbbruch, "AK-5/B-2: NaN, +unendlich, -unendlich als allererster Wert: Abbruch (wie ungültiger Wert)");
    pruefe(mitteAbbruch, "AK-5/B-2: NaN, +unendlich, -unendlich mitten im ruhigen Fenster: sofort Abbruch");
  }
  {
    // Nur 0 Ohm (als "gültig" gemeldet): darf keine Referenz 0 ergeben
    EinmessZustand z{};
    einmessenStarten(z, 0);
    bool nieNullReferenz = true;
    for (uint32_t ms = 20; ms <= 35000; ms += 20) {
      int erg = static_cast<int>(einmessenSchritt(z, 0.0f, true, ms));
      if (erg == FERTIG) { if (!(einmessenReferenz(z) > 0.0f)) nieNullReferenz = false; break; }
      if (erg == ABBRUCH) break;
    }
    pruefe(nieNullReferenz, "Fuzz (B-2): nur 0-Ohm-Werte ergeben keine Referenz 0 (sonst gäbe es nie Warnungen)");
  }

  return testErgebnis();
}
