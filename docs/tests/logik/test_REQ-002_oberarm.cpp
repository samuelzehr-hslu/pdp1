// test_REQ-002_oberarm.cpp
// Tests zu REQ-002, AK-7d (Abbruch, wenn die Scharnier-Achse nicht eindeutig bestimmbar ist: "Oberarm")
// und AK-6 (Einmessen: genau 2 Mal beugen + Oberarm in einem kleinen Kreis, dann Hand drehen = Phase 3). Stand: "Achse messen statt annehmen".
//
// Grundsatz der Anforderung: entweder ist die Achse richtig, oder das Einmessen bricht ab, das Gerät rät nie.
//   - Oberarm bewegt sich in Phase 2 höchstens 3 Grad (oder nur um die Senkrechte): Einmessen muss abbrechen.
//   - Oberarm macht einen kleinen Kreis von mindestens 15 Grad und der Unterarm beugt genau 2 Mal >= 30 Grad (Bedienung nach AK-6):
//     Einmessen muss klappen, danach müssen AK-1 bis AK-4 gelten (Winkel stimmt auf 2 Grad, auch bei gleichzeitigen Bewegungen).
//   - Reines Pendeln um EINE Achse und nur 1 Mal Beugen gehören nicht mehr zur Bedienung: Das Einmessen DARF klappen
//     oder mit 'Oberarm' abbrechen. Klappt es, muss der Winkel danach auf 2 Grad stimmen. Bricht es ab, muss die alte
//     Referenz bleiben (bzw. 'keine Abweichung', wenn es keine gab) und nie true beim zweiten Abschliessen kommen.
// Jeweils: ohne alte Referenz und mit alter Referenz (zweites Einmessen bei -20 Grad), mit 5 verschieden eingebauten IMUs,
// mit Sensor-Rauschen (bei zwei Einbauten zusätzlich Gyro-Offset +-2 Grad/s). Der Unterarm beugt immer 70 Grad.
// Der Meldungstext "Grund: Oberarm" steht nur in jointsense.ino und wird in Wokwi geprüft (docs/wokwi/REQ-002-stoerfaelle.md).

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"

// ======================================================================
// Bausteine
// ======================================================================

const int ANZAHL_EINBAU = 5;

// Fünf verschiedene Einbaulagen der IMUs (Knochenachse = Sensor-Achse 0/1/2, verschieden verdreht, einmal ganz zufällig).
Einbau einbauNr(int k) {
  if (k == 0) return einbauStandard();
  if (k == 1) return einbauMit(1, 100.0, 250.0);
  if (k == 2) return einbauMit(2, 200.0, 40.0);
  if (k == 3) return einbauMit(0, 300.0, 150.0);
  Zufall zf(555);
  Einbau e;
  e.oben = matMal(matMal(drehZ(360.0 * zf.gleich()), drehY(360.0 * zf.gleich())), drehX(360.0 * zf.gleich()));
  e.unten = matMal(matMal(drehZ(360.0 * zf.gleich()), drehY(360.0 * zf.gleich())), drehX(360.0 * zf.gleich()));
  return e;
}

// Oberarm in der Schulter gedreht (twist): ändert, in welche Richtung die Scharnier-Achse in der Welt zeigt.
// Bei 90 Grad steht die Beuge-Achse quer zur Pendel-Richtung des Oberarms, bei 0 Grad liegt sie in dieser Richtung.
double twistStart(int k) {
  return (k % 2 == 1) ? 90.0 : 0.0;
}

SensorFehler fehlerNr(int k) {
  if (k == 1) return fehlerMitOffset(0);
  if (k == 3) return fehlerMitOffset(1);
  return fehlerNurRauschen();
}

bool messOk(const Messreihe &m) {
  return m.unbekannt == 0 && m.maxFehler <= 2.5;
}

// Zeitplan "zwei Einmessungen": 0-7,5 s erstes Einmessen (gültig, Oberarm bewegt sich leicht mit, Hand dreht), 7,5-9,5 s Arm weich auf -20 Grad,
// 9,5-11,5 s halten, 11,5-13,5 s Phase 1 des zweiten Einmessens, 13,5-17,5 s Phase 2 (Oberarm: obArt/obGrad, Unterarm beugt 70 Grad),
// 17,5-19 s Phase 3 (Hand drehen), 19-29 s Arm bei -20 Grad, ab 29 s Beugen bis -90 Grad bei gleichzeitigem Unterarm-Drehen,
// Oberarm-Drehen, Schwenken und Neigen.
Bewegung zeitplanZweiMal(int obArt, double obGrad, double twist0, int zyklen2) {
  return [=](double t) -> Pose {
    Pose p = poseHaengend();
    p.twist = twist0;
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t < 9.5) { p.flex = -20.0 * weich((t - 7.5) / 2.0); return p; }
    p.flex = -20.0;
    if (t < 11.5) return p;
    if (einmessenPose(p, t, 11.5, 70.0, zyklen2, obArt, obGrad)) return p;
    if (t >= 29.0) {
      double d = t - 29.0;
      p.flex = -20.0 - 35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * d / 4.0));
      p.pron = 60.0 * std::sin(2.0 * PI_ZAHL * d / 5.0);
      p.twist = twist0 + 40.0 * std::sin(2.0 * PI_ZAHL * d / 7.0);
      p.yaw = 20.0 * d;
      p.pitch = 90.0 - 30.0 * (1.0 - std::cos(2.0 * PI_ZAHL * d / 6.5));
    }
    return p;
  };
}

struct ZweiMalErgebnis {
  bool ok1;       // erstes Einmessen
  bool ok2;       // zweites Einmessen
  bool nochmal;   // nochmaliges einmessenAbschliessen nach einem Abbruch (muss false sein)
  Messreihe b;    // 6 s nach dem zweiten Einmessen
  Messreihe h;    // 4 s
  Messreihe w;    // 8 s Bewegung (gleichzeitig beugen, drehen, schwenken)
};

// Läuft den Zeitplan. Misst danach gegen die Referenz, die gelten muss:
// bei Erfolg die neue (Arm bei -20 Grad), bei Abbruch die alte (Arm gestreckt, 0 Grad).
ZweiMalErgebnis laufeZweiMal(int k, int obArt, double obGrad, int zyklen2 = 2) {
  ZweiMalErgebnis r;
  ArmSim sim(zeitplanZweiMal(obArt, obGrad, twistStart(k), zyklen2), einbauNr(k), fehlerNr(k),
             static_cast<unsigned long long>(500 + k));
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  r.ok1 = einmessenLaufen(sim, z, 30.0f);   // t = 0 bis 7,5
  laufe(sim, z, 4.0, 0.0);                  // t = 7,5 bis 11,5
  r.ok2 = einmessenLaufen(sim, z, 30.0f);   // t = 11,5 bis 19
  r.nochmal = r.ok2 ? false : einmessenAbschliessen(z, 30.0f);
  double ref = r.ok2 ? -20.0 : 0.0;
  r.b = laufe(sim, z, 6.0, 0.0, ref);       // t = 19 bis 25
  r.h = laufe(sim, z, 4.0, 0.0, ref);       // t = 25 bis 29
  r.w = laufe(sim, z, 8.0, 0.0, ref);       // t = 29 bis 37
  return r;
}

void merke(std::string &liste, int k) {
  if (!liste.empty()) liste += ", ";
  liste += std::to_string(k);
}

// ======================================================================
// AK-7d: das Einmessen muss abbrechen
// ======================================================================

void abbruchGruppe(const char *kopf, int obArt, double obGrad) {
  std::string schlechtOhne;     // Einbauten, bei denen ohne alte Referenz etwas falsch lief
  std::string schlechtMit;      // dito mit alter Referenz
  std::string schlechtAchse;    // alte Achse/Referenz nach dem Abbruch nicht mehr richtig
  std::string vorbereitung;     // erstes Einmessen klappte nicht (dann ist der Test nicht aussagekräftig)
  for (int k = 0; k < ANZAHL_EINBAU; k++) {
    // ohne alte Referenz
    {
      Pose start = poseHaengend();
      start.twist = twistStart(k);
      Bewegung bew = mitEinmessenOberarm(start, 70.0, 2, 0.0, 0.0, obArt, obGrad, danachHalten);
      ArmSim sim(bew, einbauNr(k), fehlerNr(k), static_cast<unsigned long long>(400 + k));
      EllbogenZustand z;
      ellbogenZuruecksetzen(z);
      bool ok = einmessenLaufen(sim, z, 30.0f);
      bool nochmal = einmessenAbschliessen(z, 30.0f);
      Messreihe m = laufe(sim, z, 6.0, 0.0);
      if (ok || nochmal || m.zyklen == 0 || m.unbekannt != m.zyklen) merke(schlechtOhne, k);
    }
    // mit alter Referenz und alter Achse
    {
      ZweiMalErgebnis r = laufeZweiMal(k, obArt, obGrad);
      if (!r.ok1) merke(vorbereitung, k);
      if (r.ok2 || r.nochmal) merke(schlechtMit, k);
      if (!messOk(r.b) || !messOk(r.h) || !messOk(r.w)) {
        merke(schlechtAchse, k);
        std::printf("          Einbau %d: nach Abbruch: maxFehler %.2f / %.2f / %.2f Grad, unbekannt %d / %d / %d\n", k,
                    r.b.maxFehler, r.h.maxFehler, r.w.maxFehler, r.b.unbekannt, r.h.unbekannt, r.w.unbekannt);
      }
    }
  }
  char text[300];
  std::snprintf(text, sizeof(text), "%s: Vorbereitung: das erste (gültige) Einmessen klappt bei allen 5 Einbauten (Fehler bei: %s)",
                kopf, vorbereitung.c_str());
  pruefe(vorbereitung.empty(), text);
  std::snprintf(text, sizeof(text),
                "%s, ohne alte Referenz: Einmessen bricht ab (false), nie true beim zweiten Abschliessen, danach 'keine Abweichung' (Fehler bei Einbau: %s)",
                kopf, schlechtOhne.c_str());
  pruefe(schlechtOhne.empty(), text);
  std::snprintf(text, sizeof(text), "%s, mit alter Referenz: Einmessen bricht ab (false), nie true beim zweiten Abschliessen (Fehler bei Einbau: %s)",
                kopf, schlechtMit.c_str());
  pruefe(schlechtMit.empty(), text);
  std::snprintf(text, sizeof(text),
                "%s, mit alter Referenz: alte Referenz und Achse bleiben, Lage immer bekannt, Fehler <= 2,5 Grad auch bei gleichzeitigen Bewegungen (Fehler bei Einbau: %s)",
                kopf, schlechtAchse.c_str());
  pruefe(schlechtAchse.empty(), text);
}

// ======================================================================
// AK-6 / AK-7d: das Einmessen muss klappen und danach muss die Achse stimmen
// ======================================================================

void klapptGruppe(const char *kopf, int obArt, double obGrad) {
  std::string schlechtOhne;
  std::string schlechtMit;
  for (int k = 0; k < ANZAHL_EINBAU; k++) {
    // ohne alte Referenz: danach alles gleichzeitig (beugen 0 bis -100, Hand drehen, Oberarm drehen, schwenken, neigen)
    {
      Pose start = poseHaengend();
      start.twist = twistStart(k);
      Danach danach = [](double s, const Pose &ende) -> Pose { return alltagPose(ende, s, 70.0); };
      Bewegung bew = mitEinmessenOberarm(start, 70.0, 2, 0.0, 0.0, obArt, obGrad, danach);
      ArmSim sim(bew, einbauNr(k), fehlerNr(k), static_cast<unsigned long long>(600 + k));
      EllbogenZustand z;
      ellbogenZuruecksetzen(z);
      bool ok = einmessenLaufen(sim, z, 30.0f);
      Messreihe m = laufe(sim, z, 8.0, 0.0);
      if (!ok || !messOk(m)) {
        merke(schlechtOhne, k);
        std::printf("          ohne alte Referenz, Einbau %d: einmessen %s, maxFehler %.2f Grad, unbekannt %d\n", k,
                    ok ? "ok" : "ABBRUCH", m.maxFehler, m.unbekannt);
      }
    }
    // mit alter Referenz: neue Referenz bei -20 Grad
    {
      ZweiMalErgebnis r = laufeZweiMal(k, obArt, obGrad);
      if (!r.ok1 || !r.ok2 || !messOk(r.b) || !messOk(r.h) || !messOk(r.w)) {
        merke(schlechtMit, k);
        std::printf("          mit alter Referenz, Einbau %d: ok1 %d, ok2 %d, maxFehler %.2f / %.2f / %.2f Grad\n", k,
                    r.ok1 ? 1 : 0, r.ok2 ? 1 : 0, r.b.maxFehler, r.h.maxFehler, r.w.maxFehler);
      }
    }
  }
  char text[300];
  std::snprintf(text, sizeof(text),
                "%s, ohne alte Referenz: Einmessen klappt und der Winkel stimmt danach auf 2,5 Grad (Fehler bei Einbau: %s)",
                kopf, schlechtOhne.c_str());
  pruefe(schlechtOhne.empty(), text);
  std::snprintf(text, sizeof(text),
                "%s, mit alter Referenz: Einmessen klappt, neue Referenz, Winkel stimmt danach auf 2,5 Grad (Fehler bei Einbau: %s)",
                kopf, schlechtMit.c_str());
  pruefe(schlechtMit.empty(), text);
}

// ======================================================================
// Bewegung, die nicht mehr zur Bedienung gehört: das Einmessen DARF klappen oder abbrechen
// ======================================================================

// zyklen = Anzahl Beugungen in Phase 2 (hier 1 Mal ist erlaubt, gehört aber nicht mehr zur Bedienung).
// Klappt es: Winkel stimmt danach auf 2,5 Grad. Bricht es ab: alte Referenz bleibt, nie true beim zweiten Abschliessen.
void toleriertGruppe(const char *kopf, int obArt, double obGrad, int zyklen) {
  std::string schlechtOhne;     // ohne alte Referenz: etwas Falsches (Erfolg mit falschem Winkel, oder Abbruch ohne 'keine Abweichung')
  std::string schlechtMit;      // mit alter Referenz: dito
  std::string vorbereitung;
  int klapptOhne = 0;
  int klapptMit = 0;
  for (int k = 0; k < ANZAHL_EINBAU; k++) {
    // ohne alte Referenz
    {
      Pose start = poseHaengend();
      start.twist = twistStart(k);
      Danach danach = [](double s, const Pose &ende) -> Pose { return alltagPose(ende, s, 70.0); };
      Bewegung bew = mitEinmessenOberarm(start, 70.0, zyklen, 0.0, 0.0, obArt, obGrad, danach);
      ArmSim sim(bew, einbauNr(k), fehlerNr(k), static_cast<unsigned long long>(800 + k));
      EllbogenZustand z;
      ellbogenZuruecksetzen(z);
      bool ok = einmessenLaufen(sim, z, 30.0f);
      bool nochmal = ok ? false : einmessenAbschliessen(z, 30.0f);
      Messreihe m = laufe(sim, z, 8.0, 0.0);
      if (ok) {
        klapptOhne++;
        if (!messOk(m)) {
          merke(schlechtOhne, k);
          std::printf("          ohne alte Referenz, Einbau %d: Einmessen klappte, aber maxFehler %.2f Grad, unbekannt %d\n", k,
                      m.maxFehler, m.unbekannt);
        }
      } else if (nochmal || m.zyklen == 0 || m.unbekannt != m.zyklen) {
        merke(schlechtOhne, k);
      }
    }
    // mit alter Referenz (zweites Einmessen bei -20 Grad)
    {
      ZweiMalErgebnis r = laufeZweiMal(k, obArt, obGrad, zyklen);
      if (!r.ok1) merke(vorbereitung, k);
      if (r.ok2) klapptMit++;
      if (r.nochmal || !messOk(r.b) || !messOk(r.h) || !messOk(r.w)) {
        merke(schlechtMit, k);
        std::printf("          mit alter Referenz, Einbau %d: ok2 %d, maxFehler %.2f / %.2f / %.2f Grad, unbekannt %d / %d / %d\n", k,
                    r.ok2 ? 1 : 0, r.b.maxFehler, r.h.maxFehler, r.w.maxFehler, r.b.unbekannt, r.h.unbekannt, r.w.unbekannt);
      }
    }
  }
  std::printf("          Info: Einmessen klappte ohne alte Referenz in %d von %d, mit alter Referenz in %d von %d Fällen\n",
              klapptOhne, ANZAHL_EINBAU, klapptMit, ANZAHL_EINBAU);
  char text[300];
  std::snprintf(text, sizeof(text), "%s: Vorbereitung: das erste (gültige) Einmessen klappt bei allen 5 Einbauten (Fehler bei: %s)",
                kopf, vorbereitung.c_str());
  pruefe(vorbereitung.empty(), text);
  std::snprintf(text, sizeof(text),
                "%s, ohne alte Referenz: klappt (dann Winkel auf 2,5 Grad) oder bricht sauber ab (dann 'keine Abweichung') (Fehler bei Einbau: %s)",
                kopf, schlechtOhne.c_str());
  pruefe(schlechtOhne.empty(), text);
  std::snprintf(text, sizeof(text),
                "%s, mit alter Referenz: klappt (neue Referenz, Winkel auf 2,5 Grad) oder bricht ab (alte Referenz bleibt, Winkel auf 2,5 Grad) (Fehler bei Einbau: %s)",
                kopf, schlechtMit.c_str());
  pruefe(schlechtMit.empty(), text);
}

// Nach einem Abbruch (d) darf man es nochmal versuchen: mit richtiger Bewegung klappt es (kein "hängender" Zustand).
void wiederholungNachAbbruch() {
  std::string schlecht;
  for (int k = 0; k < ANZAHL_EINBAU; k++) {
    double tw = twistStart(k);
    Bewegung bew = [=](double t) -> Pose {
      Pose p = poseHaengend();
      p.twist = tw;
      // 0-7,5 s erstes Einmessen (Oberarm hängt still, Hand dreht: Abbruch nur wegen d), 7,5-9,5 s warten,
      // 9,5-17 s zweites Einmessen mit 1 Oberarm-Kreis und Hand-Drehung, danach alles gleichzeitig
      if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_NICHTS, 0.0)) return p;
      if (t < 9.5) return p;
      if (einmessenPose(p, t, 9.5, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
      return alltagPose(p, t - 17.0, 70.0);
    };
    ArmSim sim(bew, einbauNr(k), fehlerNr(k), static_cast<unsigned long long>(700 + k));
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    bool ok1 = einmessenLaufen(sim, z, 30.0f);   // t = 0 bis 7,5, muss abbrechen
    laufe(sim, z, 2.0, 0.0);                     // t = 7,5 bis 9,5
    bool ok2 = einmessenLaufen(sim, z, 30.0f);   // t = 9,5 bis 17, muss klappen
    Messreihe m = laufe(sim, z, 8.0, 0.0);
    if (ok1 || !ok2 || !messOk(m)) merke(schlecht, k);
  }
  char text[300];
  std::snprintf(text, sizeof(text),
                "AK-7d: erst Abbruch (Oberarm still), dann zweiter Versuch mit Oberarm-Bewegung klappt und stimmt auf 2,5 Grad (Fehler bei Einbau: %s)",
                schlecht.c_str());
  pruefe(schlecht.empty(), text);
}

// ======================================================================

int main() {
  std::printf("REQ-002: Einmessen mit Oberarm-Bewegung (AK-6) und Abbruch 'Oberarm' (AK-7d)\n");

  std::printf("\n-- AK-7d: Oberarm bewegt sich nicht (hoechstens 3 Grad) oder nur um die Senkrechte: Abbruch\n");
  abbruchGruppe("AK-7d: Oberarm haengt still (0 Grad)", OBERARM_NICHTS, 0.0);
  abbruchGruppe("AK-7d: Oberarm bewegt sich nur 3 Grad", OBERARM_PENDELN, 3.0);
  abbruchGruppe("AK-7d: Oberarm nur um die Senkrechte (Schwenken + Schulterdrehung, je 60 Grad)", OBERARM_SENKRECHT, 60.0);

  std::printf("\n-- AK-6: genau 2 Mal beugen + ein kleiner Kreis des Oberarms (mindestens 15 Grad): Einmessen muss klappen\n");
  klapptGruppe("AK-6: 2 Mal beugen + Oberarm macht 1 kleinen Kreis, 15 Grad (Minimum)", OBERARM_KREISE, 15.0);
  klapptGruppe("AK-6: 2 Mal beugen + Oberarm macht 1 kleinen Kreis, 20 Grad", OBERARM_KREISE, 20.0);

  std::printf("\n-- Gehört nicht mehr zur Bedienung (Pendeln um eine Achse, nur 1 Mal beugen): darf klappen oder abbrechen\n");
  toleriertGruppe("Pendeln um eine Achse, 15 Grad, 2 Mal beugen", OBERARM_PENDELN, 15.0, 2);
  toleriertGruppe("Pendeln um eine Achse, 30 Grad, 2 Mal beugen", OBERARM_PENDELN, 30.0, 2);
  toleriertGruppe("nur 1 Mal beugen, Oberarm macht 1 kleinen Kreis, 20 Grad", OBERARM_KREISE, 20.0, 1);
  toleriertGruppe("nur 1 Mal beugen, Oberarm pendelt 20 Grad", OBERARM_PENDELN, 20.0, 1);

  std::printf("\n-- AK-7d: Wiederholung nach Abbruch\n");
  wiederholungNachAbbruch();

  return testErgebnis();
}
