// test_REQ-004_warten.cpp
// Tests zu REQ-004, Teil 2: Wartezeiten zwischen den Phasen (AK-13, Physik-Annahmen "Bewegung beim Warten").
//
// Ablauf in jedem Test (Zeiten in Simulationszeit, Zyklus 20 ms):
//   0 bis 2 s         Phase 1 (Arm hängt still)            -> einmessenStillAuswerten
//   Warte-Zustand 1   W Sekunden, der Arm bewegt sich FREI (ganzer Arm, Unterarm drehen, Ellbogen beugen ...)
//                     In dieser Zeit wird NUR ellbogenAktualisieren aufgerufen (laufende Winkelmessung), sonst nichts.
//   Phase 2           4 s: 2 Mal beugen (70 Grad) + Oberarm-Kreis 20 Grad, ausgehend von der Lage bei der Bestätigung
//                     -> einmessenBewegungAuswerten
//   Warte-Zustand 2   W Sekunden, Arm wieder frei
//   Phase 3           1,5 s Hand drehen (60 Grad hin und zurück) -> einmessenAbschliessen
//   danach            3 s halten, dann 30 s "Alltag" (beugen 0 bis -100 Grad, dabei Hand drehen, Oberarm drehen, schwenken)
// Erwartung (AK-13): alle drei Prüfungen true, Lage bekannt, Winkel zu jedem Zeitpunkt auf 2,5 Grad genau.
//
// Sensorfehler (Physik-Annahmen REQ-004): Der Gyro-Offset wird in Phase 1 gemessen (+-2 Grad/s je Achse, wie REQ-002).
// Danach darf er sich bis zum Ende der beiden Wartezeiten um höchstens 0,05 Grad/s je Achse ändern: hier als gleichmässige
// Rampe von 0 bis 0,05 Grad/s (Vorzeichen so gewählt, dass sich die Fehler der beiden IMUs NICHT aufheben).
//
// Annahmen des Testers (nicht wörtlich in der REQ, bitte bestätigen):
//   - Der Ellbogen ist bei der Bestätigung gestreckt (0 Grad), die Person beginnt "beugen" aus der Streckung.
//   - Die Lage des ganzen Arms bei der Bestätigung ist beliebig (hängend, nach vorn, nach oben, seitlich).
//   - Während der Wartezeit wird weiter ellbogenAktualisieren aufgerufen (die Messung läuft weiter).

#include <cmath>
#include <cstdio>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"

// Lage des ganzen Arms (ohne Ellbogen): siehe Pose in sim_ellbogen.h. pitch 90 = hängend, 0 = nach vorn, -90 = nach oben.
struct Lage {
  double yaw;
  double pitch;
  double seit;
  double twist;
  double pron;
};

Pose zuPose(const Lage &l) {
  Pose p;
  p.yaw = l.yaw;
  p.pitch = l.pitch;
  p.seit = l.seit;
  p.twist = l.twist;
  p.flex = 0.0;
  p.pron = l.pron;
  return p;
}

// Freie Bewegung während des Wartens: sanft von 'von' nach 'nach' (dauer Sekunden), dazu "Wackeln" in allen
// Gelenken (auch der Ellbogen beugt sich bis -60 Grad). Am Anfang und am Ende ist das Wackeln 0.
Pose freiBewegen(const Pose &von, const Pose &nach, double s, double dauer, double wackel) {
  double u = (dauer > 0.0) ? s / dauer : 1.0;
  if (u < 0.0) u = 0.0;
  if (u > 1.0) u = 1.0;
  double w = weich(u);
  double h = std::sin(PI_ZAHL * u);
  double a = 2.0 * PI_ZAHL * s;
  Pose p;
  p.yaw = von.yaw + (nach.yaw - von.yaw) * w + wackel * 60.0 * h * std::sin(a / 17.0);
  p.pitch = von.pitch + (nach.pitch - von.pitch) * w + wackel * 40.0 * h * std::sin(a / 11.0);
  p.seit = von.seit + (nach.seit - von.seit) * w + wackel * 20.0 * h * std::sin(a / 19.0);
  p.twist = von.twist + (nach.twist - von.twist) * w + wackel * 50.0 * h * std::sin(a / 13.0);
  p.pron = von.pron + (nach.pron - von.pron) * w + wackel * 80.0 * h * std::sin(a / 7.0);
  p.flex = von.flex + (nach.flex - von.flex) * w - wackel * 60.0 * h * 0.5 * (1.0 - std::cos(a / 9.0));
  return p;
}

struct Fall4 {
  const char *name;
  int einbau;          // einbauNr(0..4)
  int fehlerMuster;    // fehlerMitOffset(0 oder 1)
  Lage lage1;          // Lage bei der 1. Bestätigung (Start Phase 2)
  Lage lage2;          // Lage bei der 2. Bestätigung (Start Phase 3)
  double wartezeit;    // je Warte-Zustand in Sekunden
  double wackel;       // 1 = Arm bewegt sich frei, 0 = Arm hängt still
  double drift;        // +1 / -1 = Vorzeichen der Offset-Änderung (0,05 Grad/s am Ende), 0 = keine
};

Bewegung ablauf(const Fall4 &c) {
  Pose p0 = poseHaengend();
  Pose p1 = zuPose(c.lage1);
  Pose p2 = zuPose(c.lage2);
  double W = c.wartezeit;
  double wackel = c.wackel;
  double t1 = 2.0;
  double t2 = t1 + W;      // Bestätigung 1: Start Phase 2
  double t3 = t2 + 4.0;    // Ende Phase 2
  double t4 = t3 + W;      // Bestätigung 2: Start Phase 3
  double t5 = t4 + 1.5;    // Ende Phase 3
  return [=](double t) -> Pose {
    if (t < t1) return p0;
    if (t < t2) return freiBewegen(p0, p1, t - t1, W, wackel);
    if (t < t3) {
      Pose p = p1;
      double s = t - t2;
      p.flex = beugeKurve(s, 70.0, 2);
      oberarmBewegen(p, s, OBERARM_KREISE, 20.0);
      return p;
    }
    if (t < t4) return freiBewegen(p1, p2, t - t3, W, wackel);
    if (t < t5) {
      Pose p = p2;
      phase3Bewegen(p, t - t4);
      return p;
    }
    double s = t - t5;
    if (s < 3.0) return p2;
    return alltagPose(p2, s - 3.0, 60.0);
  };
}

// n Zyklen: Gyro-Offset ändert sich als Rampe (0 bis 0,05 Grad/s je Achse) von sekStart an über 'dauer' Sekunden.
// Es wird NUR ellbogenAktualisieren aufgerufen (laufende Messung).
void zyklenMitDrift(ArmSim &sim, EllbogenZustand &zustand, int n, const SensorFehler &basis, double vorzeichen,
                    double sekStart, double dauer) {
  for (int i = 0; i < n; i++) {
    double f = (sim.t - sekStart) / dauer;
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    double d = vorzeichen * 0.05 * f;
    sim.fehler = basis;
    sim.fehler.gyroOffsetOben.x += d;
    sim.fehler.gyroOffsetOben.y -= d;
    sim.fehler.gyroOffsetOben.z += d;
    sim.fehler.gyroOffsetUnten.x -= d;
    sim.fehler.gyroOffsetUnten.y += d;
    sim.fehler.gyroOffsetUnten.z -= d;
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
    ellbogenAktualisieren(zustand, o, u, static_cast<float>(sim.dt));
  }
}

void ablaufPruefen(const Fall4 &c) {
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  SensorFehler basis = fehlerMitOffset(c.fehlerMuster);
  ArmSim sim(ablauf(c), einbauNr(c.einbau), basis, 500 + static_cast<unsigned long long>(c.einbau));
  int nWarte = static_cast<int>(c.wartezeit / sim.dt + 0.5);
  double driftDauer = 2.0 * c.wartezeit + 4.0;   // Rampe vom Ende von Phase 1 bis zum Ende des 2. Warte-Zustands
  const double t1 = 2.0;

  einmessenStart(z);
  zyklenMitDrift(sim, z, 100, basis, c.drift, t1, driftDauer);       // Phase 1
  bool ok1 = einmessenStillAuswerten(z);
  zyklenMitDrift(sim, z, nWarte, basis, c.drift, t1, driftDauer);    // Warte-Zustand 1: nur Messung läuft
  einmessenBewegungStart(z);
  zyklenMitDrift(sim, z, 200, basis, c.drift, t1, driftDauer);       // Phase 2
  bool ok2 = einmessenBewegungAuswerten(z, 30.0f);
  zyklenMitDrift(sim, z, nWarte, basis, c.drift, t1, driftDauer);    // Warte-Zustand 2
  einmessenDrehungStart(z);
  zyklenMitDrift(sim, z, 75, basis, c.drift, t1, driftDauer);        // Phase 3
  bool ok3 = einmessenAbschliessen(z, 30.0f);
  Messreihe m = laufe(sim, z, 33.0, 0.0);                            // 3 s halten + 30 s Alltag

  char text[260];
  std::snprintf(text, sizeof(text), "AK-13 %s: Still-Prüfung nach Phase 1 liefert true", c.name);
  pruefe(ok1, text);
  std::snprintf(text, sizeof(text), "AK-13 %s: Bewegungs-Prüfung nach Phase 2 liefert true (Wartezeit %.0f s davor)", c.name, c.wartezeit);
  pruefe(ok2, text);
  std::snprintf(text, sizeof(text), "AK-13 %s: Abschluss nach Phase 3 liefert true (Wartezeit %.0f s davor)", c.name, c.wartezeit);
  pruefe(ok3, text);
  std::snprintf(text, sizeof(text), "AK-13 %s: danach ist die Lage immer bekannt", c.name);
  pruefe(m.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "AK-13 %s: Winkel zu jedem Zeitpunkt auf 2,5 Grad genau (Halten und gleichzeitige Bewegungen)", c.name);
  zeige(c.name, m);
  pruefe(m.maxFehler <= 2.5, text);
}

void testAK13() {
  std::printf("\n-- AK-13: Wartezeiten dürfen die Messung nicht verschlechtern\n");
  const Lage haengend = {0.0, 90.0, 0.0, 0.0, 0.0};
  const Lage vorn = {0.0, 0.0, 0.0, 0.0, 0.0};
  const Lage oben = {0.0, -90.0, 0.0, 0.0, 0.0};
  const Lage seitlich = {90.0, 0.0, 0.0, 30.0, -20.0};
  const Lage schraeg = {-50.0, 40.0, 10.0, -40.0, 30.0};
  // Bei der 1. Bestätigung ist die Hand (pron) wie in Phase 1 (REQ-004, Physik-Annahmen): pron = 0
  const Lage seitlich1 = {90.0, 0.0, 0.0, 30.0, 0.0};
  const Lage schraeg1 = {-50.0, 40.0, 10.0, -40.0, 0.0};
  const Fall4 faelle[] = {
      // name, einbau, fehlerMuster, lage1, lage2, wartezeit, wackel, drift
      {"29 s, Arm frei, Lage hängend/hängend, Drift +", 0, 0, haengend, haengend, 29.0, 1.0, 1.0},
      {"29 s, Arm frei, Lage vorn/hängend, Drift -", 1, 1, vorn, haengend, 29.0, 1.0, -1.0},
      {"29 s, Arm frei, Lage oben/seitlich, Drift +", 2, 0, oben, seitlich, 29.0, 1.0, 1.0},
      {"29 s, Arm frei, Lage seitlich/vorn, Drift -", 3, 1, seitlich1, vorn, 29.0, 1.0, -1.0},
      {"29 s, Arm frei, Lage schräg/schräg, zufälliger Einbau, Drift +", 4, 0, schraeg1, schraeg, 29.0, 1.0, 1.0},
      {"29 s, Arm hängt still, Drift +", 0, 1, haengend, haengend, 29.0, 0.0, 1.0},
      {"kurze Wartezeit 0,5 s, ohne Drift", 0, 1, haengend, haengend, 0.5, 1.0, 0.0},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) ablaufPruefen(faelle[i]);
}

int main() {
  testAK13();
  return testErgebnis();
}
