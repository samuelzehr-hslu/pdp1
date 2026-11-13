// test_REQ-005.cpp
// Tests zu REQ-005 "Einfaches, verzeihendes Einmessen (still halten, dann frei bewegen)", Teil 1 (Kern, Sicherheit B-6).
// Teil 2 (AK-11 Reihenfolge egal, AK-12 gleichzeitig, AK-18 Wiederholgenauigkeit) steht in test_REQ-005_frei.cpp.
//
// Was hier geprüft wird (Logik auf dem PC, nur aus der Anforderung, ohne den Code zu kennen):
//   AK-3  : Ruhe = Drehgeschwindigkeit (Länge) <= 10 Grad/s bei BEIDEN IMUs, 2000 ms (+-100) am Stück, Wackeln = neu zählen,
//           kein Abbruch durch die Logik, Referenz nur aus der ruhigen Zeit
//   AK-4  : Zahl ruhigSekunden (Grundlage der Anzeige "Ruhig: x.x s")
//   AK-8  : Fortschritt: Beugung (Spannweite), Oberarm (kleinere der zwei Richtungen), Hand, Achse; Werte werden nie kleiner
//   AK-10 : Erfolg nur, wenn alle vier Werte OK sind (und genau dann); Referenz stimmt, Winkel danach <= 2,5 Grad
//   AK-13 : Abbruchgrund = erster nicht erfüllter Wert (Beugung -> Oberarm -> Hand), einmessenGrund nach dem Abbruch
//   AK-14 : nie Erfolg ohne Oberarm-/Handbewegung (<= 3 Grad), Beugung nie "geraten"
//   AK-15 : Abbruch (IMU) in Schritt 1, Warte-Zustand, Schritt 2: alte Referenz und Achse bleiben, sofort wieder bereit
//   Fuzz  : kurz, NaN/unendlich/riesig, Aufrufe in falscher Reihenfolge (B-1, B-2, B-6)
// AK-14 "Schieflage bis 10 Grad": test_REQ-002_schief.cpp (läuft jetzt mit dem neuen Einmess-Ablauf, siehe sim_ellbogen.h).
// NICHT hier prüfbar (steckt in jointsense.ino, siehe docs/tests/board-test-REQ-005.md): AK-1, AK-2, AK-4 (Textzeile), AK-5 bis
// AK-7 (Zeiten, Warte-Stoss), AK-9 (Textzeile), AK-15/16 (Motor, Texte), AK-17 (Zykluszeit, Speicher).
// Bei AK-13 wird der 30-s-Abbruch von der Hardware-Seite ausgelöst; hier wird nach 30 s Simulationszeit abgebrochen,
// so wie das .ino es tun soll (wartezeitAbgelaufen ist in test_REQ-004.cpp geprüft).
//
// Die Winkel-Logik wird nicht nachgebaut: der Simulator (sim_ellbogen.h, frei_hilfe.h) erzeugt nur Sensorwerte.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"
#include "frei_hilfe.h"

// "Zusatz": Annahme des Testers, die nicht wörtlich in der REQ steht. Zählt NICHT als Fehler.
void zusatz(bool bedingung, const char *text) {
  if (bedingung) {
    std::printf("  Zusatz ok      %s\n", text);
  } else {
    std::printf("  Zusatz HINWEIS %s (zählt nicht als Fehler)\n", text);
  }
}

// ======================================================================
// AK-3 / AK-4: Ruhe (direkt mit selbst gebauten Messwerten, 50 Hz)
// ======================================================================

ImuMessung ruhigeMessung() {
  ImuMessung m = ImuMessung();
  m.az = 1.0f;   // liegt ruhig, Schwerkraft in z
  return m;
}

ImuMessung messungMitGyro(float gx, float gy, float gz) {
  ImuMessung m = ruhigeMessung();
  m.gx = gx;
  m.gy = gy;
  m.gz = gz;
  return m;
}

// Zyklen (je 20 ms), bis einmessenStillFertig true wird. -1 = nie innerhalb von maxZyklen.
int zyklenBisFertig(EllbogenZustand &z, const ImuMessung &o, const ImuMessung &u, int maxZyklen) {
  for (int i = 1; i <= maxZyklen; i++) {
    ellbogenAktualisieren(z, o, u, 0.02f);
    if (einmessenStillFertig(z)) return i;
  }
  return -1;
}

void testRuhe() {
  std::printf("\n-- AK-3, AK-4: Ruhe (2000 ms am Stück, beide IMUs <= 10 Grad/s, Wackeln = neu zählen)\n");
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);

  // Vor dem Start gibt es kein "fertig".
  pruefe(!einmessenStillFertig(z), "AK-3: ohne einmessenStart ist Schritt 1 nie fertig");

  // Normalfall: völlig ruhig. Fertig nach 2000 ms (+-100 ms) = 95 bis 105 Zyklen.
  einmessenStart(z);
  int n = zyklenBisFertig(z, ruhigeMessung(), ruhigeMessung(), 300);
  std::printf("          Schritt 1 fertig nach %d Zyklen (%d ms)\n", n, n * 20);
  pruefe(n >= 95 && n <= 105, "AK-3: völlig ruhig: Schritt 1 ist nach 2000 ms (+-100 ms) fertig");
  pruefe(einmessenGrund(z) == EINMESSEN_GRUND_KEINER, "AK-3: kein Abbruch gemeldet, wenn Schritt 1 klappt");

  // Anzeige-Zahl (AK-4): nach 1 s ruhig ca. 1,0 s.
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  for (int i = 0; i < 50; i++) ellbogenAktualisieren(z, ruhigeMessung(), ruhigeMessung(), 0.02f);
  EinmessFortschritt f = EinmessFortschritt();
  einmessenFortschritt(z, f);
  pruefeNahe(f.ruhigSekunden, 1.0, 0.1, "AK-4: nach 1000 ms Ruhe zeigt ruhigSekunden etwa 1,0 s");
  pruefe(f.beugungGrad == 0.0f && f.oberarmGrad == 0.0f && f.handGrad == 0.0f && f.achseProzent == 0,
         "AK-8: in Schritt 1 sind die Werte für Schritt 2 (Beugung, Oberarm, Hand, Achse) 0");

  // Grenze 10 Grad/s ist die LÄNGE des Vektors (gx, gy, gz), ohne Abzug eines Gyro-Fehlers.
  struct Fall {
    const char *name;
    ImuMessung oben;
    ImuMessung unten;
    bool ruhig;
  };
  const Fall faelle[] = {
      {"beide 9,5 Grad/s in einer Achse", messungMitGyro(9.5f, 0, 0), messungMitGyro(0, 9.5f, 0), true},
      {"beide Länge 9,5 (5,5 in allen drei Achsen)", messungMitGyro(5.5f, 5.5f, 5.5f), messungMitGyro(5.5f, 5.5f, 5.5f), true},
      {"konstanter Gyro-Fehler 4 Grad/s je Achse (Länge 6,9) zählt als ruhig", messungMitGyro(4, 4, 4), messungMitGyro(-4, 4, -4), true},
      {"Länge 10,4 (6 in allen drei Achsen), obwohl jede Achse unter 10", messungMitGyro(6, 6, 6), ruhigeMessung(), false},
      {"nur IMU 2 dreht mit 11 Grad/s", ruhigeMessung(), messungMitGyro(11, 0, 0), false},
      {"nur IMU 1 dreht mit 11 Grad/s", messungMitGyro(0, 0, 11), ruhigeMessung(), false},
      {"beide 30 Grad/s", messungMitGyro(30, 0, 0), messungMitGyro(0, 30, 0), false},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    EllbogenZustand zz;
    ellbogenZuruecksetzen(zz);
    einmessenStart(zz);
    int k = zyklenBisFertig(zz, faelle[i].oben, faelle[i].unten, 600);   // 12 s
    char text[200];
    std::snprintf(text, sizeof(text), "AK-3: %s -> %s", faelle[i].name, faelle[i].ruhig ? "ruhig, Schritt 1 wird fertig" : "nicht ruhig, nie fertig");
    pruefe(faelle[i].ruhig ? (k >= 95 && k <= 105) : (k == -1), text);
    if (!faelle[i].ruhig) {
      std::snprintf(text, sizeof(text), "AK-3: %s: trotzdem KEIN Abbruch durch die Logik", faelle[i].name);
      pruefe(einmessenGrund(zz) == EINMESSEN_GRUND_KEINER, text);
    }
  }

  // Ein einziger nicht ruhiger Zyklus setzt die Ruhezeit auf 0 zurück (kein Abbruch).
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  for (int i = 0; i < 75; i++) ellbogenAktualisieren(z, ruhigeMessung(), ruhigeMessung(), 0.02f);   // 1,5 s
  ellbogenAktualisieren(z, messungMitGyro(60, 0, 0), ruhigeMessung(), 0.02f);                       // ein Wackler
  einmessenFortschritt(z, f);
  pruefe(f.ruhigSekunden <= 0.1f, "AK-3/AK-4: ein einziger nicht ruhiger Zyklus setzt die Ruhezeit auf (fast) 0 zurück");
  for (int i = 0; i < 94; i++) ellbogenAktualisieren(z, ruhigeMessung(), ruhigeMessung(), 0.02f);   // 1,88 s ruhig
  pruefe(!einmessenStillFertig(z), "AK-3: nach dem Wackler zählt die Zeit neu: 1,88 s ruhig reichen noch nicht");
  pruefe(einmessenGrund(z) == EINMESSEN_GRUND_KEINER, "AK-3: der Wackler bricht NICHT ab");
  int rest = zyklenBisFertig(z, ruhigeMessung(), ruhigeMessung(), 30);
  pruefe(rest != -1, "AK-3: nach insgesamt 2000 ms Ruhe (ab dem Wackler) ist Schritt 1 fertig");

  // Dauerndes Wackeln: nie fertig, nie Abbruch durch die Logik (der Abbruch nach 30 s gehört zum Ablauf im .ino).
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  bool jemalsFertig = false;
  for (int i = 0; i < 1500; i++) {   // 30 s
    ImuMessung o = (i % 75 == 74) ? messungMitGyro(40, 40, 0) : ruhigeMessung();
    ellbogenAktualisieren(z, o, ruhigeMessung(), 0.02f);
    if (einmessenStillFertig(z)) jemalsFertig = true;
  }
  pruefe(!jemalsFertig, "AK-3: alle 1,5 s ein Wackler (30 s lang): Schritt 1 wird nie fertig");
  pruefe(einmessenGrund(z) == EINMESSEN_GRUND_KEINER, "AK-3: auch nach 30 s Wackeln meldet die Logik selbst keinen Abbruch");

  // Zusatz: ungültige Messungen (NaN) zählen nie als ruhig.
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  ImuMessung kaputt = ruhigeMessung();
  kaputt.ax = NAN;
  bool fertigMitKaputt = false;
  for (int i = 0; i < 500; i++) {
    ellbogenAktualisieren(z, kaputt, ruhigeMessung(), 0.02f);
    if (einmessenStillFertig(z)) fertigMitKaputt = true;
  }
  zusatz(!fertigMitKaputt, "Zusatz: 10 s nur ungültige Messungen (NaN) -> Schritt 1 wird nie fertig (fail-safe)");
}

// ======================================================================
// AK-8: Fortschritt, Zahlen (ohne Sensorfehler, damit die Werte genau sind)
// ======================================================================

// Simuliert Schritt 1 (still), dann den Plan; liefert die Fortschrittswerte kurz nach dem Plan-Ende.
EinmessFortschritt fortschrittNachPlan(const Plan &pl, int einbau, unsigned long long samen) {
  Skript sk;
  sk.basis = grundLage(einbau);
  sk.plan = pl;
  sk.pruefAb = 1e9;
  ArmSim sim(skriptBewegung(sk), einbauNr(einbau), fehlerKeine(), samen);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  int n = 0;
  while (!einmessenStillFertig(z) && n < 300) {
    zyklenOhne(sim, z, 1);
    n++;
  }
  *sk.t2 = sim.t;
  einmessenBewegungStart(z);
  // Wie im Betrieb (REQ-005 Schnittstelle): jede Sekunde erst einmessenAbschliessen, danach einmessenFortschritt.
  // Beugung und Achse stammen aus der letzten Auswertung.
  // Gelesen wird 2000 ms nach dem Ende der Bewegung (AK-8: Beugung steht spätestens dann im Fortschritt, auch wenn Oberarm/Hand nicht bewegt wurden).
  int gesamt = static_cast<int>((pl.ende + 2.0) / sim.dt + 0.5);
  for (int i = 1; i <= gesamt; i++) {
    zyklenOhne(sim, z, 1);
    if (i % 50 == 0 || i == gesamt) {
      if (einmessenAbschliessen(z)) break;   // Erfolg ist in diesen Plänen nicht zu erwarten
    }
  }
  EinmessFortschritt f = EinmessFortschritt();
  einmessenFortschritt(z, f);
  return f;
}

void testFortschrittZahlen() {
  std::printf("\n-- AK-8: Fortschritt (Beugung = Spannweite, Oberarm = kleinere der zwei Richtungen, Hand = Scheitelwert, Achse)\n");
  char text[220];

  {  // Nichts bewegt
    Plan pl;
    planPause(pl, 3.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 1);
    pruefe(f.beugungGrad <= 3.0f && f.oberarmGrad <= 3.0f && f.handGrad <= 3.0f, "AK-8: Arm still in Schritt 2: Beugung, Oberarm, Hand bleiben bei (fast) 0");
    pruefe(f.achseProzent < 100, "AK-8: Arm still in Schritt 2: Achse ist nicht 100 %");
    pruefeNahe(f.ruhigSekunden, 0.0, 0.01, "AK-8: in Schritt 2 ist ruhigSekunden 0 (nicht benutzter Wert)");
  }
  {  // Vor der ersten Auswertung (kein einmessenAbschliessen aufgerufen) sind Beugung und Achse 0 (REQ-005 Schnittstelle)
    Skript sk;
    sk.basis = grundLage(0);
    planTeil(sk.plan, T_BEUGEN, 50.0, 1.0, 2.0);
    sk.pruefAb = 1e9;
    ArmSim sim(skriptBewegung(sk), einbauNr(0), fehlerKeine(), 8);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    einmessenStart(z);
    while (!einmessenStillFertig(z)) zyklenOhne(sim, z, 1);
    *sk.t2 = sim.t;
    einmessenBewegungStart(z);
    zyklenOhne(sim, z, 120);   // 2,4 s Beugen, aber noch keine Auswertung
    EinmessFortschritt f = EinmessFortschritt();
    einmessenFortschritt(z, f);
    pruefe(f.achseProzent == 0, "AK-8: vor der ersten Auswertung in Schritt 2 ist die Achse 0 %");
    pruefe(f.beugungGrad >= 0.0f && f.beugungGrad <= 53.0f, "AK-8: vor der ersten Auswertung darf die Beugung schon aus der vorläufigen Schätzung kommen, aber nie über der echten Spannweite 50 (+3)");
  }
  {  // Beugung 50 Grad, 2 Mal
    Plan pl;
    planTeil(pl, T_BEUGEN, 50.0, 1.0, 2.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 2);
    pruefeNahe(f.beugungGrad, 50.0, 3.0, "AK-8: nur beugen (2 Mal 50 Grad): Beugung steht spätestens 2000 ms nach der Bewegung auf 50 (+-3), Spannweite, nicht der Weg 200");
    pruefe(f.oberarmGrad <= 3.0f && f.handGrad <= 3.0f, "AK-8: nur Beugen: Oberarm und Hand bleiben bei (fast) 0");
    pruefe(f.achseProzent < 100, "AK-8: nur Beugen: Achse ist noch nicht 100 % (Oberarm und Hand fehlen)");
  }
  {  // Beugung 20 Grad: unter 30
    Plan pl;
    planTeil(pl, T_BEUGEN, 20.0, 1.0, 4.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 3);
    pruefeNahe(f.beugungGrad, 20.0, 3.0, "AK-8: nur beugen (4 Mal 20 Grad): Beugung steht spätestens 2000 ms nach der Bewegung auf 20 (+-3), Spannweite, nicht der Weg 80");
  }
  {  // Hand 60 Grad
    Plan pl;
    planTeil(pl, T_HAND, 60.0, 1.0, 1.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 4);
    pruefeNahe(f.handGrad, 60.0, 6.0, "AK-8: Unterarm 60 Grad um die eigene Achse gedreht: Hand ist etwa 60");
    pruefe(f.beugungGrad <= 3.0f, "AK-8: nur Hand drehen: Beugung bleibt bei (fast) 0");
  }
  {  // Hand 30 Grad
    Plan pl;
    planTeil(pl, T_HAND, 30.0, 1.0, 1.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 5);
    pruefeNahe(f.handGrad, 30.0, 4.0, "AK-8: Unterarm 30 Grad gedreht: Hand ist etwa 30 (unter 45, also nicht OK)");
  }
  {  // Oberarm nur in einer Richtung
    Plan pl;
    planTeil(pl, T_OBERARM_VORN, 40.0, 1.0, 2.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 6);
    std::snprintf(text, sizeof(text), "AK-8: Oberarm nur vor und zurück (40 Grad, eine Richtung): Oberarm = kleinere der zwei Richtungen < 15 (ist %.1f)", f.oberarmGrad);
    pruefe(f.oberarmGrad < 15.0f, text);
  }
  {  // Oberarm in beiden Richtungen
    Plan pl;
    planTeil(pl, T_KREIS, 40.0, 1.0, 2.0);
    EinmessFortschritt f = fortschrittNachPlan(pl, 0, 7);
    std::snprintf(text, sizeof(text), "AK-8: Oberarm-Kreis 40 Grad in beiden Richtungen: Oberarm >= 15 (ist %.1f)", f.oberarmGrad);
    pruefe(f.oberarmGrad >= 15.0f, text);
    pruefe(f.oberarmGrad <= 60.0f, "AK-8: Oberarm-Wert ist nicht grösser als die echte Bewegung (+ Reserve)");
    pruefe(f.beugungGrad <= 3.0f && f.handGrad <= 3.0f, "AK-8: nur Oberarm bewegt: Beugung und Hand bleiben bei (fast) 0 (jeder Wert hängt nur von seiner eigenen Bewegung ab)");
  }
}

// Werte werden nie kleiner, solange Schritt 2 läuft: erst grosse Bewegung, dann Ruhe, dann kleine Bewegung.
void testFortschrittNieKleiner() {
  std::printf("\n-- AK-8: die drei Grad-Werte werden nie kleiner (bester bisher erreichter Wert)\n");
  Skript sk;
  sk.basis = grundLage(0);
  planTeil(sk.plan, T_BEUGEN, 70.0, 1.0, 1.0);
  planTeil(sk.plan, T_HAND, 70.0, 1.0, 1.0);
  planTeil(sk.plan, T_KREIS, 30.0, 1.0, 1.0);
  planPause(sk.plan, 3.0);
  planTeil(sk.plan, T_BEUGEN, 10.0, 1.0, 2.0);   // danach nur noch kleine Bewegung
  planTeil(sk.plan, T_HAND, 10.0, 1.0, 2.0);
  sk.pruefAb = 1e9;
  ArmSim sim(skriptBewegung(sk), einbauStandard(), fehlerNurRauschen(), 11);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  while (!einmessenStillFertig(z)) zyklenOhne(sim, z, 1);
  *sk.t2 = sim.t;
  einmessenBewegungStart(z);
  EinmessFortschritt alt = EinmessFortschritt();
  int rueckwaerts = 0;
  int ungueltig = 0;
  Ergebnis dummy = ergebnisLeer();
  for (int i = 0; i < 12; i++) {   // 12 s, jede Sekunde: erst Auswertung, dann Fortschritt (REQ-005 Schnittstelle)
    zyklenOhne(sim, z, 50);
    if (einmessenAbschliessen(z)) break;   // Erfolg ist erlaubt; dann endet der Test
    EinmessFortschritt f = EinmessFortschritt();
    einmessenFortschritt(z, f);
    fortschrittKontrolle(f, alt, dummy);
    alt = f;
  }
  rueckwaerts = dummy.rueckwaerts;
  ungueltig = dummy.ungueltig;
  pruefe(rueckwaerts == 0, "AK-8: Beugung, Oberarm und Hand werden während Schritt 2 nie kleiner");
  pruefe(ungueltig == 0, "AK-8: alle Fortschrittswerte sind endlich, nicht negativ, Achse <= 100");
}

// ======================================================================
// AK-10, AK-3 (Referenz), AK-1..4 aus REQ-002: Stichprobe nach dem Einmessen
// ======================================================================

void testErfolg() {
  std::printf("\n-- AK-10: Erfolg nur mit allen vier Werten OK; danach stimmt der Winkel (Stichprobe REQ-002 AK-1 bis AK-4)\n");
  Skript sk;
  sk.basis = grundLage(1);
  planPause(sk.plan, 0.5);
  planTeil(sk.plan, T_BEUGEN, 50.0, 0.7, 2.0);
  planTeil(sk.plan, T_KREIS, 25.0, 0.7, 1.0);
  planTeil(sk.plan, T_HAND, 60.0, 0.7, 1.0);
  skriptFertig(sk);
  ArmSim sim(skriptBewegung(sk), einbauNr(1), fehlerMitOffset(0), 21);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  Ergebnis e = frei(sim, z, sk);
  std::printf("          Schritt 1: %.2f s, Schritt 2 bis Erfolg: %.2f s, Fehler danach max %.3f Grad\n", e.stillS, e.dauerS2, e.maxFehler);
  pruefe(e.ok, "AK-10: normale Bewegung (2 Mal beugen, Oberarm-Kreis, Hand 60 Grad): Einmessen klappt");
  pruefe(e.stillS >= 1.9 && e.stillS <= 2.1, "AK-3: Schritt 1 dauert 2000 ms (+-100 ms), wenn der Arm von Anfang an ruhig ist");
  pruefe(e.dauerS2 <= 30.0, "AK-11: Schritt 2 klappt innerhalb von 30 s");
  pruefe(e.widerspruch == 0, "AK-10: Erfolg (true) genau dann, wenn Beugung, Oberarm, Hand OK und Achse 100 % sind");
  pruefe(e.fehlenderPasst == 0, "AK-13: einmessenFehlenderGrund ist KEINER genau dann, wenn alle vier Werte OK sind");
  pruefe(e.rueckwaerts == 0 && e.ungueltig == 0, "AK-8: Werte nie kleiner, immer sinnvoll");
  pruefe(e.unbekannt == 0, "AK-10: nach dem Einmessen ist der Winkel bekannt");
  pruefe(e.maxFehler <= 2.5, "AK-10: nach dem Einmessen Winkelfehler <= 2,5 Grad (Stillhalten, Beugen, Hand drehen, Alltag)");
  pruefeNahe(e.mittelStill, 0.0, 2.5, "AK-10: Referenz = Ruhe aus Schritt 1: gestreckter Arm zeigt 0 Grad");
}

// Referenz nur aus der ruhigen Zeit: vorher wird 3 s mit dem Unterarm gewackelt, der Ellbogen steht dabei bei -20 Grad.
void testReferenzNurAusRuhe() {
  std::printf("\n-- AK-3: Referenz (0 Grad) kommt nur aus den 2000 ms ruhiger Zeit, nicht aus dem Wackeln davor\n");
  Skript sk;
  sk.basis = grundLage(0);
  sk.basis.flex = -20.0;   // Ellbogen steht bei -20 Grad, das ist die neue Null
  sk.vorher = [](double t, const Pose &b) -> Pose {
    Pose p = b;
    if (t < 3.0) p.pron += 30.0 * std::sin(2.0 * PI_ZAHL * t);   // wackeln, nach 3 s wieder in Ruhe
    return p;
  };
  planPause(sk.plan, 0.5);
  planTeil(sk.plan, T_BEUGEN, 60.0, 0.8, 2.0);
  planTeil(sk.plan, T_KREIS, 25.0, 0.8, 1.0);
  planTeil(sk.plan, T_HAND, 70.0, 0.8, 1.0);
  skriptFertig(sk);
  ArmSim sim(skriptBewegung(sk), einbauStandard(), fehlerNurRauschen(), 31);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  Ergebnis e = frei(sim, z, sk);
  std::printf("          Schritt 1 dauerte %.2f s\n", e.stillS);
  pruefe(e.stillS >= 4.8, "AK-3: nach 3 s Wackeln wird neu gezählt: Schritt 1 ist frühestens nach 3 s + 1,9 s fertig, ohne Abbruch");
  pruefe(e.ok, "AK-3: nach dem Wackeln klappt das Einmessen");
  pruefe(e.unbekannt == 0 && e.maxFehler <= 2.5, "AK-3/AK-10: Referenz stimmt (-20 Grad ist die Null), Fehler <= 2,5 Grad");
}

// ======================================================================
// AK-13, AK-14: Abbruch nach 30 s, Grund = erster fehlender Wert; nie Erfolg ohne Oberarm-/Handbewegung
// ======================================================================

void abbruchFall(const char *name, const Plan &pl, uint8_t erwartet, int einbau, unsigned long long samen) {
  Skript sk;
  sk.basis = grundLage(einbau);
  sk.plan = pl;
  sk.pruefAb = 1e9;   // keine Alltags-Bewegung am Ende, sonst würde die Hand/Beugung doch noch gesammelt
  ArmSim sim(skriptBewegung(sk), einbauNr(einbau), fehlerMitOffset(einbau % 2), samen);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  Ergebnis e = frei(sim, z, sk);
  char text[260];
  std::snprintf(text, sizeof(text), "AK-13/AK-14: %s: kein Erfolg, auch nicht nach 30 s", name);
  pruefe(!e.ok, text);
  std::snprintf(text, sizeof(text), "AK-13: %s: erster fehlender Wert = Grund %d (ist %d)", name, static_cast<int>(erwartet), static_cast<int>(e.grund));
  pruefe(e.grund == erwartet, text);
  std::snprintf(text, sizeof(text), "AK-13: %s: einmessenGrund meldet nach dem Abbruch denselben Grund", name);
  pruefe(e.grundGespeichert == erwartet, text);
  std::snprintf(text, sizeof(text), "AK-10: %s: nie Erfolg ohne dass alle vier Werte OK sind", name);
  pruefe(e.widerspruch == 0 && e.fehlenderPasst == 0, text);
  // Nach dem Abbruch ohne frühere Referenz: keine Abweichung (nichts gemeldet)
  Messreihe m = laufe(sim, z, 2.0, 0.0);
  std::snprintf(text, sizeof(text), "AK-15: %s: ohne frühere Referenz bleibt es bei 'keine Abweichung'", name);
  pruefe(m.unbekannt == m.zyklen, text);
}

void testAbbruchGruende() {
  std::printf("\n-- AK-13, AK-14: Abbruch nach 30 s mit dem ersten fehlenden Grund (Beugung, Oberarm, Hand)\n");
  const uint8_t B = EINMESSEN_GRUND_BEUGUNG;
  const uint8_t O = EINMESSEN_GRUND_OBERARM;
  const uint8_t H = EINMESSEN_GRUND_HAND;
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 10.0, 0.7, 2.0);
    planTeil(pl, T_KREIS, 25.0, 0.7, 1.0);
    planTeil(pl, T_HAND, 60.0, 0.7, 1.0);
    abbruchFall("Beugung nur 10 Grad (Oberarm und Hand ok)", pl, B, 1, 101);
  }
  {
    Plan pl;
    planPause(pl, 3.0);
    abbruchFall("gar keine Bewegung (alles fehlt: Beugung kommt zuerst)", pl, B, 2, 102);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 20.0, 0.7, 4.0);
    planTeil(pl, T_KREIS, 25.0, 0.7, 1.0);
    abbruchFall("4 Mal 20 Grad beugen (Spannweite 20, Weg 160) und keine Hand", pl, B, 1, 103);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_HAND, 60.0, 0.7, 1.0);
    abbruchFall("Oberarm hängt still (Beugung und Hand ok)", pl, O, 1, 104);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 3.0);
    abbruchFall("nur Beugen (Oberarm und Hand fehlen: Oberarm kommt zuerst)", pl, O, 2, 105);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_KREIS, 3.0, 0.7, 2.0);
    planTeil(pl, T_HAND, 60.0, 0.7, 1.0);
    abbruchFall("Oberarm bewegt sich nur 3 Grad (AK-14: nie Erfolg)", pl, O, 1, 106);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_OBERARM_VORN, 40.0, 0.7, 2.0);
    planTeil(pl, T_HAND, 60.0, 0.7, 1.0);
    abbruchFall("Oberarm nur in EINER Richtung (kleinere der zwei Richtungen zählt)", pl, O, 1, 107);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_KREIS, 25.0, 0.7, 1.0);
    abbruchFall("Hand nicht gedreht (Beugung und Oberarm ok)", pl, H, 1, 108);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_KREIS, 25.0, 0.7, 1.0);
    planTeil(pl, T_HAND, 2.0, 0.7, 2.0);
    abbruchFall("Hand dreht sich nur 2 Grad (AK-14: nie Erfolg)", pl, H, 2, 109);
  }
  {
    Plan pl;
    planTeil(pl, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(pl, T_KREIS, 25.0, 0.7, 1.0);
    planTeil(pl, T_HAND, 30.0, 0.7, 2.0);
    abbruchFall("Hand nur 30 Grad (unter 45)", pl, H, 1, 110);
  }

  // Oberarm nur um die Senkrechte (Schwenken): Erfolg ist nur erlaubt, wenn der Winkel danach stimmt (AK-12/B-6),
  // sonst Abbruch mit Grund Oberarm.
  {
    Skript sk;
    sk.basis = grundLage(1);
    planTeil(sk.plan, T_BEUGEN, 60.0, 0.7, 2.0);
    planTeil(sk.plan, T_SCHWENK, 30.0, 0.7, 2.0);
    planTeil(sk.plan, T_HAND, 60.0, 0.7, 1.0);
    sk.pruefAb = sk.plan.ende + 2.0;
    ArmSim sim(skriptBewegung(sk), einbauNr(1), fehlerMitOffset(0), 111);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    Ergebnis e = frei(sim, z, sk);
    pruefe(!e.ok || (e.unbekannt == 0 && e.maxFehler <= 2.5), "AK-12/AK-14: Oberarm nur geschwenkt (um die Senkrechte): kein Erfolg mit falschem Winkel");
    pruefe(e.ok || e.grund == O, "AK-13: Oberarm nur geschwenkt, kein Erfolg: Grund ist Oberarm (Achse nicht eindeutig)");
  }
}

// Ohne Bewegung (synthetische Messwerte) darf es in 30 s nie einen Erfolg geben.
void testNieErfolgOhneBewegung() {
  std::printf("\n-- AK-14: nie Erfolg, wenn sich nichts bewegt (synthetische ruhige Messwerte)\n");
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  einmessenStart(z);
  zyklenBisFertig(z, ruhigeMessung(), ruhigeMessung(), 105);
  pruefe(einmessenStillFertig(z), "Vorbereitung: Schritt 1 fertig");
  einmessenBewegungStart(z);
  int erfolge = 0;
  for (int i = 1; i <= 1500; i++) {
    ellbogenAktualisieren(z, ruhigeMessung(), ruhigeMessung(), 0.02f);
    if (i % 50 == 0 && einmessenAbschliessen(z)) erfolge++;
  }
  pruefe(erfolge == 0, "AK-14: 30 s ohne jede Bewegung: einmessenAbschliessen liefert nie true");
  pruefe(einmessenFehlenderGrund(z) == EINMESSEN_GRUND_BEUGUNG, "AK-13: ohne Bewegung ist der erste fehlende Grund die Beugung");
  float a = 5.0f;
  pruefe(!ellbogenAbweichung(z, a), "AK-14: ohne Erfolg gibt es keine Abweichung (keine Referenz geraten)");

  // Aufrufe in falscher Reihenfolge auf frischem Zustand
  EllbogenZustand z2;
  ellbogenZuruecksetzen(z2);
  pruefe(!einmessenAbschliessen(z2), "AK-14: einmessenAbschliessen ohne einmessenStart liefert false");
  einmessenBewegungStart(z2);
  pruefe(!einmessenAbschliessen(z2), "AK-14: einmessenBewegungStart ohne Schritt 1, dann Abschliessen: false (nichts geraten)");
  pruefe(!ellbogenAbweichung(z2, a), "AK-14: dabei entsteht keine Referenz");
}

// ======================================================================
// AK-15: Abbruch lässt alte Referenz und Achse stehen, sofort wieder bereit
// ======================================================================

// stufe: 1 = Abbruch in Schritt 1, 2 = im Warte-Zustand, 3 = in Schritt 2. mitAlter: vorher gültig eingemessen.
void abbruchSzenario(int stufe, bool mitAlter, const char *kopf) {
  Bewegung bew = [](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;   // 0 bis 7,5 s: erstes Einmessen
    if (t < 16.0) return p;                                                  // still hängen
    return alltagPose(p, t - 16.0, 70.0);                                    // ab 16 s: Beugen, Hand drehen, Oberarm, Schwenken
  };
  ArmSim sim(bew, einbauStandard(), fehlerNurRauschen(), static_cast<unsigned long long>(700 + stufe));
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  char text[200];
  if (mitAlter) {
    pruefe(einmessenLaufen(sim, z), "Vorbereitung: erstes Einmessen klappt");
  } else {
    zyklenOhne(sim, z, 375);
  }
  zyklenOhne(sim, z, 75);   // bis t = 9 s, still
  einmessenStart(z);
  if (stufe == 1) {
    zyklenOhne(sim, z, 40);
  } else {
    int n = 0;
    while (!einmessenStillFertig(z) && n < 300) {
      zyklenOhne(sim, z, 1);
      n++;
    }
    if (stufe == 2) {
      zyklenOhne(sim, z, 10);
    } else {
      einmessenBewegungStart(z);
      zyklenOhne(sim, z, 25);
    }
  }
  einmessenAbbrechen(z, EINMESSEN_GRUND_IMU);
  std::snprintf(text, sizeof(text), "AK-15 %s: einmessenGrund meldet IMU", kopf);
  pruefe(einmessenGrund(z) == EINMESSEN_GRUND_IMU, text);
  std::snprintf(text, sizeof(text), "AK-15 %s: nach dem Abbruch liefert einmessenAbschliessen nie true", kopf);
  pruefe(!einmessenAbschliessen(z), text);
  Messreihe still = laufe(sim, z, 16.0 - sim.t, 0.0);   // bis t = 16 s
  Messreihe bewegt = laufe(sim, z, 8.0, 0.0);           // Alltag
  if (mitAlter) {
    std::snprintf(text, sizeof(text), "AK-15 %s: alte Referenz bleibt: Winkel bleibt bekannt und stimmt auf 2,5 Grad (Stillhalten)", kopf);
    pruefe(still.unbekannt == 0 && still.maxFehler <= 2.5, text);
    std::snprintf(text, sizeof(text), "AK-15 %s: alte Achse bleibt: Beugen, Hand drehen, Schwenken stimmt auf 2,5 Grad", kopf);
    zeige("Alltag", bewegt);
    pruefe(bewegt.unbekannt == 0 && bewegt.maxFehler <= 2.5, text);
  } else {
    std::snprintf(text, sizeof(text), "AK-15 %s: ohne frühere Referenz bleibt es bei 'keine Abweichung'", kopf);
    pruefe(still.unbekannt == still.zyklen && bewegt.unbekannt == bewegt.zyklen, text);
  }
}

// Nach einem Abbruch sofort wieder bereit: Neustart klappt ohne Zurücksetzen.
void testNeustart() {
  std::printf("\n-- AK-15: nach einem Abbruch sofort wieder bereit (Neustart ohne Zurücksetzen)\n");
  for (int stufe = 1; stufe <= 2; stufe++) {
    Skript sk;
    sk.basis = grundLage(1);
    planPause(sk.plan, 0.5);
    planTeil(sk.plan, T_BEUGEN, 50.0, 0.8, 2.0);
    planTeil(sk.plan, T_KREIS, 25.0, 0.8, 1.0);
    planTeil(sk.plan, T_HAND, 60.0, 0.8, 1.0);
    skriptFertig(sk);
    ArmSim sim(skriptBewegung(sk), einbauNr(1), fehlerMitOffset(1), static_cast<unsigned long long>(800 + stufe));
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    einmessenStart(z);
    if (stufe == 1) {
      zyklenOhne(sim, z, 30);
    } else {
      int n = 0;
      while (!einmessenStillFertig(z) && n < 300) {
        zyklenOhne(sim, z, 1);
        n++;
      }
    }
    einmessenAbbrechen(z, EINMESSEN_GRUND_BESTAETIGUNG);
    pruefe(einmessenGrund(z) == EINMESSEN_GRUND_BESTAETIGUNG, "AK-15: Abbruch mit Grund Bestaetigung wird gemeldet");
    Ergebnis e = frei(sim, z, sk);
    char text[160];
    std::snprintf(text, sizeof(text), "AK-15: nach Abbruch (%s) klappt ein neues Einmessen sofort", stufe == 1 ? "in Schritt 1" : "im Warte-Zustand");
    pruefe(e.ok && e.unbekannt == 0 && e.maxFehler <= 2.5, text);
    pruefe(einmessenGrund(z) == EINMESSEN_GRUND_KEINER, "AK-15: nach erfolgreichem Neustart steht kein alter Abbruchgrund mehr");
  }
  abbruchSzenario(1, true, "Schritt 1, mit alter Referenz");
  abbruchSzenario(2, true, "Warte-Zustand, mit alter Referenz");
  abbruchSzenario(3, true, "Schritt 2, mit alter Referenz");
  abbruchSzenario(1, false, "Schritt 1, ohne alte Referenz");
  abbruchSzenario(3, false, "Schritt 2, ohne alte Referenz");

  // Alle Abbruchgründe lassen sich setzen und lesen
  const uint8_t gruende[6] = {EINMESSEN_GRUND_BEUGUNG, EINMESSEN_GRUND_RUHIG, EINMESSEN_GRUND_IMU,
                              EINMESSEN_GRUND_OBERARM, EINMESSEN_GRUND_HAND, EINMESSEN_GRUND_BESTAETIGUNG};
  int falsch = 0;
  for (int i = 0; i < 6; i++) {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    einmessenStart(z);
    einmessenAbbrechen(z, gruende[i]);
    if (einmessenGrund(z) != gruende[i]) falsch++;
  }
  pruefe(falsch == 0, "AK-15/AK-13: einmessenAbbrechen(grund) und einmessenGrund geben alle sechs Gründe unverändert zurück");
}

// ======================================================================
// Fuzz (kurz): NaN, unendlich, riesige Werte, falsche Reihenfolge
// ======================================================================

float zufallsFloat() {
  int r = std::rand() % 14;
  switch (r) {
    case 0: return NAN;
    case 1: return INFINITY;
    case 2: return -INFINITY;
    case 3: return 1e30f;
    case 4: return -1e30f;
    case 5: return 0.0f;
    case 6: return 1e-30f;
    default: return static_cast<float>(std::rand() % 6001 - 3000) / 1000.0f;
  }
}

ImuMessung zufallsMessung() {
  ImuMessung m = ImuMessung();
  m.ax = zufallsFloat();
  m.ay = zufallsFloat();
  m.az = zufallsFloat();
  m.gx = zufallsFloat();
  m.gy = zufallsFloat();
  m.gz = zufallsFloat();
  return m;
}

ImuMessung plausibleMessung() {
  ImuMessung m = ImuMessung();
  m.ax = static_cast<float>(std::rand() % 2001 - 1000) / 1000.0f;
  m.ay = static_cast<float>(std::rand() % 2001 - 1000) / 1000.0f;
  m.az = static_cast<float>(std::rand() % 2001 - 1000) / 1000.0f;
  m.gx = static_cast<float>(std::rand() % 401 - 200) / 2.0f;
  m.gy = static_cast<float>(std::rand() % 401 - 200) / 2.0f;
  m.gz = static_cast<float>(std::rand() % 401 - 200) / 2.0f;
  return m;
}

void testFuzz() {
  std::printf("\n-- Fuzz: zufällige Reihenfolge der neuen Funktionen, NaN/unendlich/riesig (fester Startwert 42)\n");
  std::srand(42);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  int nichtEndlich = 0;
  int falscheErfolge = 0;
  int ungueltigeFortschritte = 0;
  int nachResetBekannt = 0;
  int erfolge = 0;
  for (int i = 0; i < 30000; i++) {
    int op = std::rand() % 12;
    float a = 0.0f;
    if (op == 0) {
      ellbogenZuruecksetzen(z);
      if (ellbogenAbweichung(z, a)) nachResetBekannt++;
    } else if (op == 1) {
      einmessenStart(z);
    } else if (op == 2) {
      einmessenBewegungStart(z);
    } else if (op == 3) {
      // Reihenfolge laut REQ-005: erst Abschliessen, dann Fortschritt
      bool klappt = einmessenAbschliessen(z);
      if (klappt) {
        erfolge++;
        if (einmessenFehlenderGrund(z) != EINMESSEN_GRUND_KEINER) falscheErfolge++;   // Erfolg nur mit allen vier OK
      } else {
        EinmessFortschritt f = EinmessFortschritt();
        einmessenFortschritt(z, f);
        if (alleOk(f)) falscheErfolge++;   // alle vier OK, aber kein Erfolg
      }
    } else if (op == 4) {
      einmessenAbbrechen(z, static_cast<uint8_t>(std::rand() % 256));
    } else if (op == 5) {
      EinmessFortschritt f = EinmessFortschritt();
      einmessenFortschritt(z, f);
      if (!std::isfinite(f.ruhigSekunden) || !std::isfinite(f.beugungGrad) || !std::isfinite(f.oberarmGrad) ||
          !std::isfinite(f.handGrad) || f.beugungGrad < 0.0f || f.oberarmGrad < 0.0f || f.handGrad < 0.0f || f.achseProzent > 100) {
        ungueltigeFortschritte++;
      }
    } else if (op == 6) {
      einmessenStillFertig(z);
    } else if (op == 7) {
      einmessenFehlenderGrund(z);
      einmessenGrund(z);
    } else if (op == 8) {
      ellbogenLageVerwerfen(z);
    } else {
      ImuMessung o = (std::rand() % 4 == 0) ? zufallsMessung() : plausibleMessung();
      ImuMessung u = (std::rand() % 4 == 0) ? zufallsMessung() : plausibleMessung();
      float dt = (std::rand() % 10 == 0) ? zufallsFloat() : 0.02f;
      ellbogenAktualisieren(z, o, u, dt);
    }
    if (ellbogenAbweichung(z, a) && !std::isfinite(a)) nichtEndlich++;
  }
  pruefe(true, "Fuzz: 30000 Aufrufe in zufälliger Reihenfolge, kein Absturz");
  pruefe(nachResetBekannt == 0, "Fuzz: nach ellbogenZuruecksetzen gibt es nie eine Abweichung");
  pruefe(nichtEndlich == 0, "Fuzz: gemeldete Abweichung ist nie NaN oder unendlich");
  pruefe(ungueltigeFortschritte == 0, "Fuzz: Fortschrittswerte sind nie NaN, unendlich oder negativ, Achse nie über 100");
  pruefe(falscheErfolge == 0, "Fuzz/B-6: einmessenAbschliessen liefert nie true, ohne dass alle vier Werte OK sind");
  std::printf("          (Erfolge bei Zufallsdaten: %d, nicht verlangt, nur zur Information)\n", erfolge);

  // Mitten in Schritt 2 NaN-Messungen: darf nie zu einem Erfolg mit unsinnigen Werten führen
  Skript sk;
  sk.basis = grundLage(0);
  planTeil(sk.plan, T_BEUGEN, 50.0, 0.8, 2.0);
  planTeil(sk.plan, T_KREIS, 25.0, 0.8, 1.0);
  planTeil(sk.plan, T_HAND, 60.0, 0.8, 1.0);
  skriptFertig(sk);
  ArmSim sim(skriptBewegung(sk), einbauStandard(), fehlerNurRauschen(), 55);
  EllbogenZustand z2;
  ellbogenZuruecksetzen(z2);
  einmessenStart(z2);
  while (!einmessenStillFertig(z2)) zyklenOhne(sim, z2, 1);
  *sk.t2 = sim.t;
  einmessenBewegungStart(z2);
  int falsch = 0;
  for (int i = 1; i <= 1500; i++) {
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
    if (i % 9 == 0) o.gx = NAN;
    if (i % 11 == 0) u.az = INFINITY;
    ellbogenAktualisieren(z2, o, u, 0.02f);
    if (i % 50 == 0) {
      // Reihenfolge laut REQ-005: erst Abschliessen, dann Fortschritt
      if (einmessenAbschliessen(z2)) {
        if (einmessenFehlenderGrund(z2) != EINMESSEN_GRUND_KEINER) falsch++;   // Erfolg nur mit allen vier OK
        float a = 0.0f;
        if (ellbogenAbweichung(z2, a) && !std::isfinite(a)) falsch++;
        break;
      }
      EinmessFortschritt f = EinmessFortschritt();
      einmessenFortschritt(z2, f);
      if (!std::isfinite(f.beugungGrad) || !std::isfinite(f.oberarmGrad) || !std::isfinite(f.handGrad)) falsch++;
      if (alleOk(f)) falsch++;
    }
  }
  pruefe(falsch == 0, "Fuzz: ungültige Messungen (NaN, unendlich) mitten in Schritt 2: nie unsinnige Werte, nie falscher Erfolg");
}

int main() {
  std::printf("REQ-005: Einfaches, verzeihendes Einmessen (Kern)\n");
  testRuhe();
  testFortschrittZahlen();
  testFortschrittNieKleiner();
  testErfolg();
  testReferenzNurAusRuhe();
  testAbbruchGruende();
  testNieErfolgOhneBewegung();
  testNeustart();
  testFuzz();
  return testErgebnis();
}
