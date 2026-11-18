// test_REQ-008_messen.cpp  (Prototyp 2 Flex)
// Prüft aus REQ-008: AK-2 (Widerstand), AK-3 (ungültige Werte), AK-4 (Sensor-Ausfall), AK-6 (Abweichung in Prozent)
// und Fuzz (NaN, unendlich, extreme Werte) für diese vier Teile. Bedrohungen: B-2, B-8.

#include "test_hilfe.h"
#include "logik.h"
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>

// Gibt "anzahl" gleiche Werte in die Ausfall-Logik und liefert das letzte Ergebnis.
static bool fuettere(AusfallZustand& z, int anzahl, bool gueltig) {
  bool letztes = false;
  for (int i = 0; i < anzahl; i++) letztes = ausfallAktualisieren(z, gueltig);
  return letztes;
}

int main() {
  std::srand(42);

  // ---------------------------------------------------------------- AK-3: gültiger Bereich
  std::printf("AK-3: ADC 100 bis 3995 ist gültig, alles andere ungültig\n");
  pruefe(flexWertGueltig(100), "AK-3: 100 ist gültig (Grenze)");
  pruefe(flexWertGueltig(3995), "AK-3: 3995 ist gültig (Grenze)");
  pruefe(flexWertGueltig(2000), "AK-3: 2000 ist gültig");
  pruefe(!flexWertGueltig(99), "AK-3: 99 ist ungültig (Kabel ab)");
  pruefe(!flexWertGueltig(3996), "AK-3: 3996 ist ungültig (Kurzschluss)");
  pruefe(!flexWertGueltig(0), "AK-3: 0 ist ungültig");
  pruefe(!flexWertGueltig(4095), "AK-3: 4095 ist ungültig");
  pruefe(!flexWertGueltig(-1), "AK-3: -1 ist ungültig");
  pruefe(!flexWertGueltig(INT_MIN), "AK-3: INT_MIN ist ungültig");
  pruefe(!flexWertGueltig(INT_MAX), "AK-3: INT_MAX ist ungültig");

  std::printf("AK-3: für ungültige Werte gibt es keinen Widerstand (-1.0)\n");
  pruefeNahe(flexWiderstandOhm(99), -1.0, 0.001, "AK-3: Widerstand bei 99 ist -1.0");
  pruefeNahe(flexWiderstandOhm(3996), -1.0, 0.001, "AK-3: Widerstand bei 3996 ist -1.0");
  pruefeNahe(flexWiderstandOhm(0), -1.0, 0.001, "AK-3: Widerstand bei 0 ist -1.0 (keine Division durch 0)");
  pruefeNahe(flexWiderstandOhm(4095), -1.0, 0.001, "AK-3: Widerstand bei 4095 ist -1.0");
  pruefeNahe(flexWiderstandOhm(-5), -1.0, 0.001, "AK-3: Widerstand bei -5 ist -1.0");
  pruefeNahe(flexWiderstandOhm(INT_MAX), -1.0, 0.001, "AK-3: Widerstand bei INT_MAX ist -1.0");
  pruefeNahe(flexWiderstandOhm(INT_MIN), -1.0, 0.001, "AK-3: Widerstand bei INT_MIN ist -1.0");

  // ---------------------------------------------------------------- AK-2: Formel
  std::printf("AK-2: R_flex = 22000 * (4095 / adc - 1), Abweichung höchstens 1 %%\n");
  const int stichproben[] = {100, 500, 1000, 1732, 2000, 2048, 2145, 3000, 3995};
  for (unsigned i = 0; i < sizeof(stichproben) / sizeof(stichproben[0]); i++) {
    int adc = stichproben[i];
    double soll = 22000.0 * (4095.0 / adc - 1.0);
    char text[80];
    std::snprintf(text, sizeof(text), "AK-2: ADC %d ergibt den Widerstand der Formel (1 %%)", adc);
    pruefeNahe(flexWiderstandOhm(adc), soll, 0.01 * soll, text);
  }
  double schlimmsterFehler = 0.0;
  bool fallend = true;
  for (int adc = 100; adc <= 3995; adc++) {
    double soll = 22000.0 * (4095.0 / adc - 1.0);
    double ist = flexWiderstandOhm(adc);
    double fehler = std::fabs(ist - soll) / soll;
    if (!(fehler <= schlimmsterFehler)) schlimmsterFehler = fehler;
    if (adc < 3995 && !(flexWiderstandOhm(adc + 1) < ist)) fallend = false;
  }
  pruefe(schlimmsterFehler <= 0.01, "AK-2: für jeden gültigen ADC-Wert (100..3995) höchstens 1 % Abweichung zur Formel");
  pruefe(fallend, "AK-2: höherer ADC-Wert = kleinerer Widerstand (Richtung stimmt, Physik-Annahme)");

  // ---------------------------------------------------------------- AK-6: Abweichung in Prozent
  std::printf("AK-6: Abweichung = (R_referenz - R_flex) / R_referenz * 100\n");
  pruefeNahe(abweichungProzent(19000.0f, 20000.0f), 5.0, 0.001, "AK-6: R 19000 bei Referenz 20000 ergibt +5 %");
  pruefeNahe(abweichungProzent(21000.0f, 20000.0f), -5.0, 0.001, "AK-6: R 21000 bei Referenz 20000 ergibt -5 % (Beugen)");
  pruefeNahe(abweichungProzent(20000.0f, 20000.0f), 0.0, 0.001, "AK-6: gleicher Wert ergibt 0 %");
  pruefeNahe(abweichungProzent(0.0f, 20000.0f), 100.0, 0.001, "AK-6: R 0 ergibt 100 %");
  pruefeNahe(abweichungProzent(40000.0f, 20000.0f), -100.0, 0.001, "AK-6: doppelter Widerstand ergibt -100 %");
  pruefeNahe(abweichungProzent(1234.5f, 100000.0f), (100000.0 - 1234.5) / 100000.0 * 100.0, 0.001,
             "AK-6: Formel stimmt auch bei grosser Referenz");
  pruefeNahe(abweichungProzent(1000.0f, 0.0f), 0.0, 0.001, "AK-6: Referenz 0 ergibt 0.0 (keine Referenz)");
  pruefeNahe(abweichungProzent(1000.0f, -5.0f), 0.0, 0.001, "AK-6: negative Referenz ergibt 0.0");
  pruefeNahe(abweichungProzent(1000.0f, -1.0f), 0.0, 0.001, "AK-6: Referenz -1 ergibt 0.0");
  // Zusammenspiel AK-2 + AK-6: Überstrecken = weniger Biegung = ADC steigt = positive Abweichung
  {
    float referenz = flexWiderstandOhm(2000);
    pruefe(abweichungProzent(flexWiderstandOhm(2100), referenz) > 0.0f, "AK-6: ADC höher als bei der Referenz ergibt positive Abweichung (Überstreckung)");
    pruefe(abweichungProzent(flexWiderstandOhm(1900), referenz) < 0.0f, "AK-6: ADC tiefer als bei der Referenz ergibt negative Abweichung (Beugen)");
  }

  // ---------------------------------------------------------------- AK-4: Ausfall
  std::printf("AK-4: Ausfall nach 3 ungültigen Werten in Folge, zurück nach 3 gültigen\n");
  {
    AusfallZustand z{};
    pruefe(!fuettere(z, 1, true), "AK-4: am Anfang (gültiger Wert) kein Ausfall");
    pruefe(!ausfallAktualisieren(z, false), "AK-4: 1. ungültiger Wert: noch kein Ausfall");
    pruefe(!ausfallAktualisieren(z, false), "AK-4: 2. ungültiger Wert: noch kein Ausfall");
    pruefe(ausfallAktualisieren(z, false), "AK-4: 3. ungültiger Wert in Folge: Ausfall");
    pruefe(ausfallAktualisieren(z, false), "AK-4: 4. ungültiger Wert: Ausfall bleibt");
    pruefe(ausfallAktualisieren(z, true), "AK-4: 1. gültiger Wert danach: Ausfall bleibt");
    pruefe(ausfallAktualisieren(z, true), "AK-4: 2. gültiger Wert danach: Ausfall bleibt");
    pruefe(!ausfallAktualisieren(z, true), "AK-4: 3. gültiger Wert in Folge: Ausfall vorbei");
    pruefe(!fuettere(z, 50, true), "AK-4: viele gültige Werte: weiter kein Ausfall");
  }
  {
    AusfallZustand z{};
    pruefe(!fuettere(z, 2, false), "AK-4: 2 ungültige: kein Ausfall");
    pruefe(!ausfallAktualisieren(z, true), "AK-4: dann 1 gültiger: kein Ausfall");
    pruefe(!fuettere(z, 2, false), "AK-4: dann 2 ungültige (nicht in Folge mit den ersten): kein Ausfall");
    pruefe(ausfallAktualisieren(z, false), "AK-4: der 3. ungültige in Folge löst den Ausfall aus");
  }
  {
    AusfallZustand z{};
    fuettere(z, 3, false);
    pruefe(ausfallAktualisieren(z, true), "AK-4: im Ausfall: gültig 1");
    pruefe(ausfallAktualisieren(z, true), "AK-4: im Ausfall: gültig 2");
    pruefe(ausfallAktualisieren(z, false), "AK-4: im Ausfall: ein ungültiger dazwischen: weiter Ausfall");
    pruefe(ausfallAktualisieren(z, true), "AK-4: im Ausfall: gültig 1 (Zähler neu)");
    pruefe(ausfallAktualisieren(z, true), "AK-4: im Ausfall: gültig 2 (Zähler neu): immer noch Ausfall");
    pruefe(!ausfallAktualisieren(z, true), "AK-4: im Ausfall: gültig 3 in Folge: vorbei");
  }
  {
    AusfallZustand z{};
    pruefe(fuettere(z, 1000, false), "AK-4: sehr langer Ausfall: bleibt Ausfall");
    pruefe(!fuettere(z, 3, true), "AK-4: nach sehr langem Ausfall genügen 3 gültige Werte");
  }
  {
    AusfallZustand a{};
    AusfallZustand b{};
    fuettere(a, 3, false);
    pruefe(!fuettere(b, 2, false), "AK-4: zwei Zustände stören sich nicht (b hat nur 2 ungültige)");
    pruefe(ausfallAktualisieren(a, false), "AK-4: Zustand a bleibt im Ausfall, unabhängig von b");
  }

  // ---------------------------------------------------------------- Fuzz
  std::printf("Fuzz: extreme und unsinnige Werte, nichts stürzt ab (B-2)\n");
  {
    bool allesInOrdnung = true;
    const int extreme[] = {0, 1, -1, 99, 100, 101, 3994, 3995, 3996, 4095, 4096, 65535, -65536, INT_MAX, INT_MIN};
    for (unsigned i = 0; i < sizeof(extreme) / sizeof(extreme[0]); i++) {
      float r = flexWiderstandOhm(extreme[i]);
      bool gueltig = flexWertGueltig(extreme[i]);
      if (gueltig && !(std::isfinite(r) && r > 0.0f)) allesInOrdnung = false;
      if (!gueltig && !(r < 0.0f)) allesInOrdnung = false;
    }
    for (int lauf = 0; lauf < 200000; lauf++) {
      int adc = zufallInt(-100000, 100000);
      float r = flexWiderstandOhm(adc);
      bool gueltig = flexWertGueltig(adc);
      if (gueltig && !(std::isfinite(r) && r > 0.0f)) allesInOrdnung = false;
      if (!gueltig && !(r < 0.0f)) allesInOrdnung = false;
    }
    pruefe(allesInOrdnung, "Fuzz: gültiger ADC gibt endlichen Widerstand > 0, ungültiger gibt negativen Wert (-1.0)");
  }
  {
    // Abweichung mit Unsinn: kein Absturz. Ergebnis bei endlichen, vernünftigen Werten bleibt endlich.
    const float seltsam[] = {NAN, INFINITY, -INFINITY, 0.0f, -0.0f, 1e30f, -1e30f, 1e-30f, -1.0f, 1.0f, 20000.0f};
    const int n = sizeof(seltsam) / sizeof(seltsam[0]);
    bool endlichBeiEndlich = true;
    for (int a = 0; a < n; a++) {
      for (int b = 0; b < n; b++) {
        float erg = abweichungProzent(seltsam[a], seltsam[b]);
        if (std::isfinite(seltsam[a]) && std::isfinite(seltsam[b]) && std::fabs(seltsam[a]) < 1e20f &&
            std::fabs(seltsam[b]) < 1e20f && !std::isfinite(erg)) {
          endlichBeiEndlich = false;
        }
      }
    }
    for (int lauf = 0; lauf < 100000; lauf++) {
      abweichungProzent(static_cast<float>(zufall(-1e6, 1e6)), static_cast<float>(zufall(-1e6, 1e6)));
    }
    pruefe(endlichBeiEndlich, "Fuzz: Abweichung bei endlichen Werten ist endlich, bei NaN/unendlich kein Absturz");
  }
  {
    // Fail-safe: ein NaN-Widerstand oder NaN-Referenz darf nie wie "Überstreckung" aussehen, die über jedem Grenzwert liegt.
    float a1 = abweichungProzent(NAN, 20000.0f);
    float a2 = abweichungProzent(20000.0f, NAN);
    pruefe(!(a1 > 0.0f), "Fuzz (B-2): NaN-Widerstand ergibt keine positive Abweichung (kein Alarm aus NaN)");
    pruefe(!(a2 > 0.0f), "Fuzz (B-2): NaN-Referenz ergibt keine positive Abweichung (kein Alarm aus NaN)");
  }
  {
    // Fuzz Ausfall: Zufallsfolge. Regel aus AK-4: 3 ungültige in Folge = Ausfall, 3 gültige in Folge = kein Ausfall,
    // sonst ändert sich nichts.
    AusfallZustand z{};
    bool vorher = false;
    int ungueltigInFolge = 0;
    int gueltigInFolge = 0;
    bool regelOk = true;
    for (int lauf = 0; lauf < 200000; lauf++) {
      bool gueltig = (std::rand() % 3) != 0;   // meist gültig, oft ungültig
      if (lauf % 5000 > 4000) gueltig = false;  // zwischendurch lange Ausfälle
      bool jetzt = ausfallAktualisieren(z, gueltig);
      if (gueltig) { gueltigInFolge++; ungueltigInFolge = 0; } else { ungueltigInFolge++; gueltigInFolge = 0; }
      if (ungueltigInFolge >= 3 && !jetzt) regelOk = false;
      else if (gueltigInFolge >= 3 && jetzt) regelOk = false;
      else if (ungueltigInFolge < 3 && gueltigInFolge < 3 && jetzt != vorher) regelOk = false;
      vorher = jetzt;
    }
    pruefe(regelOk, "Fuzz AK-4: 200000 Zufallswerte, Ausfall-Regel (3 in Folge) stimmt immer");
  }

  return testErgebnis();
}
