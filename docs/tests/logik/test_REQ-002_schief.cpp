// test_REQ-002_schief.cpp
// Tests zu REQ-002, AK-16: Schiefes Einmessen.
// Der Arm hängt beim Einmessen schief (Neigung 0 bis 10 Grad nach vorn, hinten, seitlich oder dazwischen, hier 8 Richtungen).
// Erwartung: Einmessen klappt (bis 8 Grad muss es, bei 10 Grad darf es abbrechen), danach gelten AK-1 bis AK-4 mit höchstens
// 2,5 Grad Fehler (gleichzeitig beugen, Hand drehen, Oberarm drehen, schwenken, neigen). Referenz am echten Winkel 0 und -20 Grad.
// Ab 10 Grad darf das Einmessen abbrechen, aber ein FALSCHER Winkel (Fehler über 2,5 Grad) ohne Abbruch ist nie erlaubt (B-6).
// Neigung: pitch = 90 - delta*cos(richtung) (vorn/hinten), seit = delta*sin(richtung) (seitlich). Hand dreht in Phase 3 um 60 Grad.

#include <cmath>
#include <cstdio>
#include <string>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"

double g_groessterFehler = 0.0;   // nur für die Reserve am Ende (Grenze 2,5 Grad), nur Läufe die geklappt haben

// Ergebnis eines Laufs: 0 = Einmessen abgebrochen, 1 = geklappt und Messung ok, 2 = geklappt, aber falscher Winkel / Lage unbekannt
int lauf(double delta, double richtungGrad, double refFlex, int einbauNummer, unsigned long long samen) {
  double r = gradZuRad(richtungGrad);
  Pose start = poseHaengend();
  start.pitch = 90.0 - delta * std::cos(r);
  start.seit = delta * std::sin(r);
  start.twist = twistStart(einbauNummer);
  start.flex = refFlex;
  // Nach dem Einmessen: alles gleichzeitig. Beugung von refFlex um 'amp' (Gesamtbereich 0/-20 bis -100).
  double amp = (refFlex < -10.0) ? 40.0 : 50.0;
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = alltagPose(ende, s, 70.0);
    p.flex = ende.flex - amp * (1.0 - std::cos(PI_ZAHL * s));
    return p;
  };
  ArmSim sim(mitEinmessen(start, 70.0, 2, 0.0, 0.0, danach), einbauNr(einbauNummer), fehlerNr(einbauNummer), samen);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  if (!ok) {
    bool nochmal = einmessenAbschliessen(z, 30.0f);
    float a = 0.0f;
    if (nochmal || ellbogenAbweichung(z, a)) return 2;   // nach Abbruch ohne Referenz darf nichts gemeldet werden
    return 0;
  }
  Messreihe m = laufe(sim, z, 14.0, 0.0, refFlex);
  if (m.maxFehler > g_groessterFehler) g_groessterFehler = m.maxFehler;
  return messOk(m) ? 1 : 2;
}

// Alle 8 Richtungen für eine Neigung durchspielen. pflicht = true: Einmessen muss klappen.
void neigungPruefen(double delta, double refFlex, int einbauNummer, bool pflicht, const char *kopf) {
  std::string abgebrochen;
  std::string falsch;
  for (int i = 0; i < 8; i++) {
    int e = lauf(delta, 45.0 * i, refFlex, einbauNummer, 200 + i);
    if (e == 0) merke(abgebrochen, static_cast<int>(45 * i));
    if (e == 2) merke(falsch, static_cast<int>(45 * i));
  }
  char text[260];
  std::snprintf(text, sizeof(text), "%s: nie ein falscher Winkel (über 2,5 Grad) ohne Abbruch (falsch bei Richtung in Grad: %s)", kopf, falsch.c_str());
  pruefe(falsch.empty(), text);
  if (pflicht) {
    std::snprintf(text, sizeof(text), "%s: Einmessen klappt in allen 8 Richtungen (abgebrochen bei Richtung in Grad: %s)", kopf, abgebrochen.c_str());
    pruefe(abgebrochen.empty(), text);
  }
}

int main() {
  std::printf("REQ-002: AK-16 schiefes Einmessen\n");
  char kopf[200];

  const double refs[2] = {0.0, -20.0};
  const double neigungen[6] = {0.0, 1.0, 2.0, 5.0, 8.0, 10.0};
  for (int ri = 0; ri < 2; ri++) {
    std::printf("\n-- AK-16: Referenz bei %.0f Grad, Neigung 0 bis 10 Grad in 8 Richtungen (Einbau 0)\n", refs[ri]);
    for (int ni = 0; ni < 6; ni++) {
      // Bis 8 Grad muss das Einmessen klappen. Bei genau 10 Grad ist Abbruch erlaubt ("ab 10 Grad").
      bool pflicht = neigungen[ni] <= 8.0;
      std::snprintf(kopf, sizeof(kopf), "AK-16: Neigung %.0f Grad, Referenz %.0f Grad", neigungen[ni], refs[ri]);
      neigungPruefen(neigungen[ni], refs[ri], 0, pflicht, kopf);
    }
  }

  std::printf("\n-- AK-16: andere Einbaulagen der IMUs (Neigung 5 und 10 Grad)\n");
  for (int k = 1; k < ANZAHL_EINBAU_H; k++) {
    std::snprintf(kopf, sizeof(kopf), "AK-16: Einbau %d, Neigung 5 Grad", k);
    neigungPruefen(5.0, 0.0, k, true, kopf);
    std::snprintf(kopf, sizeof(kopf), "AK-16: Einbau %d, Neigung 10 Grad", k);
    neigungPruefen(10.0, 0.0, k, false, kopf);
  }

  std::printf("\n-- AK-16: starke Schieflage (über 10 Grad): Abbruch erlaubt, falsche Winkel nie\n");
  const double gross[4] = {12.0, 15.0, 20.0, 30.0};
  for (int gi = 0; gi < 4; gi++) {
    std::snprintf(kopf, sizeof(kopf), "AK-16: Neigung %.0f Grad", gross[gi]);
    neigungPruefen(gross[gi], 0.0, 0, false, kopf);
  }

  std::printf("\nRESERVE (Grenze 2,5 Grad): groesster Fehler bei geklappten Einmessungen = %.3f Grad\n", g_groessterFehler);
  return testErgebnis();
}
