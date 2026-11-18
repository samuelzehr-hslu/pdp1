// test_REQ-008_warnen.cpp  (Prototyp 2 Flex)
// Prüft aus REQ-008: AK-7 (Grenzwert begrenzen), AK-8 (Entprellung 80 ms, Hysterese 1 Prozentpunkt, Latenz),
// AK-9 (kurze Überschreitung), AK-10 (ohne Referenz keine Warnung), AK-4/AK-5/AK-10 (motorSollAn),
// dazu Physik-Annahmen der REQ (Rauschen +-10 ADC, Beugen/Strecken 2 Hz) und Fuzz.
// Bedrohungen: B-2 (falsche Sensordaten), B-8 (Sensor fällt aus).
//
// Wie wird getestet? grenzwertPruefen() wird alle 20 ms aufgerufen (Messzyklus). "ms" = Zeit seit dem ersten Aufruf.
// Regeln aus der REQ (nach Klärung 2026-11-17): "über dem Grenzwert" heisst echt grösser (>). Die Entprellung zählt ab
// der ersten Messung über dem Grenzwert, der Motor geht bei der ersten Messung an, bei der seither >= 80 ms vergangen
// sind (bei 20-ms-Zyklus also genau bei ms = 80). Eine Messung nicht über dem Grenzwert setzt die Entprellung zurück.
// Ausschalten per Hysterese (Abweichung < Grenzwert - Hysterese) passiert sofort, in derselben Messung.
// NaN und unendlich lösen nie Alarm aus und schalten einen laufenden Motor aus.

#include "test_hilfe.h"
#include "logik.h"
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>

typedef std::function<float(uint32_t)> AbwFkt;   // Abweichung in Prozent zur Zeit "ms seit Start"

static const float GRENZ = 5.0f;        // Vorgabe aus AK-7
static const float HYST = 1.0f;         // AK-8
static const uint32_t ENTPRELL = 80;    // AK-8

// Spielt alle 20 ms; Eintrag i des Ergebnisses = Antwort beim Aufruf bei ms = 20 * i.
static std::vector<bool> spielen(GrenzwertZustand& z, uint32_t t0, uint32_t dauerMs, const AbwFkt& f,
                                 float grenz, float hyst, uint32_t entprell) {
  std::vector<bool> v;
  for (uint32_t ms = 0; ms <= dauerMs; ms += 20) {
    v.push_back(grenzwertPruefen(z, f(ms), t0 + ms, grenz, hyst, entprell));
  }
  return v;
}

static std::vector<bool> spielenStandard(uint32_t t0, uint32_t dauerMs, const AbwFkt& f) {
  GrenzwertZustand z{};
  return spielen(z, t0, dauerMs, f, GRENZ, HYST, ENTPRELL);
}

// Erster Zeitpunkt (ms) ab "abMs", an dem true kam, sonst -1.
static int ersteWahrAb(const std::vector<bool>& v, int abMs) {
  for (size_t i = 0; i < v.size(); i++) {
    int ms = static_cast<int>(i) * 20;
    if (ms >= abMs && v[i]) return ms;
  }
  return -1;
}

static bool alleWahr(const std::vector<bool>& v, int vonMs, int bisMs) {
  for (size_t i = 0; i < v.size(); i++) {
    int ms = static_cast<int>(i) * 20;
    if (ms >= vonMs && ms <= bisMs && !v[i]) return false;
  }
  return true;
}

static bool alleFalsch(const std::vector<bool>& v, int vonMs, int bisMs) {
  for (size_t i = 0; i < v.size(); i++) {
    int ms = static_cast<int>(i) * 20;
    if (ms >= vonMs && ms <= bisMs && v[i]) return false;
  }
  return true;
}

// ---------- Nachbau eines Messzyklus aus den echten Logik-Funktionen (für die Physik-Tests) ----------
struct Geraet {
  AusfallZustand ausfall;
  GrenzwertZustand grenze;
  float referenz;
};

// Ein Zyklus: ADC -> gültig? -> Ausfall? -> Widerstand -> Abweichung -> Grenzwert -> Motor. Liefert "Motor soll an".
static bool zyklus(Geraet& g, int adc, uint32_t t, bool* ausfallOut) {
  bool gueltig = flexWertGueltig(adc);
  bool ausfall = ausfallAktualisieren(g.ausfall, gueltig);
  bool zuWeit = false;
  if (gueltig) {
    float abw = abweichungProzent(flexWiderstandOhm(adc), g.referenz);
    zuWeit = grenzwertPruefen(g.grenze, abw, t, GRENZ, HYST, ENTPRELL);
  }
  if (ausfallOut != nullptr) *ausfallOut = ausfall;
  return motorSollAn(ausfall, g.referenz > 0.0f, false, zuWeit);
}

int main() {
  std::srand(42);

  // ---------------------------------------------------------------- AK-7: Grenzwert begrenzen
  std::printf("AK-7: Grenzwert wird auf 0 bis 20 %% begrenzt\n");
  pruefeNahe(grenzwertBegrenzen(5.0f), 5.0, 0.001, "AK-7: 5 bleibt 5 (Vorgabe)");
  pruefeNahe(grenzwertBegrenzen(0.0f), 0.0, 0.001, "AK-7: 0 bleibt 0");
  pruefeNahe(grenzwertBegrenzen(20.0f), 20.0, 0.001, "AK-7: 20 bleibt 20");
  pruefeNahe(grenzwertBegrenzen(1.0f), 1.0, 0.001, "AK-7: 1 bleibt 1");
  pruefeNahe(grenzwertBegrenzen(13.0f), 13.0, 0.001, "AK-7: 13 bleibt 13");
  pruefeNahe(grenzwertBegrenzen(5.4f), 5.0, 0.001, "AK-7: 5,4 wird auf ganze Prozent gerundet: 5");
  pruefeNahe(grenzwertBegrenzen(5.5f), 6.0, 0.001, "AK-7: 5,5 wird auf ganze Prozent gerundet: 6");
  pruefeNahe(grenzwertBegrenzen(5.6f), 6.0, 0.001, "AK-7: 5,6 wird 6");
  pruefeNahe(grenzwertBegrenzen(12.3f), 12.0, 0.001, "AK-7: 12,3 wird 12");
  pruefeNahe(grenzwertBegrenzen(0.4f), 0.0, 0.001, "AK-7: 0,4 wird 0");
  pruefeNahe(grenzwertBegrenzen(19.6f), 20.0, 0.001, "AK-7: 19,6 wird 20");
  pruefeNahe(grenzwertBegrenzen(20.4f), 20.0, 0.001, "AK-7: 20,4 wird 20 (begrenzt)");
  pruefeNahe(grenzwertBegrenzen(-0.4f), 0.0, 0.001, "AK-7: -0,4 wird 0 (begrenzt)");
  pruefeNahe(grenzwertBegrenzen(21.0f), 20.0, 0.001, "AK-7: 21 wird 20");
  pruefeNahe(grenzwertBegrenzen(25.0f), 20.0, 0.001, "AK-7: 25 wird 20");
  pruefeNahe(grenzwertBegrenzen(1000.0f), 20.0, 0.001, "AK-7: 1000 wird 20");
  pruefeNahe(grenzwertBegrenzen(-1.0f), 0.0, 0.001, "AK-7: -1 wird 0");
  pruefeNahe(grenzwertBegrenzen(-50.0f), 0.0, 0.001, "AK-7: -50 wird 0");
  pruefeNahe(grenzwertBegrenzen(-INFINITY), 0.0, 0.001, "AK-7: minus unendlich wird 0");
  pruefeNahe(grenzwertBegrenzen(INFINITY), 20.0, 0.001, "AK-7: plus unendlich wird 20");
  pruefeNahe(grenzwertBegrenzen(NAN), 5.0, 0.001, "AK-7/B-2: ungültige Zahl (NaN) ergibt die Vorgabe 5 %");

  // ---------------------------------------------------------------- motorSollAn
  std::printf("AK-4, AK-5, AK-10: Motor nur bei zuWeit UND Referenz UND kein Ausfall UND kein Einmessen\n");
  {
    int falsch = 0;
    for (int m = 0; m < 16; m++) {
      bool ausfall = (m & 1) != 0;
      bool referenzDa = (m & 2) != 0;
      bool einmessen = (m & 4) != 0;
      bool zuWeit = (m & 8) != 0;
      bool soll = zuWeit && referenzDa && !ausfall && !einmessen;
      if (motorSollAn(ausfall, referenzDa, einmessen, zuWeit) != soll) falsch++;
    }
    pruefe(falsch == 0, "alle 16 Kombinationen von motorSollAn stimmen");
  }
  pruefe(motorSollAn(false, true, false, true), "AK-8: zuWeit, Referenz da, kein Ausfall, kein Einmessen: Motor an");
  pruefe(!motorSollAn(true, true, false, true), "AK-4: bei Sensor-Ausfall ist der Motor aus (fail-safe)");
  pruefe(!motorSollAn(false, false, false, true), "AK-10: ohne Referenz keine Warnung");
  pruefe(!motorSollAn(false, true, true, true), "AK-5: während des Einmessens warnt der Motor nicht");
  pruefe(!motorSollAn(false, true, false, false), "AK-8: nicht zu weit: Motor aus");

  // ---------------------------------------------------------------- AK-8: Entprellung 80 ms
  std::printf("AK-8: Abweichung länger als 80 ms über dem Grenzwert: Motor an (Latenz höchstens 100 ms)\n");
  {
    // Erster Aufruf überhaupt liegt schon über dem Grenzwert, grosse Zeit (deckt "Startzeit 0" Fehler auf)
    std::vector<bool> v = spielenStandard(1000000, 600, [](uint32_t) { return 10.0f; });
    int an = ersteWahrAb(v, 0);
    pruefe(an == 80, "AK-8: konstant 10 %: Motor an bei der ersten Messung, die >= 80 ms nach der ersten Überschreitung liegt (ms = 80)");
    pruefe(alleFalsch(v, 0, 60), "AK-8: in den ersten 60 ms bleibt der Motor aus (Entprellung)");
    pruefe(an >= 0 && alleWahr(v, an, 600), "AK-8: danach bleibt der Motor an");
  }
  {
    // zuerst ruhig (0 %), dann Sprung auf 10 %
    std::vector<bool> v = spielenStandard(5000, 1000, [](uint32_t ms) { return ms < 400 ? 0.0f : 10.0f; });
    int an = ersteWahrAb(v, 0);
    pruefe(an == 400 + 80, "AK-8: Sprung von 0 auf 10 %: Motor an genau 80 ms nach dem Sprung (QA-05, höchstens 100 ms)");
    pruefe(alleFalsch(v, 0, 460), "AK-8: vor dem Sprung und in den ersten 60 ms danach: Motor aus");
  }
  {
    // Grenzwert +0,5: knapp darüber
    std::vector<bool> v = spielenStandard(7777, 600, [](uint32_t) { return 5.5f; });
    int an = ersteWahrAb(v, 0);
    pruefe(an == 80, "AK-8: 5,5 % (knapp über Grenzwert 5 %) löst bei ms = 80 aus");
  }
  {
    // "über" heisst echt grösser: genau gleich dem Grenzwert löst nie aus
    std::vector<bool> v = spielenStandard(1000, 1000, [](uint32_t) { return 5.0f; });
    pruefe(alleFalsch(v, 0, 1000), "AK-8: Abweichung genau gleich dem Grenzwert (5,0 %) löst nicht aus (über = echt grösser)");
  }
  {
    std::vector<bool> v = spielenStandard(7777, 1000, [](uint32_t) { return 4.5f; });
    pruefe(alleFalsch(v, 0, 1000), "AK-8: 4,5 % (unter dem Grenzwert 5 %) löst nie aus");
  }

  // ---------------------------------------------------------------- AK-9: kurze Überschreitung
  std::printf("AK-9: Überschreitung kürzer als 80 ms schaltet den Motor nicht ein\n");
  {
    // 3 Werte über dem Grenzwert (0, 20, 40 ms), dann darunter: 60 ms
    std::vector<bool> v = spielenStandard(2000, 1000, [](uint32_t ms) { return ms <= 40 ? 10.0f : 0.0f; });
    pruefe(alleFalsch(v, 0, 1000), "AK-9: 60 ms über dem Grenzwert: Motor bleibt aus");
  }
  {
    std::vector<bool> v = spielenStandard(2000, 1000, [](uint32_t ms) { return ms == 0 ? 10.0f : 0.0f; });
    pruefe(alleFalsch(v, 0, 1000), "AK-9: ein einzelner Messwert über dem Grenzwert: Motor bleibt aus");
  }
  {
    // 20 kurze Impulse hintereinander (je 60 ms über, 100 ms darunter)
    std::vector<bool> v = spielenStandard(3000, 3200, [](uint32_t ms) { return (ms % 160) <= 40 ? 10.0f : 0.0f; });
    pruefe(alleFalsch(v, 0, 3200), "AK-9: 20 kurze Impulse (je 60 ms): Motor bleibt aus, die Zeiten zählen nicht zusammen");
  }
  {
    // Entprellung beginnt neu: 60 ms über, 40 ms darunter, dann dauerhaft darüber
    std::vector<bool> v = spielenStandard(4000, 800, [](uint32_t ms) { return (ms <= 40 || ms >= 100) ? 10.0f : 0.0f; });
    int an = ersteWahrAb(v, 0);
    pruefe(an == 100 + 80, "AK-8/AK-9: nach einer Unterbrechung beginnt die Entprellung neu (Motor an genau 80 ms nach dem Wiedereinstieg)");
  }
  {
    // Zittern an der Grenze, Motor aus: 5,4 / 4,6 im Wechsel (alle 20 ms)
    std::vector<bool> v = spielenStandard(1000, 3000, [](uint32_t ms) { return ((ms / 20) % 2) ? 4.6f : 5.4f; });
    pruefe(alleFalsch(v, 0, 3000), "AK-8: Zittern 5,4 / 4,6 % um den Grenzwert (Motor aus): Motor bleibt aus");
  }

  // ---------------------------------------------------------------- AK-8: Hysterese
  std::printf("AK-8: Motor bleibt an, bis die Abweichung unter Grenzwert - 1 Prozentpunkt fällt\n");
  {
    // 0..200 ms: 10 %, danach 4,5 % (zwischen 4 und 5), ab 1240: 3,5 % (unter 4), ab 1560: wieder 4,5 %, ab 2580: 5,5 %
    AbwFkt f = [](uint32_t ms) -> float {
      if (ms <= 200) return 10.0f;
      if (ms <= 1220) return 4.5f;
      if (ms <= 1540) return 3.5f;
      if (ms <= 2560) return 4.5f;
      return 5.5f;
    };
    std::vector<bool> v = spielenStandard(10000, 3200, f);
    int an = ersteWahrAb(v, 0);
    pruefe(an == 80, "AK-8: Motor geht bei ms = 80 an");
    pruefe(an >= 0 && alleWahr(v, an, 1220), "AK-8: bei 4,5 % (zwischen 4 und 5 %, Hysterese) bleibt der Motor an");
    pruefe(alleFalsch(v, 1240, 2640), "AK-8: bei 3,5 % (unter 4 %) geht der Motor sofort (in derselben Messung) aus und bleibt aus, auch bei 4,5 % (kein Neustart in der Hysterese-Zone)");
    int wieder = ersteWahrAb(v, 2561);
    pruefe(wieder == 2580 + 80, "AK-8: bei 5,5 % geht er erst nach erneuter Entprellung wieder an (genau 80 ms später)");
  }
  {
    // Zittern an der Grenze, Motor an: 5,4 / 4,6 im Wechsel nach dem Einschalten: kein Flattern
    std::vector<bool> v = spielenStandard(1000, 3300, [](uint32_t ms) { return ms <= 300 ? 10.0f : (((ms / 20) % 2) ? 4.6f : 5.4f); });
    int an = ersteWahrAb(v, 0);
    pruefe(an >= 0 && alleWahr(v, an, 3300), "AK-8: Zittern 5,4 / 4,6 % bei eingeschaltetem Motor: er bleibt durchgehend an (kein Flattern)");
  }
  {
    // andere Parameter (die Funktion muss die übergebenen Werte benutzen): Grenzwert 10, Hysterese 2, Entprellung 200 ms
    GrenzwertZustand z{};
    std::vector<bool> v = spielen(z, 100, 2600,
                                  [](uint32_t ms) -> float { return ms < 1000 ? 11.0f : (ms < 2000 ? 9.0f : 7.5f); },
                                  10.0f, 2.0f, 200);
    int an = ersteWahrAb(v, 0);
    pruefe(an == 200, "Parameter: Entprellung 200 ms und Grenzwert 10 % werden benutzt (an bei ms = 200)");
    pruefe(an >= 0 && alleWahr(v, an, 1980), "Parameter: bei 9 % (über 10 - 2 = 8 %) bleibt der Motor an");
    pruefe(alleFalsch(v, 2000, 2600), "Parameter: bei 7,5 % (unter 8 %) geht der Motor sofort aus");
  }
  {
    // Grenzwert 0 %
    GrenzwertZustand z{};
    std::vector<bool> v = spielen(z, 100, 1600,
                                  [](uint32_t ms) -> float { return ms < 500 ? 0.5f : (ms < 1000 ? -0.5f : -2.0f); },
                                  0.0f, 1.0f, 80);
    int an = ersteWahrAb(v, 0);
    pruefe(an == 80, "AK-7: Grenzwert 0 %: schon 0,5 % löst bei ms = 80 aus");
    pruefe(an >= 0 && alleWahr(v, an, 980), "AK-7/AK-8: Grenzwert 0 %: bei -0,5 % (über -1 %) bleibt der Motor an");
    pruefe(alleFalsch(v, 1000, 1600), "AK-7/AK-8: Grenzwert 0 %: bei -2 % geht der Motor sofort aus");
  }
  {
    // Grenzwert 20 %
    GrenzwertZustand z{};
    std::vector<bool> v = spielen(z, 100, 1000, [](uint32_t) -> float { return 19.5f; }, 20.0f, 1.0f, 80);
    pruefe(alleFalsch(v, 0, 1000), "AK-7: Grenzwert 20 %: 19,5 % löst nicht aus");
    GrenzwertZustand z2{};
    std::vector<bool> v2 = spielen(z2, 100, 1000, [](uint32_t) -> float { return 20.5f; }, 20.0f, 1.0f, 80);
    int an = ersteWahrAb(v2, 0);
    pruefe(an == 80, "AK-7: Grenzwert 20 %: 20,5 % löst bei ms = 80 aus");
  }

  // ---------------------------------------------------------------- AK-10: ohne Referenz
  std::printf("AK-10: ohne Referenz (R_referenz = 0) gibt es keine Warnung\n");
  {
    bool nieAn = true;
    const float widerstaende[] = {500.0f, 5000.0f, 20000.0f, 100000.0f};
    for (unsigned i = 0; i < sizeof(widerstaende) / sizeof(widerstaende[0]); i++) {
      float abw = abweichungProzent(widerstaende[i], 0.0f);
      GrenzwertZustand z{};
      std::vector<bool> v = spielen(z, 1000, 1000, [abw](uint32_t) { return abw; }, GRENZ, HYST, ENTPRELL);
      if (!alleFalsch(v, 0, 1000)) nieAn = false;
      if (motorSollAn(false, false, false, true)) nieAn = false;
    }
    pruefe(nieAn, "AK-10: Abweichung ohne Referenz löst nie aus, und motorSollAn ist ohne Referenz immer aus");
  }

  // ---------------------------------------------------------------- Fuzz
  std::printf("Fuzz: NaN, unendlich, extreme Werte: kein Absturz, Motor bleibt aus (B-2)\n");
  {
    // NaN allein: nie an
    std::vector<bool> v = spielenStandard(1000, 100000, [](uint32_t) { return NAN; });
    pruefe(alleFalsch(v, 0, 100000), "Fuzz (B-2): nur NaN: Motor nie an");
  }
  {
    // minus unendlich allein: nie an
    std::vector<bool> v = spielenStandard(1000, 5000, [](uint32_t) { return -INFINITY; });
    pruefe(alleFalsch(v, 0, 5000), "Fuzz (B-2): nur minus unendlich: Motor nie an");
  }
  {
    // plus unendlich allein: nie an (unendlich ist eine kaputte Zahl, kein Alarm)
    std::vector<bool> v = spielenStandard(1000, 5000, [](uint32_t) { return INFINITY; });
    pruefe(alleFalsch(v, 0, 5000), "Fuzz (B-2): nur plus unendlich: Motor nie an (kaputte Zahl, kein Alarm)");
  }
  {
    // Motor an, dann plus unendlich: Motor aus
    std::vector<bool> v = spielenStandard(1000, 1500, [](uint32_t ms) { return ms < 300 ? 10.0f : INFINITY; });
    pruefe(alleFalsch(v, 400, 1500), "Fuzz (B-2): Motor an, dann nur plus unendlich: Motor ist nach spätestens 100 ms aus und bleibt aus");
  }
  {
    // Motor an, dann NaN: Motor muss innerhalb von 100 ms aus sein (fail-safe) und bleiben
    std::vector<bool> v = spielenStandard(1000, 1500, [](uint32_t ms) { return ms < 300 ? 10.0f : NAN; });
    int an = ersteWahrAb(v, 0);
    pruefe(an >= 0 && an <= 100, "Fuzz (B-2): Motor geht an, bevor die NaN-Werte kommen (Vorbereitung)");
    pruefe(alleFalsch(v, 400, 1500), "Fuzz (B-2): Motor an, dann nur NaN: Motor ist nach spätestens 100 ms aus und bleibt aus (fail-safe)");
  }
  {
    // Zufallsfolge mit Unsinn, vernünftige Parameter: kein Absturz
    const float seltsam[] = {NAN, INFINITY, -INFINITY, 1e30f, -1e30f, 0.0f, -0.0f, 5.0f, 4.0f, 100.0f, -100.0f};
    const int n = sizeof(seltsam) / sizeof(seltsam[0]);
    GrenzwertZustand z{};
    uint32_t t = 4294960000u;   // läuft kurz nach dem Start über (Zeitüberlauf), darf nicht abstürzen
    for (int lauf = 0; lauf < 500000; lauf++) {
      float abw = (std::rand() % 3 == 0) ? seltsam[std::rand() % n] : static_cast<float>(zufall(-50.0, 100.0));
      t += static_cast<uint32_t>(zufallInt(0, 200));
      grenzwertPruefen(z, abw, t, GRENZ, HYST, ENTPRELL);
    }
    pruefe(true, "Fuzz: 500000 Zufallsaufrufe (mit NaN, unendlich, Zeitüberlauf): kein Absturz");
  }
  {
    // Zufallsfolge mit unsinnigen Parametern: kein Absturz
    const float grenzen[] = {NAN, INFINITY, -INFINITY, -1e30f, 1e30f, 0.0f, 5.0f, 20.0f, -5.0f};
    const float hysteresen[] = {NAN, INFINITY, -1.0f, 0.0f, 1.0f, 1e30f};
    const uint32_t entprell[] = {0u, 1u, 80u, 1000u, 4294967295u};
    GrenzwertZustand z{};
    uint32_t t = 0;
    for (int lauf = 0; lauf < 200000; lauf++) {
      float abw = static_cast<float>(zufall(-30.0, 60.0));
      t += static_cast<uint32_t>(zufallInt(0, 100));
      grenzwertPruefen(z, abw, t, grenzen[std::rand() % 9], hysteresen[std::rand() % 6], entprell[std::rand() % 5]);
    }
    pruefe(true, "Fuzz: 200000 Aufrufe mit unsinnigen Parametern (NaN, unendlich, Entprellung 0 bis 2^32-1): kein Absturz");
  }

  // ---------------------------------------------------------------- Physik-Annahmen der REQ
  std::printf("Physik (REQ): Rauschen +-10 ADC-Werte, Exemplare 15 bis 30 kOhm, Beugen und Strecken bis 2 Hz\n");
  const double exemplare[] = {15000, 18000, 22000, 26000, 30000};
  const int anzahlExemplare = sizeof(exemplare) / sizeof(exemplare[0]);
  {
    // Ruhe mit Rauschen: 20 s, kein Alarm, kein Ausfall
    bool keinAlarm = true;
    bool keinAusfall = true;
    for (int e = 0; e < anzahlExemplare; e++) {
      int adcMitte = adcAusOhm(exemplare[e]);
      Geraet g = {};
      g.referenz = flexWiderstandOhm(adcMitte);
      for (int i = 0; i < 1000; i++) {
        bool ausfall = false;
        bool motor = zyklus(g, adcMitte + zufallInt(-10, 10), 1000u + 20u * static_cast<uint32_t>(i), &ausfall);
        if (motor) keinAlarm = false;
        if (ausfall) keinAusfall = false;
      }
    }
    pruefe(keinAlarm, "Physik: Ruhe mit Rauschen +-10 ADC (20 s, 5 Exemplare): kein Fehlalarm bei Grenzwert 5 %");
    pruefe(keinAusfall, "Physik: Ruhe mit Rauschen: kein Sensor-Ausfall");
  }
  {
    // Überstreckung +10 % (R sinkt um 10 %): Motor an innerhalb von 100 ms nach dem Sprung (QA-05)
    bool schnellGenug = true;
    bool nichtZuFrueh = true;
    for (int e = 0; e < anzahlExemplare; e++) {
      int adcRuhe = adcAusOhm(exemplare[e]);
      int adcSprung = adcAusOhm(exemplare[e] * 0.90);
      Geraet g = {};
      g.referenz = flexWiderstandOhm(adcRuhe);
      int sprungMs = 1000;
      int erstesAn = -1;
      for (int i = 0; i < 100; i++) {
        int ms = i * 20;
        int basis = (ms < sprungMs) ? adcRuhe : adcSprung;
        bool motor = zyklus(g, basis + zufallInt(-10, 10), 1000u + static_cast<uint32_t>(ms), nullptr);
        if (motor && erstesAn < 0) erstesAn = ms;
      }
      if (erstesAn < 0 || erstesAn > sprungMs + 100) schnellGenug = false;
      if (erstesAn >= 0 && erstesAn < sprungMs + 80) nichtZuFrueh = false;
    }
    pruefe(schnellGenug, "Physik/QA-05: 10 % Überstreckung (bei Rauschen): Motor an höchstens 100 ms nach dem Sprung (Regel: genau 80 ms)");
    pruefe(nichtZuFrueh, "Physik/AK-8: ... aber nicht früher als 80 ms nach dem Sprung (Entprellung)");
  }
  {
    // Beugen und Strecken mit 2 Hz bis auf das 2,5-fache des Widerstands (bis ca. -100 Grad): kein Alarm
    bool keinAlarm = true;
    for (int e = 0; e < anzahlExemplare; e++) {
      int adcRuhe = adcAusOhm(exemplare[e]);
      Geraet g = {};
      g.referenz = flexWiderstandOhm(adcRuhe);
      for (int i = 0; i < 500; i++) {
        double s = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * 2.0 * (i * 0.02));   // 0 = gestreckt, 1 = stark gebeugt
        double ohm = exemplare[e] * (1.0 + 1.5 * s);
        bool motor = zyklus(g, adcAusOhm(ohm) + zufallInt(-10, 10), 1000u + 20u * static_cast<uint32_t>(i), nullptr);
        if (motor) keinAlarm = false;
      }
    }
    pruefe(keinAlarm, "Physik: Beugen und Strecken mit 2 Hz (10 s, Rauschen): kein Fehlalarm");
  }
  {
    // Sensor fällt aus (Kabel ab: ADC = 0), während der Motor läuft (AK-4): Motor innerhalb von 100 ms aus, danach wieder normal
    bool abgeschaltet = true;
    bool bleibtAus = true;
    bool wiederDa = true;
    for (int e = 0; e < anzahlExemplare; e++) {
      int adcRuhe = adcAusOhm(exemplare[e]);
      int adcUeber = adcAusOhm(exemplare[e] * 0.90);
      Geraet g = {};
      g.referenz = flexWiderstandOhm(adcRuhe);
      int ausfallStart = 1000;     // ab hier ADC = 0
      int ausfallEnde = 1600;      // ab hier wieder gültige Werte
      int wiederAn = -1;
      for (int i = 0; i < 150; i++) {
        int ms = i * 20;
        int adc = (ms >= ausfallStart && ms < ausfallEnde) ? 0 : adcUeber;
        bool motor = zyklus(g, adc, 1000u + static_cast<uint32_t>(ms), nullptr);
        if (ms >= ausfallStart + 100 && ms < ausfallEnde && motor) bleibtAus = false;
        if (ms == ausfallStart + 100 && motor) abgeschaltet = false;
        if (ms >= ausfallEnde && motor && wiederAn < 0) wiederAn = ms;
        if (ms == ausfallStart - 20 && !motor) abgeschaltet = false;   // vorher muss der Motor an gewesen sein
      }
      if (wiederAn < 0 || wiederAn > ausfallEnde + 300) wiederDa = false;
    }
    pruefe(abgeschaltet, "AK-4: Sensor fällt aus (ADC 0) während der Motor läuft: Motor nach höchstens 100 ms aus (vorher war er an)");
    pruefe(bleibtAus, "AK-4: solange der Sensor ausgefallen ist, bleibt der Motor aus");
    pruefe(wiederDa, "AK-4: nach 3 gültigen Werten läuft alles normal weiter (Referenz bleibt), Motor wieder an innerhalb von 300 ms");
  }

  // ---------------------------------------------------------------- Zusatz (nicht Teil der Akzeptanzkriterien)
  std::printf("Zusatz: Dinge, die die REQ offen lässt. Können nicht zu 'nicht bestanden' führen.\n");
  {
    std::vector<bool> v = spielenStandard(4294967200u, 600, [](uint32_t) { return 10.0f; });
    int an = ersteWahrAb(v, 0);
    zusatz(an == 80, "Zeitüberlauf (millis läuft nach 49 Tagen über): Entprellung klappt auch über den Überlauf");
  }
  {
    // Motor an, dann ein einziger Wert unter Grenzwert - 1 Prozentpunkt: sofort aus, danach neue Entprellung (AK-8, nach Klärung)
    std::vector<bool> v = spielenStandard(1000, 1000, [](uint32_t ms) { return ms == 500 ? 0.0f : 10.0f; });
    pruefe(alleWahr(v, 80, 480), "AK-8: Motor ist vor dem Ausreisser an");
    pruefe(!v[25], "AK-8: ein einzelner Messwert unter Grenzwert - 1 Prozentpunkt schaltet den Motor sofort aus (ms = 500)");
    pruefe(alleFalsch(v, 500, 580), "AK-8: danach beginnt die Entprellung neu (aus bis ms = 580)");
    pruefe(ersteWahrAb(v, 520) == 600, "AK-8: ... und der Motor geht 80 ms nach dem ersten neuen Wert über dem Grenzwert wieder an (ms = 600)");
  }

  return testErgebnis();
}
