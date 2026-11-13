// test_REQ-005_frei.cpp
// Tests zu REQ-005, Teil 2: das Einmessen am simulierten Arm "frei bewegen" (Kern: verzeihend, nie eine falsche Achse).
//
//   AK-11 : Reihenfolge egal, Pausen, Zusatzbewegungen, Tempo 0,3 bis 1,5 Hz, mehrere Einbaudrehungen, Sensorfehler wie
//           REQ-002 AK-10 (Gyro-Offset bis +-2 Grad/s, Rauschen). Kleine Auswahl: 3 Reihenfolgen x 3 Einbaudrehungen x
//           2 Tempi = 18 Läufe, reihum: nur Pflichtbewegungen / mit Pausen / mit Zusatzbewegungen.
//           Nach dem Erfolg wird der Winkelfehler gemessen (REQ-002 AK-1 bis AK-4, Stichprobe, <= 2,5 Grad).
//   AK-12 : gleichzeitige Bewegungen (beugen + Hand drehen, beugen + Oberarm-Kreis, alles zusammen): kein Erfolg mit falscher
//           Achse (Erfolg nur mit Winkelfehler <= 2,5 Grad). Kein Erfolg ist erlaubt (Abbruch).
//   AK-18 : Wiederholgenauigkeit: 10 Einmessungen mit jedes Mal etwas anderer Bewegung, Referenzen höchstens 3 Grad auseinander.
//   Zusatz: 29 s Warten im Warte-Zustand mit frei bewegtem Arm, dann Schritt 2 (früher REQ-004 AK-13).
//
// Annahmen des Testers (nicht wörtlich in der REQ, bitte prüfen):
//   - "Oberarm-Kreis von >= 20 Grad": Nach vorn 20 Grad und zur Seite +-10 Grad, also eine Spannweite von je 20 Grad in beiden
//     Richtungen (so wie in REQ-002 AK-6, "Spannweite von je 15 Grad oder mehr").
//   - Der Arm hängt bei Schritt 1 locker; Schritt 2 beginnt in derselben Lage (wie die Bestätigung im Warte-Zustand).
//   - Die Person bestätigt sofort (Wartezeit 0). Gyro-Offset ändert sich im Test nicht (Drift: test_REQ-002_drift.cpp).
//   - Nach Erfolg: Fehler wird bis zum Ende der Prüfphase (Stillhalten, dann Alltags-Bewegung mit Beugen bis -100 Grad,
//     Hand drehen, Oberarm drehen, Schwenken) gemessen.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"
#include "frei_hilfe.h"

double g_groessterFehler = 0.0;   // für die Reserve am Ende (Grenze 2,5 Grad)

void zusatz(bool bedingung, const char *text) {
  if (bedingung) {
    std::printf("  Zusatz ok      %s\n", text);
  } else {
    std::printf("  Zusatz HINWEIS %s (zählt nicht als Fehler)\n", text);
  }
}

// Pflichtbewegung für 'B' (Beugen), 'O' (Oberarm-Kreis), 'H' (Hand drehen). Mindestgrössen aus AK-11.
void pflicht(Plan &pl, char c, double f) {
  if (c == 'B') planTeil(pl, T_BEUGEN, 40.0, f, 2.0);   // mindestens 2 Mal um 40 Grad
  if (c == 'O') planTeil(pl, T_KREIS, 20.0, f, 1.0);    // ein Kreis von 20 Grad
  if (c == 'H') planTeil(pl, T_HAND, 60.0, f, 1.0);     // 60 Grad hin und zurück
}

// Zusatzbewegung (immer mit Tempo 1 Hz): beugen, kreisen, Hand drehen, schwenken.
void zusatzBewegung(Plan &pl, char c) {
  if (c == 'B') planTeil(pl, T_BEUGEN, 70.0, 1.0, 2.0);
  if (c == 'O') planTeil(pl, T_KREIS, 25.0, 1.0, 1.0);
  if (c == 'H') planTeil(pl, T_HAND, 80.0, 1.0, 1.0);
  if (c == 'S') planTeil(pl, T_SCHWENK, 30.0, 1.0, 1.0);
}

// variante: 0 = nur Pflicht direkt hintereinander, 1 = mit Pausen (1,5 s), 2 = mit Zusatzbewegungen und Pausen (1 s)
Plan bauePlanAK11(const char *reihenfolge, double f, int variante) {
  Plan pl;
  planPause(pl, 0.5);
  if (variante == 2) {
    zusatzBewegung(pl, 'S');
    planPause(pl, 1.0);
  }
  for (int i = 0; i < 3; i++) {
    char c = reihenfolge[i];
    if (variante == 1) planPause(pl, 1.5);
    if (variante == 2) {
      zusatzBewegung(pl, c);
      planPause(pl, 1.0);
    }
    pflicht(pl, c, f);
  }
  return pl;
}

// ======================================================================
// AK-11
// ======================================================================

void testAK11() {
  std::printf("\n-- AK-11: Reihenfolge egal, Pausen, Zusatzbewegungen, Tempo 0,3 und 1,5 Hz, drei Einbaudrehungen\n");
  const char *reihenfolgen[3] = {"BOH", "HBO", "OHB"};
  const int einbauten[3] = {1, 2, 4};
  const double tempi[2] = {0.3, 1.5};
  const char *varNamen[3] = {"nur Pflicht", "mit Pausen", "mit Zusatzbewegungen"};
  std::string nichtKlappt;
  std::string falschFortschritt;
  int lauf = 0;
  int geklappt = 0;
  int widerspruch = 0;
  int rueckwaerts = 0;
  int zuLange = 0;
  int unbekannt = 0;
  int zuGrosserFehler = 0;
  int stillFalsch = 0;
  double schlimmster = 0.0;
  double laengste = 0.0;
  for (int r = 0; r < 3; r++) {
    for (int k = 0; k < 3; k++) {
      for (int t = 0; t < 2; t++) {
        int variante = lauf % 3;
        Skript sk;
        sk.basis = grundLage(einbauten[k]);
        sk.plan = bauePlanAK11(reihenfolgen[r], tempi[t], variante);
        skriptFertig(sk);
        ArmSim sim(skriptBewegung(sk), einbauNr(einbauten[k]), fehlerMitOffset(lauf % 2), 5000 + static_cast<unsigned long long>(lauf));
        EllbogenZustand z;
        ellbogenZuruecksetzen(z);
        Ergebnis e = frei(sim, z, sk);
        char name[160];
        std::snprintf(name, sizeof(name), "%s, Einbau %d, %.1f Hz, %s", reihenfolgen[r], einbauten[k], tempi[t], varNamen[variante]);
        if (!e.ok) {
          if (!nichtKlappt.empty()) nichtKlappt += "; ";
          nichtKlappt += name;
          std::printf("          NICHT eingemessen (Grund %d): %s\n", static_cast<int>(e.grund), name);
        } else {
          geklappt++;
          if (e.dauerS2 > 30.0) zuLange++;
          if (e.dauerS2 > laengste) laengste = e.dauerS2;
          unbekannt += e.unbekannt;
          if (e.maxFehler > 2.5) {
            zuGrosserFehler++;
            std::printf("          Fehler %.2f Grad: %s\n", e.maxFehler, name);
          }
          if (e.maxFehler > schlimmster) schlimmster = e.maxFehler;
        }
        if (e.stillS < 1.9 || e.stillS > 2.1) stillFalsch++;
        widerspruch += e.widerspruch + e.fehlenderPasst;
        rueckwaerts += e.rueckwaerts + e.ungueltig;
        lauf++;
      }
    }
  }
  if (schlimmster > g_groessterFehler) g_groessterFehler = schlimmster;
  std::printf("          %d von %d Läufen eingemessen, längster Schritt 2 bis Erfolg: %.1f s, grösster Winkelfehler danach: %.2f Grad\n",
              geklappt, lauf, laengste, schlimmster);
  char text[400];
  std::snprintf(text, sizeof(text), "AK-11: alle 18 Läufe klappen innerhalb von 30 s (nicht geklappt: %s)", nichtKlappt.c_str());
  pruefe(nichtKlappt.empty(), text);
  pruefe(zuLange == 0, "AK-11: kein Erfolg nach mehr als 30 s Schritt 2");
  pruefe(stillFalsch == 0, "AK-3: Schritt 1 dauert in allen Läufen 2000 ms (+-100 ms) (Arm ruhig von Anfang an)");
  pruefe(widerspruch == 0, "AK-10: Erfolg genau dann, wenn alle vier Werte OK sind (alle Läufe, jede Sekunde)");
  pruefe(rueckwaerts == 0, "AK-8: Fortschrittswerte nie kleiner und immer sinnvoll (alle Läufe)");
  pruefe(unbekannt == 0, "AK-10: nach dem Einmessen ist der Winkel in allen Läufen bekannt");
  pruefe(zuGrosserFehler == 0, "AK-10 (REQ-002 AK-1 bis AK-4 Stichprobe): nach dem Einmessen Winkelfehler <= 2,5 Grad in allen Läufen");
}

// ======================================================================
// AK-12: gleichzeitige Bewegungen
// ======================================================================

void testAK12() {
  std::printf("\n-- AK-12: gleichzeitige Bewegungen: nie ein Erfolg mit falschem Winkel (Abbruch oder weiter sammeln ist erlaubt)\n");
  const double tempi[2] = {0.5, 1.2};
  const int einbauten[3] = {0, 2, 4};
  int lauf = 0;
  int erfolge = 0;
  int falsch = 0;
  int ungueltig = 0;
  double schlimmster = 0.0;
  for (int art = 0; art < 3; art++) {
    for (int k = 0; k < 3; k++) {
      for (int t = 0; t < 2; t++) {
        double f = tempi[t];
        Plan pl;
        planPause(pl, 0.5);
        if (art == 0) {          // beugen und dabei die Hand drehen
          planTeil(pl, T_BEUGEN, 60.0, f, 3.0);
          planTeil(pl, T_HAND, 70.0, f, 3.0, true);
        } else if (art == 1) {   // beugen und dabei den Oberarm kreisen
          planTeil(pl, T_BEUGEN, 60.0, f, 3.0);
          planTeil(pl, T_KREIS, 25.0, f, 3.0, true);
        } else {                 // alles zusammen
          planTeil(pl, T_BEUGEN, 60.0, f, 3.0);
          planTeil(pl, T_HAND, 70.0, f, 3.0, true);
          planTeil(pl, T_KREIS, 25.0, f, 3.0, true);
        }
        Skript sk;
        sk.basis = grundLage(einbauten[k]);
        sk.plan = pl;
        skriptFertig(sk);
        ArmSim sim(skriptBewegung(sk), einbauNr(einbauten[k]), fehlerMitOffset((lauf + 1) % 2), 6000 + static_cast<unsigned long long>(lauf));
        EllbogenZustand z;
        ellbogenZuruecksetzen(z);
        Ergebnis e = frei(sim, z, sk);
        if (e.ok) {
          erfolge++;
          if (e.unbekannt > 0 || e.maxFehler > 2.5) {
            falsch++;
            std::printf("          FALSCHER ERFOLG (Fehler %.2f Grad): Art %d, Einbau %d, %.1f Hz\n", e.maxFehler, art, einbauten[k], f);
          }
          if (e.maxFehler > schlimmster) schlimmster = e.maxFehler;
        }
        ungueltig += e.widerspruch + e.fehlenderPasst + e.rueckwaerts + e.ungueltig;
        lauf++;
      }
    }
  }
  if (schlimmster > g_groessterFehler) g_groessterFehler = schlimmster;
  std::printf("          %d von %d Läufen mit Erfolg (nicht verlangt), grösster Fehler bei Erfolg: %.2f Grad\n", erfolge, lauf, schlimmster);
  pruefe(falsch == 0, "AK-12: nie ein Erfolg, bei dem der Winkel danach um mehr als 2,5 Grad falsch ist (18 Läufe)");
  pruefe(ungueltig == 0, "AK-12: Fortschritt und Erfolg bleiben in sich stimmig (genau dann Erfolg, wenn alle vier OK)");
  zusatz(erfolge >= 9, "Zusatz: mindestens die Hälfte der Läufe mit gleichzeitigen Bewegungen klappt (nicht verlangt)");
}

// ======================================================================
// AK-18: Wiederholgenauigkeit
// ======================================================================

void testAK18() {
  std::printf("\n-- AK-18: Wiederholgenauigkeit (10 Einmessungen an derselben Haltung, Bewegung jedes Mal etwas anders)\n");
  Zufall zf(18);
  double werte[10];
  int geklappt = 0;
  int unbekannt = 0;
  int ohneMessung = 0;
  for (int k = 0; k < 10; k++) {
    // zufällige Reihenfolge von B, O, H
    char reihenfolge[4] = {'B', 'O', 'H', 0};
    for (int i = 2; i > 0; i--) {
      int j = static_cast<int>(zf.gleich() * (i + 1));
      if (j > i) j = i;
      char tmp = reihenfolge[i];
      reihenfolge[i] = reihenfolge[j];
      reihenfolge[j] = tmp;
    }
    double f = 0.5 + zf.gleich() * 1.0;   // 0,5 bis 1,5 Hz
    Plan pl;
    planPause(pl, 0.5);
    for (int i = 0; i < 3; i++) {
      planPause(pl, zf.gleich() * 1.5);
      char c = reihenfolge[i];
      if (c == 'B') planTeil(pl, T_BEUGEN, 40.0 + zf.gleich() * 50.0, f, 2.0 + (zf.gleich() > 0.5 ? 1.0 : 0.0));
      if (c == 'O') planTeil(pl, T_KREIS, 20.0 + zf.gleich() * 15.0, f, 1.0);
      if (c == 'H') planTeil(pl, T_HAND, 60.0 + zf.gleich() * 30.0, f, 1.0);
    }
    if (zf.gleich() > 0.5) planTeil(pl, T_SCHWENK, 25.0, 1.0, 1.0);
    Skript sk;
    sk.basis = grundLage(1);
    sk.plan = pl;
    skriptFertig(sk);
    ArmSim sim(skriptBewegung(sk), einbauNr(1), fehlerMitOffset(0), 7000 + static_cast<unsigned long long>(k));
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    Ergebnis e = frei(sim, z, sk);
    if (e.ok) geklappt++;
    unbekannt += e.unbekannt;
    if (e.ok && std::isfinite(e.mittelStill)) {
      werte[k] = e.mittelStill;
    } else {
      werte[k] = 0.0;
      if (e.ok) ohneMessung++;
    }
  }
  double kleinster = werte[0];
  double groesster = werte[0];
  double betrag = 0.0;
  for (int k = 0; k < 10; k++) {
    if (werte[k] < kleinster) kleinster = werte[k];
    if (werte[k] > groesster) groesster = werte[k];
    if (std::fabs(werte[k]) > betrag) betrag = std::fabs(werte[k]);
  }
  std::printf("          Streuung der 10 Referenzen = %.3f Grad, grösster Betrag = %.3f Grad (ohne Messfenster: %d)\n",
              groesster - kleinster, betrag, ohneMessung);
  pruefe(geklappt == 10, "AK-18: alle 10 Einmessungen klappen");
  pruefe(ohneMessung == 0, "AK-18: nach jedem Erfolg konnte die Referenz im Stillhalten gemessen werden (Erfolg vor Ende des Plans)");
  pruefe(unbekannt == 0, "AK-18: nach jedem Einmessen ist der Winkel bekannt");
  pruefe(groesster - kleinster <= 3.0, "AK-18: die 10 Referenzen liegen höchstens 3 Grad auseinander (+-1,5)");
  pruefe(betrag <= 2.5, "AK-18: jede Referenz stimmt auf 2,5 Grad (gestreckter Arm zeigt 0)");
}

// ======================================================================
// Zusatz: 29 s im Warte-Zustand (früher REQ-004 AK-13)
// ======================================================================

void testZusatzWarten() {
  std::printf("\n-- Zusatz: 29 s Warten (Arm frei bewegt), dann Schritt 2\n");
  Skript sk;
  sk.basis = grundLage(1);
  const double warten = 29.0;
  // Während des Wartens (ab 2,2 s) bewegt sich der Arm frei und ist bei der Bestätigung wieder in der Ausgangslage.
  sk.vorher = [warten](double t, const Pose &b) -> Pose {
    Pose p = b;
    double u = (t - 2.2) / (warten - 0.2);
    if (u > 0.0 && u < 1.0) {
      double huelle = std::sin(PI_ZAHL * u);
      p.flex = b.flex - 40.0 * huelle * 0.5 * (1.0 - std::cos(2.0 * PI_ZAHL * 0.4 * t));
      p.pron = b.pron + 30.0 * huelle * std::sin(2.0 * PI_ZAHL * 0.25 * t);
      p.yaw = b.yaw + 20.0 * huelle * std::sin(2.0 * PI_ZAHL * 0.1 * t);
    }
    return p;
  };
  planPause(sk.plan, 0.5);
  pflicht(sk.plan, 'B', 0.8);
  pflicht(sk.plan, 'O', 0.8);
  pflicht(sk.plan, 'H', 0.8);
  skriptFertig(sk);
  ArmSim sim(skriptBewegung(sk), einbauNr(1), fehlerMitOffset(1), 8001);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  Ergebnis e = frei(sim, z, sk, warten);
  zusatz(e.ok, "Zusatz: nach 29 s im Warte-Zustand klappt Schritt 2");
  zusatz(e.ok && e.unbekannt == 0 && e.maxFehler <= 2.5, "Zusatz: danach stimmt der Winkel auf 2,5 Grad");
}

int main() {
  std::printf("REQ-005: freies Bewegen (AK-11, AK-12, AK-18)\n");
  testAK11();
  testAK12();
  testAK18();
  testZusatzWarten();
  std::printf("\nRESERVE (Grenze 2,5 Grad): groesster Winkelfehler nach erfolgreichem Einmessen (AK-11/AK-12) = %.3f Grad\n", g_groessterFehler);
  return testErgebnis();
}
