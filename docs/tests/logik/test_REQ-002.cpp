// test_REQ-002.cpp
// Tests zu REQ-002 (Winkelmessung unabhängig von Drehung): AK-1 bis AK-9, AK-13, AK-14 und Fuzz.
// Stand Runde 3 (Team-Entscheidung): Einmess-Bewegung = genau 2 Mal beugen (je mindestens 30 Grad Spannweite) und dabei der
// Oberarm in kleinen Kreisen (20 Grad, verlangt sind mindestens 15), eingemessen wird im Hängen mit lockerem Arm (siehe
// mitEinmessen in sim_ellbogen.h). AK-1 bis AK-4 gelten NACH dem Einmessen in jeder Armlage (hängend, vorn, oben, seitlich),
// gemessen mit Sensor-Rauschen. Am Ende von main steht die Reserve (grösster Fehler gegenüber der Grenze von 2 Grad).
// Die Tests zu AK-7d (Abbruch 'Oberarm') und 'darf klappen oder Oberarm' stehen in test_REQ-002_oberarm.cpp.
// Die Drift-Tests (AK-10 bis AK-12, je 10 Minuten simuliert) stehen in test_REQ-002_drift.cpp,
// damit jede Datei unter dem 60-Sekunden-Limit von alle_tests.py bleibt.
//
// Der Simulator (sim_ellbogen.h) erzeugt aus einer wahren Arm-Bewegung die Sensorwerte beider IMUs.
// Die echte Logik (logik.cpp) muss daraus den Ellbogenwinkel finden. Die Logik wird hier nicht nachgebaut.
//
// Nicht auf dem PC prüfbar: AK-15 (Rechenzeit über die Zeile "Zyklus max" in Wokwi, RAM über die Compile-Ausgabe),
// Übersteuerung der Gyro-Rohwerte (AK-14, nur in jointsense.ino), Vibrationsstösse und Meldungstexte (Wokwi).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <vector>
#include "test_hilfe.h"
#include "logik.h"
#include "sim_ellbogen.h"

// ======================================================================
// Gemeinsame Bausteine
// ======================================================================

Bewegung standardBewegung() {
  Danach danach = [](double s, const Pose &ende) -> Pose { return alltagPose(ende, s, 70.0); };
  return mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
}

// Zielwinkel nacheinander anfahren: alle 3 s ein neues Ziel (1 s weich bewegen, 2 s halten).
double flexFolge(const double *ziele, int n, double s) {
  int i = static_cast<int>(std::floor(s / 3.0));
  if (i >= n - 1) return ziele[n - 1];
  double r = weich((s - 3.0 * i) / 1.0);
  return ziele[i] + (ziele[i + 1] - ziele[i]) * r;
}

// Misst nach dem Einmessen (bei ziele[0]) alle weiteren Ziele. mittel[j] = Mittelwert im Halten bei ziele[j].
void folgeMessen(const double *ziele, int n, const SensorFehler &fe, unsigned long long samen,
                 double *mittel, int *unbekannt) {
  std::vector<double> kopie(ziele, ziele + n);
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = ende;
    p.flex = flexFolge(&kopie[0], n, s);
    return p;
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  ArmSim sim(bew, einbauStandard(), fe, samen);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  einmessenLaufen(sim, z, 30.0f);
  mittel[0] = 0.0;
  for (int j = 1; j < n; j++) {
    Messreihe a = laufe(sim, z, 1.5, 0.0);   // bewegen und einschwingen
    Messreihe b = laufe(sim, z, 1.5, 0.0);   // halten und messen
    mittel[j] = b.mittel;
    *unbekannt += a.unbekannt + b.unbekannt;
  }
}

// ======================================================================
// AK-1 bis AK-3: Drehung ändert den Winkel nicht
// ======================================================================

// Arm-Lagen. Eingemessen wird immer im Hängen mit lockerem Arm (AK-6). Danach geht der Arm in die Lage.
//  yaw = ganzer Arm um die Senkrechte, pitch = Neigung des Oberarms (90 hängend, 0 nach vorn, -90 nach oben),
//  seit = Oberarm zur Seite abgespreizt (pitch 90 + seit 90 = Arm waagrecht zur Seite).
struct Lage {
  const char *name;
  double yaw;
  double pitch;
  double seit;
};
const Lage LAGE_HAENGEND = {"haengend", 0.0, 90.0, 0.0};
const Lage LAGE_VORN = {"vorn", 0.0, 0.0, 0.0};
const Lage LAGE_OBEN = {"oben", 0.0, -90.0, 0.0};
const Lage LAGE_SEITLICH = {"seitlich", 0.0, 90.0, 90.0};
const Lage LAGE_SCHRAEG = {"schraeg", 0.0, 45.0, 0.0};

// Grösste Fehler über alle Läufe (für die Reserve im Bericht, gedruckt am Ende von main).
double g_groessteAenderungAK1bis3 = 0.0;   // grösste Winkeländerung bei Drehung (Grenze 2 Grad)
double g_groessterFehlerAK4Lagen = 0.0;    // grösster Fehler zum echten Winkel in AK-4, Lagen-Teil (Grenze 2 Grad)
double g_groessterFehlerAK4Einbau = 0.0;   // grösster Fehler zum echten Winkel in AK-4, Einbaudrehung (Grenze 2,5 Grad)

// art: 0 = Unterarm drehen, 1 = Oberarm in der Schulter drehen, 2 = ganzer Arm schwenkt waagrecht.
// Ablauf: Einmessen im Hängen (2 Mal beugen + Oberarm-Kreise), dann Ellbogen weich auf flexZiel und der Arm
// weich in die Lage (zusammen 2 s), 0,5 s einschwingen, 1 s Grundwert messen, dann 8 s drehen.
// Mit Sensor-Rauschen (Gyro 0,1 Grad/s, 0,01 g).
void drehungPruefen(int art, double flexZiel, const Lage &lageRef, const char *kopf) {
  static unsigned long long samenZaehler = 10;
  samenZaehler++;
  const Lage lage = lageRef;
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = ende;
    double r = weich(s / 2.0);
    p.flex = ende.flex + flexZiel * r;
    p.yaw = ende.yaw + (lage.yaw - ende.yaw) * r;
    p.pitch = ende.pitch + (lage.pitch - ende.pitch) * r;
    p.seit = ende.seit + (lage.seit - ende.seit) * r;
    double d = s - 3.5;
    if (d > 0.0 && d < 8.0) {
      double w = 90.0 * std::sin(2.0 * PI_ZAHL * d / 8.0);   // 0 -> +90 -> 0 -> -90 -> 0
      if (art == 0) p.pron = ende.pron + w;
      if (art == 1) p.twist = ende.twist + w;
      if (art == 2) p.yaw = lage.yaw + 360.0 * d / 8.0;      // einmal rundherum
    }
    if (d >= 8.0 && art == 2) p.yaw = lage.yaw + 360.0;
    return p;
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(bew, einbauStandard(), fehlerNurRauschen(), samenZaehler);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  laufe(sim, z, 2.5, 0.0);                          // Ellbogen und Arm bewegen, einschwingen
  Messreihe vor = laufe(sim, z, 1.0, 0.0);          // Grundwert
  Messreihe dreh = laufe(sim, z, 8.0, vor.mittel);  // Drehung

  char text[240];
  const int flexInt = static_cast<int>(flexZiel);
  std::snprintf(text, sizeof(text), "%s: Einmessen im Hängen klappt (Ellbogen %d Grad, Arm %s)", kopf, flexInt, lage.name);
  pruefe(ok, text);
  std::snprintf(text, sizeof(text), "%s: Winkel immer bekannt (Ellbogen %d Grad, Arm %s)", kopf, flexInt, lage.name);
  pruefe(vor.unbekannt == 0 && dreh.unbekannt == 0, text);
  std::snprintf(text, sizeof(text), "%s: Grundwert vor der Drehung stimmt auf 2,5 Grad (Ellbogen %d Grad, Arm %s)", kopf, flexInt, lage.name);
  pruefeNahe(vor.mittel, flexZiel, 2.5, text);
  std::snprintf(text, sizeof(text), "%s: Winkeländerung bei Drehung <= 2 Grad (Ellbogen %d Grad, Arm %s)", kopf, flexInt, lage.name);
  zeige("Drehung", dreh);
  if (dreh.maxAenderung > g_groessteAenderungAK1bis3) g_groessteAenderungAK1bis3 = dreh.maxAenderung;
  pruefe(dreh.maxAenderung <= 2.0, text);
  std::snprintf(text, sizeof(text), "%s: kein Alarm (Wert immer unter 5 Grad) (Ellbogen %d Grad, Arm %s)", kopf, flexInt, lage.name);
  pruefe(dreh.maxAbw < 5.0, text);
}

void testAK1bis3() {
  const double flex1[3] = {0.0, -30.0, -90.0};
  std::printf("\n-- AK-1: Unterarm drehen (+-90 Grad um die Unterarm-Achse), Arm haengend / vorn / oben / seitlich\n");
  const Lage *lagen1[4] = {&LAGE_HAENGEND, &LAGE_VORN, &LAGE_OBEN, &LAGE_SEITLICH};
  for (int i = 0; i < 3; i++) {
    for (int k = 0; k < 4; k++) {
      drehungPruefen(0, flex1[i], *lagen1[k], "AK-1");
    }
  }
  std::printf("\n-- AK-2: Oberarm in der Schulter drehen (+-90 Grad)\n");
  const double flex2[2] = {0.0, -90.0};
  for (int i = 0; i < 2; i++) {
    drehungPruefen(1, flex2[i], LAGE_HAENGEND, "AK-2");
    drehungPruefen(1, flex2[i], LAGE_VORN, "AK-2");
  }
  std::printf("\n-- AK-3: ganzer Arm schwenkt waagrecht (0 bis 360 Grad um die Senkrechte)\n");
  for (int i = 0; i < 3; i++) {
    drehungPruefen(2, flex1[i], LAGE_VORN, "AK-3");      // Arm nach vorn
    drehungPruefen(2, flex1[i], LAGE_SCHRAEG, "AK-3");   // Arm schräg nach unten
  }
}

// ======================================================================
// AK-4: jede Lage, jede Einbaudrehung
// ======================================================================

// Eingemessen wird im Hängen mit lockerem Arm. Danach nacheinander (je 5 s): hängend, nach vorn, nach oben,
// seitlich abgespreizt, nach vorn und 90 Grad geschwenkt, wieder hängend. Mit Sensor-Rauschen.
void lagenPruefen(double flexZiel) {
  const double yawL[6] = {0.0, 0.0, 0.0, 0.0, 90.0, 0.0};
  const double pitchL[6] = {90.0, 0.0, -90.0, 90.0, 0.0, 90.0};
  const double seitL[6] = {0.0, 0.0, 0.0, 90.0, 0.0, 0.0};
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    Pose p = ende;
    p.flex = ende.flex + flexZiel * weich(s / 2.0);
    int von = 0;
    double r = 0.0;
    if (s >= 5.0) {
      von = static_cast<int>((s - 5.0) / 5.0);
      if (von > 4) von = 4;
      r = weich((s - 5.0 - 5.0 * von) / 2.0);
    }
    p.yaw = yawL[von] + (yawL[von + 1] - yawL[von]) * r;
    p.pitch = pitchL[von] + (pitchL[von + 1] - pitchL[von]) * r;
    p.seit = seitL[von] + (seitL[von + 1] - seitL[von]) * r;
    return p;
  };
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(bew, einbauStandard(), fehlerNurRauschen(), 12);   // mit Sensor-Rauschen (Gyro 0,1 Grad/s, 0,01 g)
  bool ok = einmessenLaufen(sim, z, 30.0f);
  int unbek = 0;
  double mittel[6];
  for (int k = 0; k < 6; k++) {
    // Lage k wird nach (k * 5 + 4) s des Danach-Teils gemessen (letzte Sekunde des Haltens).
    Messreihe weg = laufe(sim, z, 4.0, 0.0);
    Messreihe m = laufe(sim, z, 1.0, 0.0);
    unbek += weg.unbekannt + m.unbekannt;
    mittel[k] = m.mittel;
  }
  double kleinster = mittel[0];
  double groesster = mittel[0];
  double schlimmster = 0.0;
  for (int k = 0; k < 6; k++) {
    if (mittel[k] < kleinster) kleinster = mittel[k];
    if (mittel[k] > groesster) groesster = mittel[k];
    if (std::fabs(mittel[k] - flexZiel) > schlimmster) schlimmster = std::fabs(mittel[k] - flexZiel);
  }
  if (schlimmster > g_groessterFehlerAK4Lagen) g_groessterFehlerAK4Lagen = schlimmster;
  char text[220];
  std::snprintf(text, sizeof(text), "AK-4: Lagen: Einmessen im Hängen klappt (Ellbogen %d Grad)", static_cast<int>(flexZiel));
  pruefe(ok, text);
  std::snprintf(text, sizeof(text), "AK-4: Lagen: Winkel immer bekannt (Ellbogen %d Grad)", static_cast<int>(flexZiel));
  pruefe(unbek == 0, text);
  std::snprintf(text, sizeof(text),
                "AK-4: Lagen hängend/vorn/oben/seitlich: gleicher Winkel auf 2 Grad (Ellbogen %d Grad)",
                static_cast<int>(flexZiel));
  std::printf("          Unterschied zwischen den Lagen = %.3f Grad\n", groesster - kleinster);
  pruefe(groesster - kleinster <= 2.0, text);
  std::snprintf(text, sizeof(text), "AK-4: Lagen: jede Lage stimmt auf 2 Grad zum echten Winkel (Ellbogen %d Grad)",
                static_cast<int>(flexZiel));
  std::printf("          grösste Abweichung zum echten Winkel = %.3f Grad\n", schlimmster);
  pruefe(schlimmster <= 2.0, text);
}

// Beide IMUs beliebig um die Knochenachse gedreht (0 bis 360 Grad) vor dem Einmessen.
// achse: welche Sensor-Achse entlang des Knochens zeigt (der Text der Anforderung sagt das nicht, darum alle drei).
void einbauPruefen(int achse) {
  const double winkel[5] = {0.0, 72.0, 144.0, 216.0, 288.0};
  double schlimmster = 0.0;
  int schlecht = 0;
  int unbek = 0;
  int nichtEingemessen = 0;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      Bewegung bew = standardBewegung();
      ArmSim sim(bew, einbauMit(achse, winkel[i], winkel[j]), fehlerNurRauschen(),   // mit Sensor-Rauschen
                 static_cast<unsigned long long>(100 + i * 5 + j));
      EllbogenZustand z;
      ellbogenZuruecksetzen(z);
      if (!einmessenLaufen(sim, z, 30.0f)) {
        nichtEingemessen++;
        continue;
      }
      Messreihe m = laufe(sim, z, 8.0, 0.0);
      unbek += m.unbekannt;
      if (m.maxFehler > schlimmster) schlimmster = m.maxFehler;
      if (m.maxFehler > 2.5) schlecht++;
    }
  }
  if (schlimmster > g_groessterFehlerAK4Einbau) g_groessterFehlerAK4Einbau = schlimmster;
  char text[220];
  std::snprintf(text, sizeof(text), "AK-4: Einbau (Knochenachse = Sensor-Achse %d): alle 25 Drehungen lassen sich einmessen", achse);
  pruefe(nichtEingemessen == 0, text);
  std::snprintf(text, sizeof(text), "AK-4: Einbau (Knochenachse = Sensor-Achse %d): Winkel immer bekannt", achse);
  pruefe(unbek == 0, text);
  std::snprintf(text, sizeof(text), "AK-4: Einbau (Knochenachse = Sensor-Achse %d): in allen 25 Drehungen Fehler <= 2,5 Grad", achse);
  std::printf("          grösster Fehler = %.3f Grad, Fälle mit Fehler > 2 Grad: %d von 25\n", schlimmster, schlecht);
  pruefe(schlecht == 0, text);
}

// Beschreibung der Anforderung: "egal, wie die IMUs im Sleeve sitzen" (beliebige Einbaulage, zufällig).
void beliebigerEinbau() {
  Zufall zufall(777);
  int schlecht = 0;
  int nichtEingemessen = 0;
  double schlimmster = 0.0;
  for (int i = 0; i < 12; i++) {
    Einbau e;
    e.oben = matMal(matMal(drehZ(360.0 * zufall.gleich()), drehY(360.0 * zufall.gleich())), drehX(360.0 * zufall.gleich()));
    e.unten = matMal(matMal(drehZ(360.0 * zufall.gleich()), drehY(360.0 * zufall.gleich())), drehX(360.0 * zufall.gleich()));
    ArmSim sim(standardBewegung(), e, fehlerNurRauschen(), static_cast<unsigned long long>(200 + i));   // mit Sensor-Rauschen
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    if (!einmessenLaufen(sim, z, 30.0f)) {
      nichtEingemessen++;
      continue;
    }
    Messreihe m = laufe(sim, z, 8.0, 0.0);
    if (m.maxFehler > schlimmster) schlimmster = m.maxFehler;
    if (m.maxFehler > 2.5 || m.unbekannt > 0) schlecht++;
  }
  if (schlimmster > g_groessterFehlerAK4Einbau) g_groessterFehlerAK4Einbau = schlimmster;
  pruefe(nichtEingemessen == 0, "Beschreibung: bei 12 zufälligen Einbaulagen klappt das Einmessen");
  std::printf("          grösster Fehler = %.3f Grad\n", schlimmster);
  pruefe(schlecht == 0, "Beschreibung (kein AK): bei 12 zufälligen Einbaulagen ist der Fehler <= 2,5 Grad");
}

// ======================================================================
// AK-5: Messbereich, Auflösung, Erkennung
// ======================================================================

double erkennungsrate(double ziel, bool erwarteUeber) {
  int treffer = 0;
  int gesamt = 0;
  for (int i = 0; i < 20; i++) {
    Danach danach = [=](double s, const Pose &ende) -> Pose {
      Pose p = ende;
      p.flex = ende.flex + ziel * weich(s / 1.0);
      return p;
    };
    Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
    ArmSim sim(bew, einbauStandard(), fehlerNurRauschen(), static_cast<unsigned long long>(300 + i));
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    if (!einmessenLaufen(sim, z, 30.0f)) {
      gesamt += 75;
      continue;
    }
    laufe(sim, z, 2.0, 0.0);
    for (int k = 0; k < 75; k++) {
      float a = 0.0f;
      bool bek = zyklusUndLesen(sim, z, a);
      gesamt++;
      bool ueber = bek && a > 5.0f;
      if (bek && ueber == erwarteUeber) treffer++;
    }
  }
  return static_cast<double>(treffer) / static_cast<double>(gesamt);
}

void testAK5() {
  std::printf("\n-- AK-5: Messbereich -100 bis +15 Grad, Auflösung, Erkennung +-2 Grad\n");
  const double bereich[7] = {0.0, -100.0, -50.0, 0.0, 5.0, 10.0, 15.0};
  double mittel[16];
  int unbek = 0;
  folgeMessen(bereich, 7, fehlerNurRauschen(), 41, mittel, &unbek);
  for (int j = 1; j < 7; j++) {
    char text[160];
    std::snprintf(text, sizeof(text), "AK-5: Messbereich: echter Winkel %d Grad, gemessen auf 2 Grad genau",
                  static_cast<int>(bereich[j]));
    pruefeNahe(mittel[j], bereich[j], 2.0, text);
  }
  pruefe(unbek == 0, "AK-5: Messbereich: Winkel im ganzen Bereich immer bekannt");

  // Auflösung: 1 Grad echte Änderung = 0,5 bis 1,5 Grad Änderung im Wert.
  const double paare[11] = {0.0, -90.0, -89.0, -30.0, -29.0, 0.0, 1.0, 10.0, 11.0, 14.0, 15.0};
  double m2[16];
  int unbek2 = 0;
  folgeMessen(paare, 11, fehlerNurRauschen(), 42, m2, &unbek2);
  for (int j = 1; j < 11; j += 2) {
    char text[160];
    std::snprintf(text, sizeof(text), "AK-5: Auflösung: 1 Grad echte Änderung bei %d Grad ergibt 0,5 bis 1,5 Grad im Wert",
                  static_cast<int>(paare[j]));
    double d = m2[j + 1] - m2[j];
    pruefe(d >= 0.5 && d <= 1.5, text);
    if (d < 0.5 || d > 1.5) std::printf("          Änderung im Wert = %.3f Grad\n", d);
  }
  pruefe(unbek2 == 0, "AK-5: Auflösung: Winkel immer bekannt");

  // Erkennungsrate bei ruhiger Haltung, Grenzwert 5 Grad.
  double ueber = erkennungsrate(7.0, true);
  double drunter = erkennungsrate(3.0, false);
  std::printf("          Erkennungsrate bei +7 Grad: %.3f, bei +3 Grad (kein Alarm): %.3f\n", ueber, drunter);
  pruefe(ueber >= 0.95, "AK-5: bei echten +7 Grad (Grenzwert 5 + 2) liegt der Wert in >= 95 % der Zyklen über 5 Grad");
  pruefe(drunter >= 0.95, "AK-5: bei echten +3 Grad (Grenzwert 5 - 2) liegt der Wert in >= 95 % der Zyklen nicht über 5 Grad");
}

// ======================================================================
// AK-6: Einmessen (Logik-Teil: Phasen, Referenz, Achse)
// ======================================================================

void testAK6() {
  std::printf("\n-- AK-6: Einmessen\n");
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 61);
  float a = 0.0f;
  einmessenStart(z);
  int bekanntWaehrend = 0;
  for (int i = 0; i < 100; i++) {
    if (zyklusUndLesen(sim, z, a)) bekanntWaehrend++;
  }
  einmessenBewegungStart(z);
  for (int i = 0; i < 200; i++) {
    if (zyklusUndLesen(sim, z, a)) bekanntWaehrend++;
  }
  einmessenDrehungStart(z);   // Phase 3: Hand drehen (1500 ms)
  for (int i = 0; i < 75; i++) {
    if (zyklusUndLesen(sim, z, a)) bekanntWaehrend++;
  }
  bool ok = einmessenAbschliessen(z, 30.0f);
  pruefe(bekanntWaehrend == 0, "AK-6: Beim ersten Einmessen gibt es vor dem Abschluss noch keine Abweichung (keine Referenz)");
  pruefe(ok, "AK-6: Einmessen mit 2 s still + 4 s Beugen/Strecken (70 Grad, 2 Mal) + 1,5 s Hand drehen liefert true");
  float danach = 99.0f;
  bool bekannt = ellbogenAbweichung(z, danach);
  pruefe(bekannt, "AK-6: Nach erfolgreichem Einmessen ist die Abweichung bekannt (Lage bekannt)");
  pruefeNahe(danach, 0.0, 2.5, "AK-6: Referenz = Mittelwert der Ruhephase: gestreckter Arm zeigt 0 Grad");

  // Beugung zeigt negative Werte (Vorzeichen: negativ = Beugung).
  Messreihe m = laufe(sim, z, 3.0, 0.0);
  pruefe(m.unbekannt == 0, "AK-6: nach dem Einmessen ist der Winkel in der folgenden Bewegung immer bekannt");
  pruefe(m.minAbw < -50.0 && m.maxAbw < 5.0, "AK-6: Beugung gibt negative Werte, keine Überstreckung beim normalen Beugen");

  // Zweites Einmessen bei -20 Grad: neue Referenz = Mittelwert der Ruhephase (-20 Grad).
  // Zeitplan: 0-7,5 s erstes Einmessen, 7,5-9,5 s Arm weich auf -20 Grad, 9,5-11,5 s halten,
  // 11,5-19 s zweites Einmessen (Arm bei -20 Grad), 19-21 s halten, ab 21 s weiter auf -50 Grad.
  Bewegung bew2 = [](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t < 9.5) { p.flex = -20.0 * weich((t - 7.5) / 2.0); return p; }
    p.flex = -20.0;
    if (t < 11.5) return p;
    if (einmessenPose(p, t, 11.5, 60.0, 2, OBERARM_KREISE, 20.0)) return p;   // Oberarm leicht mitbewegen (AK-6)
    if (t < 21.0) return p;
    p.flex = -20.0 - 30.0 * weich((t - 21.0) / 1.0);
    return p;
  };
  EllbogenZustand z2;
  ellbogenZuruecksetzen(z2);
  ArmSim sim2(bew2, einbauStandard(), fehlerKeine(), 62);
  bool ok1 = einmessenLaufen(sim2, z2, 30.0f);      // t = 0 bis 7,5
  laufe(sim2, z2, 4.0, 0.0);                        // t = 7,5 bis 11,5
  bool ok2 = einmessenLaufen(sim2, z2, 30.0f);      // t = 11,5 bis 19, Arm bei -20 Grad
  laufe(sim2, z2, 2.0, 0.0);                        // t = 19 bis 21
  Messreihe still = laufe(sim2, z2, 1.0, 0.0, -20.0);   // t = 21 bis 22, Bewegung auf -50 beginnt
  laufe(sim2, z2, 1.5, 0.0, -20.0);                 // einschwingen
  Messreihe neu = laufe(sim2, z2, 1.5, 0.0, -20.0); // bei -50 Grad echt = -30 Grad zur neuen Referenz
  pruefe(ok1 && ok2, "AK-6: zwei Einmessungen hintereinander klappen");
  pruefeNahe(neu.mittel, -30.0, 2.5, "AK-6: Zweites Einmessen setzt die Referenz neu (bei -20 Grad eingemessen, -50 Grad echt zeigt -30)");
  pruefe(still.unbekannt == 0 && neu.unbekannt == 0, "AK-6: nach dem zweiten Einmessen ist der Winkel bekannt");
}

// ======================================================================
// AK-7: Abbruch
// ======================================================================

// Zeitplan: 0-7,5 s gültiges Einmessen bei 0 Grad; 7,5-9,5 s Arm weich auf -20 Grad; 9,5-11,5 s halten;
// 11,5-13,5 s Phase 1 des (fehlerhaften) Einmessens, mit Störung; 13,5-17,5 s Phase 2 mit 'amplitude';
// 17,5-19 s Phase 3 (Hand drehen); ab 19 s bleibt der Arm bei -20 Grad, ab 29 s bewegt er sich zwischen -20 und -90 Grad.
Bewegung fehlerZeitplan(double amplitude, double pronRate, double yawRate) {
  return [=](double t) -> Pose {
    Pose p = poseHaengend();
    if (einmessenPose(p, t, 0.0, 70.0, 2, OBERARM_KREISE, 20.0)) return p;
    if (t < 9.5) { p.flex = -20.0 * weich((t - 7.5) / 2.0); return p; }
    p.flex = -20.0;
    if (t < 11.5) return p;
    double s = t - 11.5;
    if (s < 2.0) {
      p.pron = pronRate * s;
      p.yaw = yawRate * s;
      return p;
    }
    p.pron = pronRate * 2.0;
    p.yaw = yawRate * 2.0;
    if (s < 6.0) {
      p.flex = -20.0 + beugeKurve(s - 2.0, amplitude, 2);
      oberarmBewegen(p, s - 2.0, OBERARM_KREISE, 20.0);   // Oberarm bewegt sich mit: der Abbruch hat also (a) oder (b) als Grund, nie (d)
      return p;
    }
    if (s < 7.5) {
      phase3Bewegen(p, s - 6.0);                            // Hand dreht normal: (e) trifft nicht zu
      return p;
    }
    if (t >= 29.0) p.flex = -20.0 - 35.0 * (1.0 - std::cos(2.0 * PI_ZAHL * (t - 29.0) / 4.0));
    return p;
  };
}

// Gültige Referenz vorhanden, dann scheitert ein Einmessen: Referenz (0 Grad) und Achse müssen bleiben.
// durchAbbrechen = true: die Logik wird mitten in Phase 2 mit einmessenAbbrechen beendet (AK-7 c).
void abbruchMitAlterReferenz(const char *kopf, double amplitude, double pronRate, double yawRate, bool durchAbbrechen) {
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(fehlerZeitplan(amplitude, pronRate, yawRate), einbauStandard(), fehlerKeine(), 71);
  bool ok1 = einmessenLaufen(sim, z, 30.0f);   // t = 0 bis 7,5
  laufe(sim, z, 4.0, 0.0);                     // t = 7,5 bis 11,5
  bool ok2;
  if (durchAbbrechen) {
    einmessenStart(z);
    zyklenOhne(sim, z, 100);
    einmessenBewegungStart(z);
    zyklenOhne(sim, z, 100);
    einmessenAbbrechen(z);
    ok2 = einmessenAbschliessen(z, 30.0f);     // nach dem Abbrechen darf nichts mehr abgeschlossen werden
    zyklenOhne(sim, z, 175);                   // bis t = 19
  } else {
    ok2 = einmessenLaufen(sim, z, 30.0f);      // t = 11,5 bis 19
  }
  bool nochmal = einmessenAbschliessen(z, 30.0f);    // nach einem Abbruch nie true
  Messreihe beruhigung = laufe(sim, z, 6.0, 0.0);    // t = 19 bis 25
  Messreihe halten = laufe(sim, z, 4.0, 0.0);        // t = 25 bis 29
  Messreihe bewegt = laufe(sim, z, 8.0, 0.0);        // t = 29 bis 37
  char text[220];
  pruefe(ok1, "Vorbereitung: erstes Einmessen klappt");
  std::snprintf(text, sizeof(text), "%s: das Einmessen liefert false (Abbruch)", kopf);
  pruefe(!ok2, text);
  std::snprintf(text, sizeof(text), "%s: ein erneutes einmessenAbschliessen nach dem Abbruch liefert nie true", kopf);
  pruefe(!nochmal, text);
  std::snprintf(text, sizeof(text), "%s: die Lage bleibt nach dem Abbruch bekannt und richtig auf 2,5 Grad (alte Referenz, ohne Unterbruch)", kopf);
  zeige("direkt nach Abbruch", beruhigung);
  pruefe(beruhigung.unbekannt == 0 && beruhigung.maxFehler <= 2.5, text);
  std::snprintf(text, sizeof(text), "%s: alte Referenz bleibt: Arm bei -20 Grad zeigt weiter -20 (nicht 0), auf 2,5 Grad", kopf);
  zeige("halten", halten);
  pruefe(halten.unbekannt == 0 && halten.maxFehler <= 2.5, text);
  std::snprintf(text, sizeof(text), "%s: alte Achse bleibt: Beugen bis -90 Grad bleibt auf 2,5 Grad genau", kopf);
  zeige("bewegt", bewegt);
  pruefe(bewegt.unbekannt == 0 && bewegt.maxFehler <= 2.5, text);
}

// Es gibt noch keine Referenz, das Einmessen scheitert: es darf danach keine Abweichung geben.
void abbruchOhneReferenz(const char *kopf, double amplitude, double pronRate, double yawRate) {
  Bewegung bew = mitEinmessen(poseHaengend(), amplitude, 2, pronRate, yawRate, danachHalten);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(bew, einbauStandard(), fehlerKeine(), 72);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  Messreihe m = laufe(sim, z, 6.0, 0.0);
  char text[220];
  std::snprintf(text, sizeof(text), "%s: das Einmessen liefert false (Abbruch)", kopf);
  pruefe(!ok, text);
  std::snprintf(text, sizeof(text), "%s: ohne frühere Referenz bleibt es bei 'keine Abweichung' (kein Alarm erlaubt)", kopf);
  pruefe(m.zyklen > 0 && m.unbekannt == m.zyklen, text);
}

// Gegenprobe: normale Sensorfehler und leichte Bewegung dürfen das Einmessen NICHT abbrechen.
void einmessenGelingt(const char *kopf, const SensorFehler &fe, double pronRate, double yawRate) {
  Bewegung bew = mitEinmessen(poseHaengend(), 70.0, 2, pronRate, yawRate, danachHalten);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(bew, einbauStandard(), fe, 73);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  pruefe(ok, kopf);
}

void testAK7() {
  std::printf("\n-- AK-7: Abbruch des Einmessens\n");
  // a) Gesamt-Beugung in Phase 2 zu klein (nur 10 Grad hin und zurück, klar unter 30 Grad).
  abbruchOhneReferenz("AK-7a: Beugung nur 10 Grad, ohne alte Referenz", 10.0, 0.0, 0.0);
  abbruchMitAlterReferenz("AK-7a: Beugung nur 10 Grad, mit alter Referenz", 10.0, 0.0, 0.0, false);
  // Spannweite zählt (grösster minus kleinster Winkel): 20 Grad hin und zurück = Spannweite 20 (nicht 40) -> Abbruch.
  abbruchOhneReferenz("AK-7a: Spannweite 20 Grad (hin und zurück, Weg 40), ohne alte Referenz", 20.0, 0.0, 0.0);
  abbruchMitAlterReferenz("AK-7a: Spannweite 20 Grad (hin und zurück, Weg 40), mit alter Referenz", 20.0, 0.0, 0.0, false);
  {
    // 4 Mal 20 Grad hin und zurück in 4 s: Weg 160 Grad, Spannweite aber nur 20 Grad -> Abbruch.
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(mitEinmessen(poseHaengend(), 20.0, 4, 0.0, 0.0, danachHalten), einbauStandard(), fehlerKeine(), 76);
    bool ok = einmessenLaufen(sim, z, 30.0f);
    pruefe(!ok, "AK-7a: 4 Mal 20 Grad hin und zurück (Weg 160, Spannweite 20): Abbruch, der Weg zählt nicht");
  }
  {
    // Spannweite 40 Grad (über 30), 2 Mal beugen: Einmessen klappt.
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(mitEinmessen(poseHaengend(), 40.0, 2, 0.0, 0.0, danachHalten), einbauStandard(), fehlerKeine(), 77);
    pruefe(einmessenLaufen(sim, z, 30.0f), "AK-7a: Spannweite 40 Grad (über 30): Einmessen klappt");
  }
  // b) Arm in Phase 1 nicht ruhig (Drehgeschwindigkeit weit über 10 Grad/s).
  abbruchOhneReferenz("AK-7b: nur der Unterarm dreht sich in Phase 1 (40 Grad/s), ohne alte Referenz", 70.0, 40.0, 0.0);
  abbruchMitAlterReferenz("AK-7b: nur der Unterarm dreht sich in Phase 1 (40 Grad/s), mit alter Referenz", 70.0, 40.0, 0.0, false);
  abbruchOhneReferenz("AK-7b: ganzer Arm schwenkt in Phase 1 (30 Grad/s), ohne alte Referenz", 70.0, 0.0, 30.0);
  abbruchMitAlterReferenz("AK-7b: ganzer Arm schwenkt in Phase 1 (30 Grad/s), mit alter Referenz", 70.0, 0.0, 30.0, false);
  // c) Abbruch von aussen (IMU-Ausfall): einmessenAbbrechen
  abbruchMitAlterReferenz("AK-7c: einmessenAbbrechen mitten in Phase 2", 70.0, 0.0, 0.0, true);
  {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 74);
    einmessenStart(z);
    zyklenOhne(sim, z, 100);
    einmessenBewegungStart(z);
    zyklenOhne(sim, z, 100);
    einmessenAbbrechen(z);
    Messreihe m = laufe(sim, z, 5.0, 0.0);
    pruefe(m.unbekannt == m.zyklen, "AK-7c: einmessenAbbrechen ohne frühere Referenz: es bleibt bei 'keine Abweichung'");
  }
  // Gegenproben: das darf nicht abbrechen
  einmessenGelingt("AK-7 Gegenprobe: Gyro-Offset +-2 Grad/s und Rauschen: Einmessen klappt (Muster 0)", fehlerMitOffset(0), 0.0, 0.0);
  einmessenGelingt("AK-7 Gegenprobe: Gyro-Offset +-2 Grad/s und Rauschen: Einmessen klappt (Muster 1)", fehlerMitOffset(1), 0.0, 0.0);
  einmessenGelingt("AK-7 Gegenprobe: leichte Bewegung 3 Grad/s plus Offset (unter 10 Grad/s): Einmessen klappt",
                   fehlerMitOffset(0), 3.0, 3.0);

  // Parameter minBeugungGrad: gleiche Bewegung (2 Mal 50 Grad hin und zurück), einmal 30 (reicht), einmal 120 (reicht nicht).
  {
    EllbogenZustand z1;
    ellbogenZuruecksetzen(z1);
    ArmSim sim1(mitEinmessen(poseHaengend(), 50.0, 2, 0.0, 0.0, danachHalten), einbauStandard(), fehlerKeine(), 75);
    bool okKlein = einmessenLaufen(sim1, z1, 30.0f);
    EllbogenZustand z2;
    ellbogenZuruecksetzen(z2);
    ArmSim sim2(mitEinmessen(poseHaengend(), 50.0, 2, 0.0, 0.0, danachHalten), einbauStandard(), fehlerKeine(), 75);
    bool okGross = einmessenLaufen(sim2, z2, 120.0f);
    pruefe(okKlein, "AK-7: Beugung 50 Grad bei minBeugungGrad = 30: Einmessen klappt");
    pruefe(!okGross, "AK-7: Beugung 50 Grad bei minBeugungGrad = 120: Einmessen wird abgebrochen (Parameter wirkt)");
  }
}

// ======================================================================
// AK-8: Wiederholgenauigkeit
// ======================================================================

void testAK8() {
  std::printf("\n-- AK-8: Wiederholgenauigkeit (10 Einmessungen hintereinander)\n");
  Bewegung bew = [](double t) -> Pose {
    double u = std::fmod(t, 9.5);   // 7,5 s Einmessen + 2 s Messen
    Pose p = poseHaengend();
    einmessenPose(p, u, 0.0, 70.0, 2, OBERARM_KREISE, 20.0);   // AK-6: Oberarm leicht mitbewegen, Hand drehen in Phase 3
    return p;
  };
  ArmSim sim(bew, einbauStandard(), fehlerMitOffset(0), 81);
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  double werte[10];
  int geklappt = 0;
  int unbek = 0;
  for (int k = 0; k < 10; k++) {
    if (einmessenLaufen(sim, z, 30.0f)) geklappt++;
    Messreihe m = laufe(sim, z, 2.0, 0.0);
    werte[k] = m.mittel;
    unbek += m.unbekannt;
  }
  double kleinster = werte[0];
  double groesster = werte[0];
  double schlimmster = 0.0;
  for (int k = 0; k < 10; k++) {
    if (werte[k] < kleinster) kleinster = werte[k];
    if (werte[k] > groesster) groesster = werte[k];
    if (std::fabs(werte[k]) > schlimmster) schlimmster = std::fabs(werte[k]);
  }
  std::printf("          Streuung der 10 Referenzen = %.3f Grad, grösster Betrag = %.3f Grad\n",
              groesster - kleinster, schlimmster);
  pruefe(geklappt == 10, "AK-8: alle 10 Einmessungen klappen");
  pruefe(unbek == 0, "AK-8: nach jedem Einmessen ist der Winkel bekannt");
  pruefe(groesster - kleinster <= 3.0, "AK-8: die 10 Referenzen liegen höchstens 3 Grad auseinander (+-1,5)");
  pruefe(schlimmster <= 2.5, "AK-8: jede Referenz stimmt auf 2,5 Grad (gestreckter Arm zeigt 0)");
}

// ======================================================================
// AK-9: Einschalten ohne automatisches Einmessen
// ======================================================================

void testAK9() {
  std::printf("\n-- AK-9: Einschalten, keine automatische Einmessung\n");
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  float a = 123.5f;
  pruefe(!ellbogenAbweichung(z, a), "AK-9: nach dem Zurücksetzen gibt es keine Abweichung");
  pruefeNahe(a, 123.5, 0.0, "AK-9: abweichungGrad bleibt unverändert, wenn es keine Abweichung gibt");

  ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 91);
  int bekannt = 0;
  for (int i = 0; i < 1500; i++) {   // 30 s ohne Einmessen
    if (zyklusUndLesen(sim, z, a)) bekannt++;
  }
  pruefe(bekannt == 0, "AK-9: 30 s Messzyklen ohne Taste: nie eine Abweichung (kein automatisches Einmessen)");

  Bewegung ueberstreckt = [](double t) -> Pose {
    Pose p = poseHaengend();
    p.flex = 20.0 * weich(t / 2.0);
    return p;
  };
  EllbogenZustand z2;
  ellbogenZuruecksetzen(z2);
  ArmSim sim2(ueberstreckt, einbauStandard(), fehlerKeine(), 92);
  int bekannt2 = 0;
  for (int i = 0; i < 500; i++) {    // 10 s mit überstrecktem Arm
    if (zyklusUndLesen(sim2, z2, a)) bekannt2++;
  }
  pruefe(bekannt2 == 0, "AK-9: überstreckter Arm ohne Einmessen: keine Abweichung, also keine Warnung");

  // Nach dem Einmessen gibt es sie, nach dem Zurücksetzen wieder nicht.
  EllbogenZustand z3;
  ellbogenZuruecksetzen(z3);
  ArmSim sim3(standardBewegung(), einbauStandard(), fehlerKeine(), 93);
  bool ok = einmessenLaufen(sim3, z3, 30.0f);
  pruefe(ok && ellbogenAbweichung(z3, a), "AK-9: nach erfolgreichem Einmessen gibt es eine Abweichung");
  ellbogenZuruecksetzen(z3);
  pruefe(!ellbogenAbweichung(z3, a), "AK-9: ellbogenZuruecksetzen löscht Referenz, Achse und Lage (wie nach dem Einschalten)");
}

// ======================================================================
// AK-13: nach einem Ausfall
// ======================================================================

// 0-7,5 s Einmessen, danach 6 s normale Bewegung (ohne Unterarm-Drehung), dann 3 s Ausfall (Arm bewegt sich weiter,
// geht zur Ruhe bei -45 Grad Ellbogen und Neigung 40), danach halten oder weiter bewegen.
// Die Unterarm-Drehung bleibt vor und nach dem Ausfall gleich (0), siehe Meldung an den Engineer.
Bewegung ausfallZeitplan(bool weiterBewegen) {
  Danach danach = [=](double s, const Pose &ende) -> Pose {
    if (s < 6.0) return alltagPose(ende, s, 0.0);
    Pose von = alltagPose(ende, 6.0, 0.0);
    double r = weich((s - 6.0) / 3.0);
    Pose p = von;
    p.flex = von.flex + (-45.0 - von.flex) * r;
    p.pitch = von.pitch + (40.0 - von.pitch) * r;
    p.yaw = von.yaw + (0.0 - von.yaw) * r;
    p.twist = von.twist + (0.0 - von.twist) * r;
    if (weiterBewegen && s >= 9.0) {
      double d = s - 9.0;
      p.flex = -45.0 + 45.0 * std::sin(PI_ZAHL * d);
      p.pitch = 40.0 + 20.0 * std::sin(2.0 * PI_ZAHL * d / 5.0);
    }
    return p;
  };
  return mitEinmessen(poseHaengend(), 70.0, 2, 0.0, 0.0, danach);
}

void ausfallPruefen(bool weiterBewegen, const char *kopf) {
  EllbogenZustand z;
  ellbogenZuruecksetzen(z);
  ArmSim sim(ausfallZeitplan(weiterBewegen), einbauStandard(), fehlerNurRauschen(), 131);
  bool ok = einmessenLaufen(sim, z, 30.0f);
  Messreihe vorher = laufe(sim, z, 6.0, 0.0);     // t = 6 bis 12
  ellbogenLageVerwerfen(z);                       // Ausfall endet (REQ-001)
  float a = 55.0f;
  bool sofortBekannt = ellbogenAbweichung(z, a);
  // Während des Ausfalls kommen keine Messungen bei der Logik an (der Simulator läuft weiter).
  for (int i = 0; i < 150; i++) {
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
  }
  int ersteBekannt = -1;
  int falscheWerte = 0;
  int wiederUnbekannt = 0;
  for (int i = 0; i < 250; i++) {                 // 5 s nach der Wiederkehr
    float x = 0.0f;
    bool bek = zyklusUndLesen(sim, z, x);
    if (bek) {
      if (ersteBekannt < 0) ersteBekannt = i;
      if (std::fabs(x - sim.pose.flex) > 2.5) falscheWerte++;
    } else if (ersteBekannt >= 0) {
      wiederUnbekannt++;
    }
  }
  Messreihe danach = laufe(sim, z, 10.0, 0.0);

  char text[220];
  pruefe(ok && vorher.unbekannt == 0 && vorher.maxFehler <= 2.5, "Vorbereitung: vor dem Ausfall stimmt die Messung auf 2,5 Grad");
  std::snprintf(text, sizeof(text), "AK-13 (%s): direkt nach ellbogenLageVerwerfen gibt es keine Abweichung (keine Warnung)", kopf);
  pruefe(!sofortBekannt, text);
  std::snprintf(text, sizeof(text), "AK-13 (%s): die Lage ist spätestens 5 s nach der Wiederkehr wieder bekannt", kopf);
  std::printf("          Lage bekannt nach %d Zyklen (%.2f s)\n", ersteBekannt, ersteBekannt * 0.02);
  pruefe(ersteBekannt >= 0, text);
  std::snprintf(text, sizeof(text), "AK-13 (%s): in der Zeit bis dahin kein falscher Wert (Fehler <= 2,5 Grad, keine Fehlalarme)", kopf);
  pruefe(falscheWerte == 0, text);
  std::snprintf(text, sizeof(text), "AK-13 (%s): einmal bekannt, bleibt die Lage bekannt", kopf);
  pruefe(wiederUnbekannt == 0, text);
  std::snprintf(text, sizeof(text), "AK-13 (%s): danach stimmt der Winkel zur alten Referenz auf 2,5 Grad", kopf);
  zeige("danach", danach);
  pruefe(danach.unbekannt == 0 && danach.maxFehler <= 2.5, text);
}

void testAK13() {
  std::printf("\n-- AK-13: nach einem Ausfall (REQ-001)\n");
  ausfallPruefen(false, "Arm ruht nach dem Ausfall");
  ausfallPruefen(true, "Arm bewegt sich nach dem Ausfall");
}

// ======================================================================
// AK-14: ungültige Werte
// ======================================================================

void setzeFeld(ImuMessung &m, int feld, float wert) {
  switch (feld) {
    case 0: m.ax = wert; break;
    case 1: m.ay = wert; break;
    case 2: m.az = wert; break;
    case 3: m.gx = wert; break;
    case 4: m.gy = wert; break;
    default: m.gz = wert; break;
  }
}

// Gültig nach AK-14: endlich, Beschleunigung je Achse höchstens 16 g, Drehgeschwindigkeit je Achse höchstens 2000 Grad/s.
bool messungGueltig(const ImuMessung &m) {
  return std::isfinite(m.ax) && std::isfinite(m.ay) && std::isfinite(m.az) &&
         std::isfinite(m.gx) && std::isfinite(m.gy) && std::isfinite(m.gz) &&
         std::fabs(m.ax) <= 16.0f && std::fabs(m.ay) <= 16.0f && std::fabs(m.az) <= 16.0f &&
         std::fabs(m.gx) <= 2000.0f && std::fabs(m.gy) <= 2000.0f && std::fabs(m.gz) <= 2000.0f;
}

void testAK14() {
  std::printf("\n-- AK-14: ungültige Werte\n");
  const float schlecht[3] = {NAN, INFINITY, -INFINITY};
  const float dtSchlecht[9] = {0.0f, -0.02f, 0.5001f, 0.51f, 1.0f, 1e30f, NAN, INFINITY, -INFINITY};

  // Zustand in drei Lagen: ohne Referenz, mitten im Einmessen, eingemessen.
  for (int art = 0; art < 3; art++) {
    ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 141);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    if (art == 1) {
      einmessenStart(z);
      zyklenOhne(sim, z, 100);
      einmessenBewegungStart(z);
      zyklenOhne(sim, z, 50);
    }
    if (art == 2) {
      einmessenLaufen(sim, z, 30.0f);
      laufe(sim, z, 2.0, 0.0);
    }
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
    int versuche = 0;
    int angenommen = 0;
    int veraendert = 0;
    for (int f = 0; f < 12; f++) {
      for (int b = 0; b < 3; b++) {
        ImuMessung o2 = o;
        ImuMessung u2 = u;
        if (f < 6) setzeFeld(o2, f, schlecht[b]); else setzeFeld(u2, f - 6, schlecht[b]);
        unsigned char vorher[sizeof(EllbogenZustand)];
        std::memcpy(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand));
        bool ok = ellbogenAktualisieren(z, o2, u2, 0.02f);
        versuche++;
        if (ok) angenommen++;
        if (std::memcmp(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand)) != 0) veraendert++;
      }
    }
    // (c) physikalisch unmögliche Werte: Beschleunigung > 16 g, Drehgeschwindigkeit > 2000 Grad/s (auch 9999)
    const float zuGrossAcc[5] = {16.5f, -16.5f, 100.0f, 9999.0f, -1e30f};
    const float zuGrossGyro[5] = {2000.5f, -2000.5f, 5000.0f, 9999.0f, 1e30f};
    for (int f = 0; f < 12; f++) {
      for (int b = 0; b < 5; b++) {
        ImuMessung o2 = o;
        ImuMessung u2 = u;
        int feld = f % 6;
        float wert = (feld < 3) ? zuGrossAcc[b] : zuGrossGyro[b];
        if (f < 6) setzeFeld(o2, feld, wert); else setzeFeld(u2, feld, wert);
        unsigned char vorher[sizeof(EllbogenZustand)];
        std::memcpy(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand));
        bool ok = ellbogenAktualisieren(z, o2, u2, 0.02f);
        versuche++;
        if (ok) angenommen++;
        if (std::memcmp(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand)) != 0) veraendert++;
      }
    }
    for (int d = 0; d < 9; d++) {
      unsigned char vorher[sizeof(EllbogenZustand)];
      std::memcpy(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand));
      bool ok = ellbogenAktualisieren(z, o, u, dtSchlecht[d]);
      versuche++;
      if (ok) angenommen++;
      if (std::memcmp(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand)) != 0) veraendert++;
    }
    const char *namen[3] = {"ohne Referenz", "mitten im Einmessen", "eingemessen"};
    char text[200];
    std::snprintf(text, sizeof(text), "AK-14 (%s): NaN, unendlich, über 16 g, über 2000 Grad/s (z. B. 9999) in jedem der 12 Messwerte und ungültiges dt: Rückgabe false", namen[art]);
    std::printf("          %d Versuche, davon angenommen: %d\n", versuche, angenommen);
    pruefe(angenommen == 0, text);
    std::snprintf(text, sizeof(text), "AK-14 (%s): ein abgelehnter Zyklus lässt den Zustand unverändert", namen[art]);
    pruefe(veraendert == 0, text);
  }

  // Gültige Grenzfälle für dt (0,5 s ist noch gültig).
  {
    ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 142);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
    pruefe(ellbogenAktualisieren(z, o, u, 0.5f), "AK-14: dt = 0,5 s ist gültig (true)");
    pruefe(ellbogenAktualisieren(z, o, u, 0.001f), "AK-14: dt = 0,001 s ist gültig (true)");
    // Werte knapp unter den Grenzen (15,9 g, 1999 Grad/s) sind noch gültig.
    ImuMessung o3 = o;
    o3.ax = 15.9f;
    o3.gy = 1999.0f;
    pruefe(ellbogenAktualisieren(z, o3, u, 0.02f), "AK-14: 15,9 g und 1999 Grad/s sind noch gültig (true)");
  }

  // Verhalten: zwei gleiche Zustände, einer bekommt dazwischen ungültige Zyklen. Danach müssen beide gleich sein.
  {
    ArmSim sim(standardBewegung(), einbauStandard(), fehlerNurRauschen(), 143);
    EllbogenZustand zA;
    EllbogenZustand zB;
    ellbogenZuruecksetzen(zA);
    ellbogenZuruecksetzen(zB);
    einmessenStart(zA);
    einmessenStart(zB);
    int unterschiede = 0;
    int ungueltigAngenommen = 0;
    bool okA = false;
    bool okB = false;
    for (int k = 0; k < 900; k++) {
      if (k == 100) {
        einmessenBewegungStart(zA);
        einmessenBewegungStart(zB);
      }
      if (k == 300) {
        einmessenDrehungStart(zA);
        einmessenDrehungStart(zB);
      }
      if (k == 375) {
        okA = einmessenAbschliessen(zA, 30.0f);
        okB = einmessenAbschliessen(zB, 30.0f);
      }
      ImuMessung o = ImuMessung();
      ImuMessung u = ImuMessung();
      sim.schritt(o, u);
      if (k % 7 == 3) {
        int v = (k / 7) % 29;
        ImuMessung o2 = o;
        ImuMessung u2 = u;
        float dt = 0.02f;
        if (v < 12) {
          if (v < 6) setzeFeld(o2, v, NAN); else setzeFeld(u2, v - 6, NAN);
        } else if (v < 24) {
          if (v < 18) setzeFeld(o2, v - 12, INFINITY); else setzeFeld(u2, v - 18, -INFINITY);
        } else {
          const float dts[5] = {0.0f, -0.02f, 0.51f, NAN, INFINITY};
          dt = dts[v - 24];
        }
        if (ellbogenAktualisieren(zA, o2, u2, dt)) ungueltigAngenommen++;
      }
      ellbogenAktualisieren(zA, o, u, 0.02f);
      ellbogenAktualisieren(zB, o, u, 0.02f);
      if (k > 300) {
        float a = 0.0f;
        float b = 0.0f;
        bool bekA = ellbogenAbweichung(zA, a);
        bool bekB = ellbogenAbweichung(zB, b);
        if (bekA != bekB) unterschiede++;
        else if (bekA && std::fabs(a - b) > 1e-6) unterschiede++;
      }
    }
    pruefe(okA && okB, "AK-14: Einmessen klappt trotz eingestreuter ungültiger Zyklen");
    pruefe(ungueltigAngenommen == 0, "AK-14: eingestreute ungültige Zyklen werden alle abgelehnt");
    pruefe(unterschiede == 0, "AK-14: ungültige Zyklen ändern nichts: beide Zustände zeigen danach die gleichen Werte");
  }
}

// ======================================================================
// Fuzz-Tests: zufällige und extreme Werte
// ======================================================================

float zufallWert(float bereich) {
  int r = std::rand() % 20;
  switch (r) {
    case 0: return NAN;
    case 1: return INFINITY;
    case 2: return -INFINITY;
    case 3: return 1e30f;
    case 4: return -1e30f;
    case 5: return 1e-30f;
    case 6: return 0.0f;
    case 7: return -0.0f;
    case 8: return FLT_MAX;
    case 9: return -FLT_MAX;
    default: {
      double u = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
      return static_cast<float>((2.0 * u - 1.0) * bereich);
    }
  }
}

ImuMessung zufallMessung() {
  ImuMessung m = ImuMessung();
  m.ax = zufallWert(1.5f);
  m.ay = zufallWert(1.5f);
  m.az = zufallWert(1.5f);
  m.gx = zufallWert(300.0f);
  m.gy = zufallWert(300.0f);
  m.gz = zufallWert(300.0f);
  return m;
}

float zufallDt() {
  int r = std::rand() % 12;
  switch (r) {
    case 0: return 0.0f;
    case 1: return -0.02f;
    case 2: return NAN;
    case 3: return INFINITY;
    case 4: return 1e30f;
    case 5: return 0.5f;
    case 6: return 0.5001f;
    case 7: return -INFINITY;
    case 8: {
      double u = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
      return static_cast<float>(u * 0.6);
    }
    default: return 0.02f;
  }
}

// Plausible Zufallswerte (keine Sonderwerte), für die Aufruf-Reihenfolge-Fuzz.
ImuMessung plausibleMessung() {
  ImuMessung m = ImuMessung();
  double u1 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  double u2 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  double u3 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  double u4 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  double u5 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  double u6 = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
  m.ax = static_cast<float>((2.0 * u1 - 1.0) * 1.2);
  m.ay = static_cast<float>((2.0 * u2 - 1.0) * 1.2);
  m.az = static_cast<float>((2.0 * u3 - 1.0) * 1.2);
  m.gx = static_cast<float>((2.0 * u4 - 1.0) * 150.0);
  m.gy = static_cast<float>((2.0 * u5 - 1.0) * 150.0);
  m.gz = static_cast<float>((2.0 * u6 - 1.0) * 150.0);
  return m;
}

void testFuzz() {
  std::printf("\n-- Fuzz: zufällige und extreme Werte (fester Startwert 42)\n");

  // Fuzz A: Messwerte mit NaN, unendlich, riesig, winzig, 0 und negativ. Drei Anfangszustände.
  std::srand(42);
  const char *namen[3] = {"ohne Referenz", "mitten im Einmessen", "eingemessen"};
  for (int art = 0; art < 3; art++) {
    ArmSim sim(standardBewegung(), einbauStandard(), fehlerKeine(), 151);
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    if (art == 1) {
      einmessenStart(z);
      zyklenOhne(sim, z, 100);
      einmessenBewegungStart(z);
    }
    if (art == 2) {
      einmessenLaufen(sim, z, 30.0f);
    }
    int boeseAngenommen = 0;
    int boeseVeraendert = 0;
    int nichtEndlich = 0;
    int boese = 0;
    for (int i = 0; i < 20000; i++) {
      ImuMessung o = zufallMessung();
      ImuMessung u = zufallMessung();
      float dt = zufallDt();
      bool ungueltig = !messungGueltig(o) || !messungGueltig(u) || !(dt > 0.0f && dt <= 0.5f);
      unsigned char vorher[sizeof(EllbogenZustand)];
      std::memcpy(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand));
      bool ok = ellbogenAktualisieren(z, o, u, dt);
      if (ungueltig) {
        boese++;
        if (ok) boeseAngenommen++;
        if (std::memcmp(vorher, static_cast<const void *>(&z), sizeof(EllbogenZustand)) != 0) boeseVeraendert++;
      }
      float a = 0.0f;
      if (ellbogenAbweichung(z, a) && !std::isfinite(a)) nichtEndlich++;
    }
    char text[200];
    std::snprintf(text, sizeof(text), "Fuzz A (%s): 20000 wilde Zyklen, kein Absturz (%d davon ungültig)", namen[art], boese);
    pruefe(true, text);
    std::snprintf(text, sizeof(text), "Fuzz A (%s): ungültige Zyklen (NaN, unendlich, über 16 g, über 2000 Grad/s, dt falsch) werden abgelehnt", namen[art]);
    pruefe(boeseAngenommen == 0, text);
    std::snprintf(text, sizeof(text), "Fuzz A (%s): abgelehnte Zyklen ändern den Zustand nicht", namen[art]);
    pruefe(boeseVeraendert == 0, text);
    std::snprintf(text, sizeof(text), "Fuzz A (%s): gemeldete Abweichung ist nie NaN oder unendlich (sonst bleibt es bei 'keine Abweichung')", namen[art]);
    pruefe(nichtEndlich == 0, text);
  }

  // Fuzz B: zufällige Reihenfolge aller Funktionen, auch ungewöhnliche Parameter für minBeugungGrad.
  std::srand(42);
  {
    EllbogenZustand z;
    ellbogenZuruecksetzen(z);
    const float minBeugung[8] = {30.0f, 0.0f, -5.0f, NAN, INFINITY, 1e30f, 360.0f, -INFINITY};
    int nichtEndlich = 0;
    int nachResetBekannt = 0;
    int nachVerwerfenBekannt = 0;
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
        einmessenAbschliessen(z, minBeugung[std::rand() % 8]);
      } else if (op == 6) {
        einmessenDrehungStart(z);
      } else if (op == 4) {
        einmessenAbbrechen(z);
      } else if (op == 5) {
        ellbogenLageVerwerfen(z);
        if (ellbogenAbweichung(z, a)) nachVerwerfenBekannt++;
      } else {
        ImuMessung o = plausibleMessung();
        ImuMessung u = plausibleMessung();
        ellbogenAktualisieren(z, o, u, 0.02f);
      }
      if (ellbogenAbweichung(z, a) && !std::isfinite(a)) nichtEndlich++;
    }
    pruefe(true, "Fuzz B: 30000 Aufrufe in zufälliger Reihenfolge, kein Absturz");
    pruefe(nachResetBekannt == 0, "Fuzz B: nach ellbogenZuruecksetzen gibt es nie eine Abweichung");
    pruefe(nachVerwerfenBekannt == 0, "Fuzz B: direkt nach ellbogenLageVerwerfen gibt es nie eine Abweichung");
    pruefe(nichtEndlich == 0, "Fuzz B: gemeldete Abweichung ist nie NaN oder unendlich");
  }
}

// ======================================================================

int main() {
  std::printf("REQ-002: Winkelmessung unabhängig von Drehung\n");
  testAK1bis3();

  std::printf("\n-- AK-4: Arm in jeder Lage (eingemessen im Haengen)\n");
  const double lagenFlex[3] = {0.0, -30.0, -90.0};
  for (int i = 0; i < 3; i++) lagenPruefen(lagenFlex[i]);
  std::printf("\n-- AK-4: IMUs beliebig um die Knochenachse gedreht (vor dem Einmessen)\n");
  for (int achse = 0; achse < 3; achse++) einbauPruefen(achse);
  beliebigerEinbau();

  testAK5();
  testAK6();
  testAK7();
  testAK8();
  testAK9();
  testAK13();
  testAK14();
  testFuzz();
  std::printf("\nRESERVE (Grenze 2 Grad): groesste Winkelaenderung AK-1 bis AK-3 = %.3f Grad, groesster Fehler AK-4 (Lagen) = %.3f Grad\n",
              g_groessteAenderungAK1bis3, g_groessterFehlerAK4Lagen);
  std::printf("RESERVE (Grenze 2,5 Grad): groesster Fehler AK-4 (Einbaudrehung) = %.3f Grad\n", g_groessterFehlerAK4Einbau);
  return testErgebnis();
}
