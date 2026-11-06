// test_REQ-002_hand.cpp
// Tests zu REQ-002: Phase 3 des Einmessens ("Hand drehen", Knochenachse des Unterarms messen).
//   AK-6 : Phase 3 klappt bei Drehung ab 45 Grad (hin und zurück), danach stimmt der Winkel (<= 2,5 Grad)
//   AK-7e: Abbruch, wenn die Hand nicht (genug) gedreht wird oder die falsche Bewegung gemacht wird
// Der Simulator (sim_ellbogen.h) dreht den Unterarm in Phase 3 um die EIGENE Achse (phase3Art = 0) oder beugt ihn
// stattdessen (phase3Art = 1, falsche Bewegung). Die Logik wird nicht nachgebaut.
// Der Meldungstext "Grund: Hand" steht nur in jointsense.ino (nur am Board / in Wokwi prüfbar).

#include <cmath>
#include <cstdio>
#include <string>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"

// Zeitplan: 0-7,5 s erstes Einmessen bei 0 Grad (gültig), 7,5-9,5 s Arm weich auf -20 Grad, bis 11,5 s halten,
// 11,5-19 s zweites Einmessen (Basis -20 Grad), 19-25 s halten, ab 25 s Beugen bis -90 Grad mit Unterarm-Drehen.
Bewegung zweiEinmessungen() {
  return [=](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t < 9.5) { p.flex = -20.0 * weich((t - 7.5) / 2.0); return p; }
    p.flex = -20.0;
    if (t < 11.5) return p;
    if (einmessenPose(p, t, 11.5, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t >= 25.0) {
      p.flex = -20.0 - 35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * (t - 25.0) / 4.0));
      p.pron = 40.0 * std::sin(2.0 * PI_ZAHL * (t - 25.0) / 3.0);
    }
    return p;
  };
}

// Zweites Einmessen mit verändertem Phase 3. pflicht = true: muss abbrechen.
// pflicht = false: darf klappen oder abbrechen, aber nie einen falschen Winkel ohne Abbruch liefern (richtig oder Abbruch).
void abbruchMitAlterReferenz(const char *kopf, double grad, int art, bool pflicht) {
  Phase3Einstellung einst(60.0, 0);    // stellt am Ende alles zurück
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(zweiEinmessungen(), einbauStandard(), fehlerNurRauschen(), 91);
  bool ok1 = einmessenLaufen(sim, z, 30.0f);      // t = 0 bis 7,5 (Hand dreht normal)
  laufe(sim, z, 4.0, 0.0);                        // t = 7,5 bis 11,5
  phase3Grad() = grad;
  phase3Art() = art;
  bool ok2 = einmessenLaufen(sim, z, 30.0f);      // t = 11,5 bis 19
  bool nochmal = ok2 ? false : einmessenAbschliessen(z, 30.0f);
  double refFlex = ok2 ? -20.0 : 0.0;             // bei Abbruch gilt die alte Referenz (0 Grad)
  Messreihe halten = laufe(sim, z, 6.0, 0.0, refFlex);    // t = 19 bis 25
  Messreihe bewegt = laufe(sim, z, 8.0, 0.0, refFlex);    // t = 25 bis 33
  char text[240];
  pruefe(ok1, "Vorbereitung: erstes Einmessen klappt");
  if (pflicht) {
    std::snprintf(text, sizeof(text), "%s: das Einmessen wird abgebrochen (liefert false)", kopf);
    pruefe(!ok2, text);
  }
  std::snprintf(text, sizeof(text), "%s: nach einem Abbruch liefert einmessenAbschliessen nie true", kopf);
  pruefe(!nochmal, text);
  std::snprintf(text, sizeof(text), "%s: die Lage bleibt bekannt (kein Unterbruch)", kopf);
  pruefe(halten.unbekannt == 0 && bewegt.unbekannt == 0, text);
  zeige(ok2 ? "neue Referenz, halten" : "alte Referenz, halten", halten);
  zeige(ok2 ? "neue Referenz, bewegt" : "alte Referenz, bewegt", bewegt);
  std::snprintf(text, sizeof(text), "%s: Winkel stimmt auf 2,5 Grad (bei Abbruch alte Referenz und Achse, sonst neue)", kopf);
  pruefe(halten.maxFehler <= 2.5 && bewegt.maxFehler <= 2.5, text);
}

// Ohne alte Referenz: nach dem Abbruch bleibt es bei "keine Abweichung".
void abbruchOhneReferenz(const char *kopf, double grad, int art) {
  Phase3Einstellung einst(grad, art);
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danachHalten);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(bew, einbauStandard(), fehlerNurRauschen(), 92);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  Messreihe m = laufe(sim, z, 5.0, 0.0);
  char text[240];
  std::snprintf(text, sizeof(text), "%s: Einmessen wird abgebrochen (liefert false)", kopf);
  pruefe(!ok, text);
  std::snprintf(text, sizeof(text), "%s: ohne frühere Referenz bleibt es bei 'keine Abweichung' (kein Alarm erlaubt)", kopf);
  pruefe(m.zyklen > 0 && m.unbekannt == m.zyklen, text);
}

// Hand-Drehung klappt: Einmessen liefert true, danach stimmt alles gleichzeitig (beugen 0 bis -100, Hand drehen,
// Oberarm drehen, schwenken, neigen) auf 2,5 Grad. Fünf Einbaulagen, mit Rauschen und Gyro-Offset.
void handKlappt(double grad) {
  Phase3Einstellung einst(grad, 0);
  std::string schlecht;
  std::string nichtOk;
  double schlimmster = 0.0;
  for (int k = 0; k < ANZAHL_EINBAU_H; k++) {
    Pose start = poseHaengend();
    start.twist = twistStart(k);
    Danach danach = [](double s, const Pose &ende) -> Pose { return alltagPose(ende, s, 70.0); };
    ArmSim sim(mitEinmessen(start, 70.0, 2, 0.0, 0.0, danach), einbauNr(k), fehlerNr(k), 100 + k);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    bool ok = einmessenLaufen(sim, z, 30.0f);
    if (!ok) { merke(nichtOk, k); continue; }
    Messreihe m = laufe(sim, z, 14.0, 0.0);
    if (m.maxFehler > schlimmster) schlimmster = m.maxFehler;
    if (!messOk(m)) merke(schlecht, k);
  }
  char text[240];
  std::printf("          Hand %.0f Grad: grösster Fehler %.3f Grad\n", grad, schlimmster);
  std::snprintf(text, sizeof(text), "AK-6/AK-7e: Hand %.0f Grad (hin und zurück): Einmessen klappt (nicht ok bei Einbau: %s)", grad, nichtOk.c_str());
  pruefe(nichtOk.empty(), text);
  std::snprintf(text, sizeof(text), "AK-6: nach Hand %.0f Grad stimmt der Winkel auf 2,5 Grad und ist immer bekannt (Fehler bei Einbau: %s)", grad, schlecht.c_str());
  pruefe(schlecht.empty(), text);
}

int main() {
  std::printf("REQ-002: Phase 3 (Hand drehen), AK-6 und AK-7e\n");

  std::printf("\n-- AK-7e: Hand wird nicht gedreht (höchstens 3 Grad): muss abbrechen\n");
  abbruchOhneReferenz("AK-7e: Hand 0 Grad, ohne alte Referenz", 0.0, 0);
  abbruchOhneReferenz("AK-7e: Hand 3 Grad, ohne alte Referenz", 3.0, 0);
  abbruchMitAlterReferenz("AK-7e: Hand 0 Grad, mit alter Referenz", 0.0, 0, true);
  abbruchMitAlterReferenz("AK-7e: Hand 3 Grad, mit alter Referenz", 3.0, 0, true);

  std::printf("\n-- AK-7e: falsche Bewegung (Unterarm beugen statt drehen): Achse liegt nicht quer zur Scharnier-Achse, muss abbrechen\n");
  abbruchOhneReferenz("AK-7e: Beugen statt Drehen (60 Grad), ohne alte Referenz", 60.0, 1);
  abbruchMitAlterReferenz("AK-7e: Beugen statt Drehen (60 Grad), mit alter Referenz", 60.0, 1, true);

  std::printf("\n-- AK-7e: Hand 20 und 30 Grad (zwischen 3 und 45 Grad): richtig oder Abbruch, nie falsch\n");
  abbruchMitAlterReferenz("AK-7e: Hand 20 Grad", 20.0, 0, false);
  abbruchMitAlterReferenz("AK-7e: Hand 30 Grad", 30.0, 0, false);

  std::printf("\n-- AK-6/AK-7e: Hand mindestens 45 Grad: muss klappen, danach AK-1 bis AK-4\n");
  handKlappt(45.0);
  handKlappt(60.0);
  handKlappt(90.0);

  return testErgebnis();
}
