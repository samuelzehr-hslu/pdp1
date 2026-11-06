// sim_ellbogen.h
// Hilfsmittel für die Tests zu REQ-002 (kein Produkt-Code, nur für die Tests).
//
// Der Simulator baut einen Arm mit zwei IMUs (Oberarm, Unterarm) und erzeugt daraus die Messwerte,
// so wie der MPU6050 sie liefern würde. Die Winkel-LOGIK wird hier NICHT nachgebaut:
// Der Simulator kennt nur die Geometrie (die "wahre" Lage des Arms) und rechnet Sensorwerte daraus aus.
// Die Logik aus jointsense/logik.cpp muss daraus selbst den Ellbogenwinkel finden.
//
// Annahmen des Modells:
//  - Beschleunigung in g: ein ruhender Sensor misst +1 g in Richtung "oben" (wie der MPU6050).
//  - Gyro in Grad pro Sekunde, Rechte-Hand-Regel, in den Achsen des Sensors.
//    Der Wert ist der MITTELWERT über den Zyklus (dt). So passt "Winkel += Gyro * dt" genau.
//  - Es gibt nur die Erdbeschleunigung, keine Beschleunigung durch die Armbewegung selbst.
//  - Ellbogen = Scharnier (Beugen/Strecken) plus freie Unterarm-Drehung um die Unterarm-Achse.
//  - "Wahrer Winkel" = Pose.flex: 0 = gestreckt (so wird eingemessen), negativ = Beugung, positiv = Überstreckung.

#ifndef SIM_ELLBOGEN_H
#define SIM_ELLBOGEN_H

#include <cmath>
#include <cstdio>
#include <functional>
#include "logik.h"

const double PI_ZAHL = 3.14159265358979323846;
const double DT_ZYKLUS = 0.02;   // 50 Hz

inline double gradZuRad(double grad) { return grad * PI_ZAHL / 180.0; }
inline double radZuGrad(double rad) { return rad * 180.0 / PI_ZAHL; }

// ---------- kleine Mathe-Helfer (3er-Vektoren und 3x3-Drehmatrizen) ----------

struct Vec3 { double x; double y; double z; };
struct Mat3 { double m[3][3]; };

inline Vec3 vec3(double x, double y, double z) {
  Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

inline Mat3 matEinheit() {
  Mat3 r;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r.m[i][j] = (i == j) ? 1.0 : 0.0;
    }
  }
  return r;
}

inline Mat3 matMal(const Mat3 &a, const Mat3 &b) {
  Mat3 r;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      double summe = 0.0;
      for (int k = 0; k < 3; k++) {
        summe += a.m[i][k] * b.m[k][j];
      }
      r.m[i][j] = summe;
    }
  }
  return r;
}

inline Mat3 matTransp(const Mat3 &a) {
  Mat3 r;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r.m[i][j] = a.m[j][i];
    }
  }
  return r;
}

inline Vec3 matMalVec(const Mat3 &a, const Vec3 &v) {
  return vec3(a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
              a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
              a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z);
}

inline Mat3 drehX(double grad) {
  double c = std::cos(gradZuRad(grad));
  double s = std::sin(gradZuRad(grad));
  Mat3 r = matEinheit();
  r.m[1][1] = c;
  r.m[1][2] = -s;
  r.m[2][1] = s;
  r.m[2][2] = c;
  return r;
}

inline Mat3 drehY(double grad) {
  double c = std::cos(gradZuRad(grad));
  double s = std::sin(gradZuRad(grad));
  Mat3 r = matEinheit();
  r.m[0][0] = c;
  r.m[0][2] = s;
  r.m[2][0] = -s;
  r.m[2][2] = c;
  return r;
}

inline Mat3 drehZ(double grad) {
  double c = std::cos(gradZuRad(grad));
  double s = std::sin(gradZuRad(grad));
  Mat3 r = matEinheit();
  r.m[0][0] = c;
  r.m[0][1] = -s;
  r.m[1][0] = s;
  r.m[1][1] = c;
  return r;
}

// Drehvektor (in Grad) einer Drehmatrix. Für die kleine Drehung zwischen zwei Zyklen.
inline Vec3 drehvektorGrad(const Mat3 &d) {
  double vx = (d.m[2][1] - d.m[1][2]) / 2.0;
  double vy = (d.m[0][2] - d.m[2][0]) / 2.0;
  double vz = (d.m[1][0] - d.m[0][1]) / 2.0;
  double s = std::sqrt(vx * vx + vy * vy + vz * vz);                   // = sin(Winkel)
  double c = (d.m[0][0] + d.m[1][1] + d.m[2][2] - 1.0) / 2.0;          // = cos(Winkel)
  double winkel = std::atan2(s, c);
  double faktor = (s > 1e-12) ? winkel / s : 1.0;
  return vec3(radZuGrad(vx * faktor), radZuGrad(vy * faktor), radZuGrad(vz * faktor));
}

inline double weich(double x) {   // sanfter Übergang 0 -> 1
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * (3.0 - 2.0 * x);
}

// ---------- Arm-Lage ----------

// Alle Winkel in Grad.
//  yaw   : ganzer Arm dreht sich um die Senkrechte (waagrecht schwenken)
//  pitch : Neigung der Knochenachse des Oberarms. 90 = hängend, 0 = nach vorn, -90 = nach oben
//  seit  : Oberarm pendelt zur Seite (Drehung um die waagrechte Vorwärts-Achse der Welt). Normal 0.
//          Zusammen mit pitch ergibt das "kleine Kreise" des Oberarms.
//  twist : Oberarm dreht sich in der Schulter (um die eigene Achse)
//  flex  : echter Ellbogenwinkel. 0 = gestreckt, negativ = Beugung, positiv = Überstreckung
//  pron  : Unterarm dreht sich um die eigene Achse (Schloss aufdrehen)
// Hinweis: Beim hängenden Arm (pitch = 90) sind yaw UND twist beides Drehungen um die Senkrechte.
//          Nur pitch und seit sind dann Drehungen um eine waagrechte (nicht senkrechte) Achse.
struct Pose {
  double yaw;
  double pitch;
  double seit;
  double twist;
  double flex;
  double pron;
};

// Art der Oberarm-Bewegung in Phase 2 des Einmessens (siehe oberarmBewegen weiter unten).
// OBERARM_KREISE = ein kleiner Kreis (ein Umlauf in 4 s), so verlangt es AK-6 (Stand: 1 Umlauf).
enum { OBERARM_NICHTS = 0, OBERARM_PENDELN = 1, OBERARM_KREISE = 2, OBERARM_SENKRECHT = 3 };

typedef std::function<Pose(double)> Bewegung;                         // Zeit in s -> Pose
typedef std::function<Pose(double, const Pose &)> Danach;              // Zeit seit Ende Einmessen, Pose am Ende des Einmessens

inline Pose poseHaengend() {
  Pose p;
  p.yaw = 0.0;
  p.pitch = 90.0;
  p.seit = 0.0;
  p.twist = 0.0;
  p.flex = 0.0;
  p.pron = 0.0;
  return p;
}

inline Pose danachHalten(double, const Pose &ende) {
  return ende;
}

// Einbau der IMUs im Sleeve: Drehung des Sensors gegenüber dem Knochen.
struct Einbau {
  Mat3 oben;
  Mat3 unten;
};

// knochenAchse: welche Sensor-Achse zeigt entlang des Knochens (0 = X, 1 = Y, 2 = Z).
// drehGrad: Drehung des Sensors um die Knochenachse (0 bis 360).
inline Mat3 einbauMatrix(int knochenAchse, double drehGrad) {
  Mat3 basis = matEinheit();
  if (knochenAchse == 1) basis = drehZ(-90.0);   // Sensor-Y zeigt entlang des Knochens
  if (knochenAchse == 2) basis = drehY(90.0);    // Sensor-Z zeigt entlang des Knochens
  return matMal(drehX(drehGrad), basis);
}

inline Einbau einbauMit(int knochenAchse, double drehOben, double drehUnten) {
  Einbau e;
  e.oben = einbauMatrix(knochenAchse, drehOben);
  e.unten = einbauMatrix(knochenAchse, drehUnten);
  return e;
}

inline Einbau einbauStandard() {
  return einbauMit(0, 0.0, 0.0);
}

// Sensorlage (Sensor-Achsen in der Welt) beider IMUs für eine Pose.
inline void lageMatrizen(const Pose &p, const Einbau &e, Mat3 &oben, Mat3 &unten) {
  Mat3 knochenOben = matMal(matMal(matMal(drehZ(p.yaw), drehX(p.seit)), drehY(p.pitch)), drehX(p.twist));
  Mat3 knochenUnten = matMal(matMal(knochenOben, drehY(-p.flex)), drehX(p.pron));
  oben = matMal(knochenOben, e.oben);
  unten = matMal(knochenUnten, e.unten);
}

// ---------- Sensorfehler ----------

struct SensorFehler {
  Vec3 gyroOffsetOben;       // Grad pro Sekunde je Achse
  Vec3 gyroOffsetUnten;
  double gyroRauschen;       // Effektivwert in Grad pro Sekunde
  double beschleunigungRauschen;   // Effektivwert in g
};

inline SensorFehler fehlerKeine() {
  SensorFehler f;
  f.gyroOffsetOben = vec3(0.0, 0.0, 0.0);
  f.gyroOffsetUnten = vec3(0.0, 0.0, 0.0);
  f.gyroRauschen = 0.0;
  f.beschleunigungRauschen = 0.0;
  return f;
}

inline SensorFehler fehlerNurRauschen() {
  SensorFehler f = fehlerKeine();
  f.gyroRauschen = 0.1;
  f.beschleunigungRauschen = 0.01;
  return f;
}

// Offsets wie in AK-10: bis +-2 Grad pro Sekunde je Achse, plus Rauschen 0,1 Grad/s und 0,01 g.
inline SensorFehler fehlerMitOffset(int muster) {
  SensorFehler f = fehlerNurRauschen();
  if (muster == 0) {
    f.gyroOffsetOben = vec3(2.0, -2.0, 2.0);
    f.gyroOffsetUnten = vec3(-2.0, 2.0, -2.0);
  } else {
    f.gyroOffsetOben = vec3(-2.0, 2.0, -2.0);
    f.gyroOffsetUnten = vec3(2.0, 2.0, -2.0);
  }
  return f;
}

// Eigener, fester Zufallsgenerator (gleiche Zahlen auf jedem PC).
struct Zufall {
  unsigned long long zustand;
  explicit Zufall(unsigned long long samen) : zustand(samen) {}
  double gleich() {   // 0 bis 1
    zustand = zustand * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>(zustand >> 11) / 9007199254740992.0;
  }
  double normal() {   // Normalverteilung, Mittelwert 0, Streuung 1
    double u1 = gleich();
    if (u1 < 1e-12) u1 = 1e-12;
    double u2 = gleich();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI_ZAHL * u2);
  }
};

// ---------- Simulator ----------

struct ArmSim {
  Bewegung bew;
  Einbau einbau;
  SensorFehler fehler;
  Zufall zufall;
  double t;      // aktuelle Zeit in s
  double dt;     // Zeit pro Zyklus in s
  Pose pose;     // wahre Pose zum Zeitpunkt t
  Mat3 obenAlt;
  Mat3 untenAlt;

  ArmSim(const Bewegung &b, const Einbau &e, const SensorFehler &f, unsigned long long samen)
      : bew(b), einbau(e), fehler(f), zufall(samen), t(0.0), dt(DT_ZYKLUS) {
    pose = bew(0.0);
    lageMatrizen(pose, einbau, obenAlt, untenAlt);
  }

  double rausch(double sigma) {
    return (sigma > 0.0) ? sigma * zufall.normal() : 0.0;
  }

  void messeEine(const Mat3 &alt, const Mat3 &neu, const Vec3 &offset, ImuMessung &m) {
    Vec3 w = drehvektorGrad(matMal(matTransp(alt), neu));
    Vec3 acc = matMalVec(matTransp(neu), vec3(0.0, 0.0, 1.0));
    m.ax = static_cast<float>(acc.x + rausch(fehler.beschleunigungRauschen));
    m.ay = static_cast<float>(acc.y + rausch(fehler.beschleunigungRauschen));
    m.az = static_cast<float>(acc.z + rausch(fehler.beschleunigungRauschen));
    m.gx = static_cast<float>(w.x / dt + offset.x + rausch(fehler.gyroRauschen));
    m.gy = static_cast<float>(w.y / dt + offset.y + rausch(fehler.gyroRauschen));
    m.gz = static_cast<float>(w.z / dt + offset.z + rausch(fehler.gyroRauschen));
  }

  // Einen Zyklus weiter (dt) und die beiden Messungen liefern.
  void schritt(ImuMessung &oben, ImuMessung &unten) {
    double tNeu = t + dt;
    Pose pNeu = bew(tNeu);
    Mat3 obenNeu;
    Mat3 untenNeu;
    lageMatrizen(pNeu, einbau, obenNeu, untenNeu);
    messeEine(obenAlt, obenNeu, fehler.gyroOffsetOben, oben);
    messeEine(untenAlt, untenNeu, fehler.gyroOffsetUnten, unten);
    obenAlt = obenNeu;
    untenAlt = untenNeu;
    pose = pNeu;
    t = tNeu;
  }
};

// ---------- Bewegungs-Bausteine ----------

// Beugen und Strecken in der Bewegungsphase (4 s): 'zyklen' Mal von 0 bis -amplitude und zurück.
inline double beugeKurve(double s, double amplitude, int zyklen) {
  return -0.5 * amplitude * (1.0 - std::cos(2.0 * PI_ZAHL * zyklen * s / 4.0));
}

// Bewegung des Oberarms in Phase 2 (AK-6, AK-7d). s = Sekunden seit Beginn von Phase 2 (0 bis 4).
// Die Bewegung beginnt und endet in der Ausgangslage (genau ein Durchgang in 4 s).
// 'grad' = Grösse der Drehung in Grad (so weit weicht die Oberarm-IMU höchstens von der Ausgangslage ab).
inline void oberarmBewegen(Pose &p, double s, int art, double grad) {
  if (s < 0.0 || s > 4.0) return;
  double w = 2.0 * PI_ZAHL * s / 4.0;
  if (art == OBERARM_PENDELN) {
    // vor und zurück um eine waagrechte Achse (z. B. Arm hängt und schwingt nach vorn), Drehung = grad
    p.pitch = p.pitch - grad * 0.5 * (1.0 - std::cos(w));
  } else if (art == OBERARM_KREISE) {
    // ein kleiner Kreis: Pendeln nach vorn und zur Seite zugleich, beide Drehungen um waagrechte Achsen
    p.pitch = p.pitch - grad * 0.5 * (1.0 - std::cos(w));
    p.seit = p.seit + grad * 0.5 * std::sin(w);
  } else if (art == OBERARM_SENKRECHT) {
    // nur um die Senkrechte (Arm hängt!): ganzer Arm schwenkt (yaw) und Oberarm dreht sich in der Schulter (twist)
    p.yaw = p.yaw + grad * std::sin(w);
    p.twist = p.twist + grad * std::sin(2.0 * w);
  }
}

// ---------- Phase 3 des Einmessens (Stand: Team-Entscheidung "Achse messen statt annehmen") ----------
// Phase 3 dauert 1,5 s (6,0 bis 7,5 s nach dem Start des Einmessens): der Unterarm dreht sich EINMAL um die eigene
// Achse hin und zurück (0 -> grad -> 0), der Arm bleibt sonst ruhig.
const double PHASE3_DAUER = 1.5;
const double EINMESSEN_DAUER = 7.5;   // Phase 1 (2 s) + Phase 2 (4 s) + Phase 3 (1,5 s)

// Grösse der Hand-Drehung in Phase 3 in Grad (Normalfall 60, verlangt sind mindestens 45). Tests ändern den Wert
// vor dem Lauf und setzen ihn danach zurück. 0 = Hand wird nicht gedreht.
inline double &phase3Grad() {
  static double wert = 60.0;
  return wert;
}

// Art der Bewegung in Phase 3: 0 = Unterarm um die EIGENE Achse drehen (richtig),
// 1 = Unterarm beugen statt drehen (falsche Bewegung: Drehung um die Scharnier-Achse).
inline int &phase3Art() {
  static int wert = 0;
  return wert;
}

// s = Sekunden seit Beginn von Phase 3 (0 bis 1,5). Ändert p (pron bzw. flex) um die Hand-Drehung.
inline void phase3Bewegen(Pose &p, double s) {
  if (s < 0.0 || s > PHASE3_DAUER) return;
  double k = 0.5 * (1.0 - std::cos(2.0 * PI_ZAHL * s / PHASE3_DAUER));   // 0 -> 1 -> 0
  if (phase3Art() == 0) {
    p.pron = p.pron + phase3Grad() * k;
  } else {
    p.flex = p.flex - phase3Grad() * k;
  }
}

// Pose während EINER Einmessung, die bei t0 beginnt (Phase 1: 2 s still, Phase 2: 4 s Beugen + Oberarm, Phase 3: 1,5 s Hand drehen).
// p ist die Ausgangs-Pose (Grundlage). Rückgabe: true, wenn t in der Einmessung liegt (dann ist p gesetzt).
inline bool einmessenPose(Pose &p, double t, double t0, double amplitude, int zyklen, int obArt, double obGrad) {
  double s = t - t0;
  if (s < 0.0 || s >= EINMESSEN_DAUER) return false;
  if (s >= 6.0) {
    phase3Bewegen(p, s - 6.0);
  } else if (s >= 2.0) {
    p.flex = p.flex + beugeKurve(s - 2.0, amplitude, zyklen);
    oberarmBewegen(p, s - 2.0, obArt, obGrad);
  }
  return true;
}

// Zeitplan wie im Betrieb: 0-2 s still (Phase 1), 2-6 s Beugen/Strecken (Phase 2), 6-7,5 s Hand drehen (Phase 3), danach 'danach'.
// pronRate/yawRate (Grad pro Sekunde): Störung in Phase 1 (Arm nicht ruhig). Normal 0.
// obArt/obGrad: Bewegung des Oberarms in Phase 2 (siehe oberarmBewegen).
inline Bewegung mitEinmessenOberarm(const Pose &start, double amplitude, int zyklen,
                                    double pronRate, double yawRate, int obArt, double obGrad,
                                    const Danach &danach) {
  return [=](double t) -> Pose {
    Pose p = start;
    if (t < 2.0) {
      p.pron = start.pron + pronRate * t;
      p.yaw = start.yaw + yawRate * t;
      return p;
    }
    p.pron = start.pron + pronRate * 2.0;
    p.yaw = start.yaw + yawRate * 2.0;
    if (t < 6.0) {
      p.flex = start.flex + beugeKurve(t - 2.0, amplitude, zyklen);
      oberarmBewegen(p, t - 2.0, obArt, obGrad);
      return p;
    }
    if (t < EINMESSEN_DAUER) {   // Phase 3: Hand drehen (Arm sonst ruhig)
      p.flex = start.flex;
      phase3Bewegen(p, t - 6.0);
      return p;
    }
    return danach(t - EINMESSEN_DAUER, p);
  };
}

// Normalfall (AK-6): Einmessen im Hängen, 'zyklen' (normal 2) Mal beugen und dabei einen kleinen Kreis
// des Oberarms (20 Grad, mindestens 15 Grad verlangt die Anforderung).
inline Bewegung mitEinmessen(const Pose &start, double amplitude, int zyklen,
                             double pronRate, double yawRate, const Danach &danach) {
  return mitEinmessenOberarm(start, amplitude, zyklen, pronRate, yawRate, OBERARM_KREISE, 20.0, danach);
}

// "Normale Bewegungen": Beugen/Strecken zwischen 0 und -100 Grad mit 0,5 Hz, dazu Unterarm drehen,
// Oberarm drehen, Schwenken und den Arm heben/senken. tau = Bewegungszeit in s.
// pronAmp = Grösse der Unterarm-Drehung in Grad (0 = Unterarm nicht drehen).
inline Pose alltagPose(const Pose &basis, double tau, double pronAmp) {
  Pose p = basis;
  p.flex = basis.flex - 50.0 * (1.0 - std::cos(PI_ZAHL * tau));
  p.pron = basis.pron + pronAmp * std::sin(2.0 * PI_ZAHL * tau / 7.3);
  p.twist = basis.twist + 40.0 * std::sin(2.0 * PI_ZAHL * tau / 11.0);
  p.yaw = basis.yaw + 9.0 * tau;
  p.pitch = basis.pitch - 30.0 * (1.0 - std::cos(2.0 * PI_ZAHL * tau / 13.7));
  return p;
}

// Bewegungszeit mit Ruhepause: 29 s Bewegung, dann 1 s Stillstand, immer wieder.
inline double ruheZeit(double s) {
  double ganze = std::floor(s / 30.0);
  double rest = s - ganze * 30.0;
  double bewegt = (rest < 29.0) ? rest : 29.0;
  return ganze * 29.0 + bewegt;
}

// ---------- Ablauf-Helfer ----------

// Ein Zyklus: Simulator weiter, Logik füttern, danach die Abweichung lesen.
// Rückgabe: true, wenn die Abweichung bekannt ist (Wert dann in abw).
inline bool zyklusUndLesen(ArmSim &sim, EllbogenZustand &z, float &abw) {
  ImuMessung o = ImuMessung();
  ImuMessung u = ImuMessung();
  sim.schritt(o, u);
  ellbogenAktualisieren(z, o, u, static_cast<float>(sim.dt));
  return ellbogenAbweichung(z, abw);
}

inline void zyklenOhne(ArmSim &sim, EllbogenZustand &z, int anzahl) {
  for (int i = 0; i < anzahl; i++) {
    float abw = 0.0f;
    zyklusUndLesen(sim, z, abw);
  }
}

// Einmessen wie im Betrieb: Start, 2000 ms Phase 1 (100 Zyklen), 4000 ms Phase 2 (200 Zyklen),
// 1500 ms Phase 3 (75 Zyklen, Hand drehen), Abschluss. Zusammen 7500 ms.
inline bool einmessenLaufen(ArmSim &sim, EllbogenZustand &z, float minBeugung) {
  einmessenStart(z);
  zyklenOhne(sim, z, 100);
  einmessenBewegungStart(z);
  zyklenOhne(sim, z, 200);
  einmessenDrehungStart(z);
  zyklenOhne(sim, z, 75);
  return einmessenAbschliessen(z, minBeugung);
}

struct Messreihe {
  double maxFehler;      // grösster |abweichung - wahrer Winkel|
  double zeitMaxFehler;  // Zeit (s) dieses grössten Fehlers
  double maxAenderung;   // grösster |abweichung - bezug|
  double minAbw;         // kleinster gemessener Wert
  double maxAbw;         // grösster gemessener Wert
  double mittel;         // Mittelwert der gemessenen Werte
  int zyklen;
  int unbekannt;         // Zyklen, in denen die Abweichung nicht bekannt war
  int abgelehnt;         // Zyklen, die ellbogenAktualisieren abgelehnt hat
  int nichtEndlich;      // gemeldete Werte, die NaN oder unendlich waren
};

// Lässt den Simulator 'sekunden' laufen und wertet jeden Zyklus aus.
// referenzFlex: wahrer Winkel, bei dem die Referenz gesetzt wurde (Normalfall 0).
inline Messreihe laufe(ArmSim &sim, EllbogenZustand &z, double sekunden, double bezug,
                       double referenzFlex = 0.0) {
  Messreihe m;
  m.maxFehler = 0.0;
  m.zeitMaxFehler = 0.0;
  m.maxAenderung = 0.0;
  m.minAbw = 1e9;
  m.maxAbw = -1e9;
  m.mittel = 0.0;
  m.zyklen = 0;
  m.unbekannt = 0;
  m.abgelehnt = 0;
  m.nichtEndlich = 0;
  double summe = 0.0;
  int bekannte = 0;
  int anzahl = static_cast<int>(sekunden / sim.dt + 0.5);
  for (int i = 0; i < anzahl; i++) {
    ImuMessung o = ImuMessung();
    ImuMessung u = ImuMessung();
    sim.schritt(o, u);
    bool ok = ellbogenAktualisieren(z, o, u, static_cast<float>(sim.dt));
    m.zyklen++;
    if (!ok) m.abgelehnt++;
    float a = 0.0f;
    if (!ellbogenAbweichung(z, a)) {
      m.unbekannt++;
      continue;
    }
    if (!std::isfinite(a)) {
      m.nichtEndlich++;
      continue;
    }
    double wahr = sim.pose.flex - referenzFlex;
    double fehler = std::fabs(a - wahr);
    if (fehler > m.maxFehler) {
      m.maxFehler = fehler;
      m.zeitMaxFehler = sim.t;
    }
    double aenderung = std::fabs(a - bezug);
    if (aenderung > m.maxAenderung) m.maxAenderung = aenderung;
    if (a < m.minAbw) m.minAbw = a;
    if (a > m.maxAbw) m.maxAbw = a;
    summe += a;
    bekannte++;
  }
  if (bekannte > 0) m.mittel = summe / bekannte;
  return m;
}

inline void zeige(const char *name, const Messreihe &m) {
  std::printf("          %s: maxFehler = %.3f Grad (bei t = %.1f s), maxAenderung = %.3f, "
              "unbekannt = %d von %d, abgelehnt = %d\n",
              name, m.maxFehler, m.zeitMaxFehler, m.maxAenderung, m.unbekannt, m.zyklen, m.abgelehnt);
}

#endif
