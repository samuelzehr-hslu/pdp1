// test_REQ-002_drift.cpp
// Tests zu REQ-002: AK-10, AK-11, AK-12 (Drift, je 10 Minuten simulierte Zeit bei 50 Hz).
// Eigene Datei, damit sie unter dem 60-Sekunden-Limit von alle_tests.py bleibt.
//
// Sensorfehler wie in AK-10: Gyro-Offset +-2 Grad/s je Achse (Worst-Case-Vorzeichen, zwei Muster),
// Gyro-Rauschen 0,1 Grad/s, Beschleunigungs-Rauschen 0,01 g (Effektivwerte).
// Der Simulator (sim_ellbogen.h) kennt nur die wahre Arm-Lage. Die Logik wird nicht nachgebaut.

#include <cmath>
#include <cstdio>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"

// Grösste Fehler (für die Reserve im Bericht, gedruckt am Ende von main). Grenze je 2,5 Grad (AK-10, AK-11).
double g_groessterFehlerMitRuhe = 0.0;     // AK-10
double g_groessterFehlerOhneRuhe = 0.0;    // AK-11

// 10 Minuten normale Bewegung nach dem Einmessen (Einmessen im Hängen: 2 Mal beugen + 1 Oberarm-Kreis, dann Hand drehen). mitPausen: alle 30 s eine Ruhepause von genau 1 s.
void driftLauf(bool mitPausen, int fehlerMuster, const char *kopf) {
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    double tau = mitPausen ? ruheZeit(s) : s;
    return alltagPose(ende, tau, 70.0);
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  ArmSim sim(bew, einbauStandard(), fehlerMitOffset(fehlerMuster), static_cast<unsigned long long>(1000 + fehlerMuster));
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  Messreihe m = laufe(sim, z, 600.0, 0.0);
  char text[200];
  std::snprintf(text, sizeof(text), "%s (Fehlermuster %d): Einmessen klappt", kopf, fehlerMuster);
  pruefe(ok, text);
  std::snprintf(text, sizeof(text), "%s (Fehlermuster %d): Winkel in den 10 Minuten immer bekannt", kopf, fehlerMuster);
  pruefe(m.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "%s (Fehlermuster %d): Messfehler zu jedem Zeitpunkt <= 2,5 Grad", kopf, fehlerMuster);
  zeige("10 min", m);
  if (mitPausen && m.maxFehler > g_groessterFehlerMitRuhe) g_groessterFehlerMitRuhe = m.maxFehler;
  if (!mitPausen && m.maxFehler > g_groessterFehlerOhneRuhe) g_groessterFehlerOhneRuhe = m.maxFehler;
  pruefe(m.maxFehler <= 2.5, text);
}

void testAK10und11() {
  std::printf("\n-- AK-10: Drift mit Ruhe (1 s Pause alle 30 s)\n");
  driftLauf(true, 0, "AK-10");
  driftLauf(true, 1, "AK-10");
  std::printf("\n-- AK-11: Drift ohne Ruhe (10 min Dauerbewegung)\n");
  driftLauf(false, 0, "AK-11");
  driftLauf(false, 1, "AK-11");
}

// AK-12 a): Überstreckung +8 Grad 10 Minuten halten. Die Warnung muss die ganze Zeit an bleiben.
// weiterBewegen = true: der Arm als Ganzes bewegt sich langsam, der Ellbogen bleibt bei +8 Grad (Zusatz, nicht wörtlich im AK).
void ueberstreckungHalten(bool weiterBewegen, const char *kopf) {
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = ende;
    p.flex = 8.0 * weich(s / 1.0);
    if (weiterBewegen) {
      p.yaw = ende.yaw + 6.0 * s;
      p.pitch = ende.pitch - 30.0 * (1.0 - std::cos(2.0 * PI_ZAHL * s / 13.7));
      p.twist = ende.twist + 40.0 * std::sin(2.0 * PI_ZAHL * s / 11.0);
    }
    return p;
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  ArmSim sim(bew, einbauStandard(), fehlerMitOffset(0), 1201);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  laufe(sim, z, 2.0, 0.0);                       // Überstreckung erreichen
  Messreihe m = laufe(sim, z, 600.0, 0.0);       // 10 Minuten halten
  char text[200];
  pruefe(ok, "Vorbereitung: Einmessen klappt");
  std::snprintf(text, sizeof(text), "%s: Winkel immer bekannt", kopf);
  pruefe(m.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "%s: +8 Grad bleiben 10 min lang auf 2,5 Grad genau", kopf);
  zeige("10 min", m);
  pruefe(m.maxFehler <= 2.5, text);
  std::snprintf(text, sizeof(text), "%s: Wert bleibt die ganze Zeit über dem Grenzwert 5 Grad (Warnung bleibt an)", kopf);
  std::printf("          kleinster Wert = %.3f Grad\n", m.minAbw);
  pruefe(m.minAbw > 5.0, text);
}

// AK-12 b): Ruhe, dauernd gleicher Winkel: der angezeigte Winkel driftet in 10 min höchstens 2 Grad.
void ruheDrift(double flexZiel, const char *kopf) {
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = ende;
    p.flex = flexZiel * weich(s / 1.0);
    return p;
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  ArmSim sim(bew, einbauStandard(), fehlerMitOffset(1), 1301);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  laufe(sim, z, 1.0, 0.0);                          // Bewegung zum Zielwinkel
  Messreihe anfang = laufe(sim, z, 2.0, 0.0);       // Anfangswert
  Messreihe lang = laufe(sim, z, 600.0, anfang.mittel);
  char text[200];
  pruefe(ok, "Vorbereitung: Einmessen klappt");
  std::snprintf(text, sizeof(text), "%s: Winkel immer bekannt", kopf);
  pruefe(lang.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "%s: angezeigter Winkel driftet in 10 min um höchstens 2 Grad", kopf);
  zeige("10 min", lang);
  pruefe(lang.maxAenderung <= 2.0, text);
  std::snprintf(text, sizeof(text), "%s: Winkel bleibt auf 2,5 Grad genau zum echten Winkel", kopf);
  pruefe(lang.maxFehler <= 2.5, text);
}

void testAK12() {
  std::printf("\n-- AK-12: Referenz bleibt sicher\n");
  ueberstreckungHalten(false, "AK-12: Überstreckung +8 Grad still halten");
  ueberstreckungHalten(true, "AK-12 Zusatz: +8 Grad halten, Arm als Ganzes bewegt sich");
  ruheDrift(0.0, "AK-12: Ruhe bei 0 Grad (gestreckt)");
  ruheDrift(-45.0, "AK-12: Ruhe bei -45 Grad");
}

int main() {
  std::printf("REQ-002: Drift (10 Minuten je Lauf)\n");
  testAK10und11();
  testAK12();
  std::printf("\nRESERVE (Grenze 2,5 Grad): groesster Fehler AK-10 (mit Ruhe) = %.3f Grad, AK-11 (ohne Ruhe) = %.3f Grad\n",
              g_groessterFehlerMitRuhe, g_groessterFehlerOhneRuhe);
  return testErgebnis();
}
