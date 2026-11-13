// sensor_tabelle.cpp: Arm-Bewegungen für das Testprogramm (REQ-007). Nur für die Tests.
// Benutzt den Arm-Simulator der Tests (sim_ellbogen.h, frei_hilfe.h). Die Winkel-Logik wird NICHT nachgebaut:
// Der Simulator erzeugt nur Rohwerte, so wie der MPU6050 sie liefern würde. Die echte Firmware rechnet damit.
// Alle Zeiten sind fest im Skript (nicht von der Firmware abhängig), damit jeder Lauf gleich ist.
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "frei_hilfe.h"
#include "sensor_tabelle.h"

struct Zeile {
  int16_t v[2][6];
};
static std::vector<Zeile> s_tabelle;

static int16_t roh(double wert, double faktor) {
  double x = std::floor(wert * faktor + 0.5);
  if (x > 32767.0) x = 32767.0;
  if (x < -32768.0) x = -32768.0;
  return static_cast<int16_t>(x);
}

static double begrenzt(double x) { return weich(x); }

// Überstreckung (flex in Grad) über der Zeit für das Szenario "normal" ab 25 s
static double flexNormal(double t) {
  const double ZWEI_PI = 2.0 * PI_ZAHL;
  if (t < 26.0) return 0.0;
  if (t < 27.0) return 12.0 * begrenzt(t - 26.0);
  if (t < 29.5) return 12.0;
  if (t < 30.5) return 12.0 * (1.0 - begrenzt(t - 29.5));
  if (t < 31.0) return 5.0 * begrenzt((t - 30.5) / 0.5);
  if (t < 35.0) return 5.0 + 1.5 * std::sin(ZWEI_PI * 3.0 * (t - 31.0));   // Zittern um den Grenzwert
  if (t < 36.0) return 5.0 * (1.0 - begrenzt(t - 35.0));
  if (t < 41.0) return 0.0;
  if (t < 42.0) return 12.0 * begrenzt(t - 41.0);
  if (t < 55.0) return 12.0;
  if (t < 56.0) return 12.0 * (1.0 - begrenzt(t - 55.0));
  return 0.0;
}

static Bewegung bewegungBauen(const std::string &name) {
  Pose basis = grundLage(0);
  if (name == "normal") {
    Skript sk;
    sk.basis = basis;
    planTeil(sk.plan, T_BEUGEN, 70.0, 1.0, 2.0);
    planTeil(sk.plan, T_HAND, 70.0, 1.0, 2.0);
    planTeil(sk.plan, T_KREIS, 30.0, 1.0, 2.0);
    skriptFertig(sk);
    *sk.t2 = 7.0;   // Schritt 2 beginnt bei 7 s (die Taste wird bei 7,0 s gedrückt)
    Bewegung plan = skriptBewegung(sk);
    return [plan, basis](double t) -> Pose {
      if (t < 25.0) return plan(t);
      Pose p = basis;
      p.flex = flexNormal(t);
      return p;
    };
  }
  if (name == "abbruch") {
    // 3 bis 40 s: der ganze Arm schwenkt dauernd (nie ruhig). Danach alles still.
    return [basis](double t) -> Pose {
      Pose p = basis;
      if (t >= 3.0 && t < 40.0) p.yaw += 40.0 * std::sin(2.0 * PI_ZAHL * 1.0 * (t - 3.0));
      return p;
    };
  }
  // "ohne_imu2": der Arm liegt still
  return [basis](double) -> Pose { return basis; };
}

unsigned long sensorSzenarioLaden(const char *name) {
  std::string n(name);
  unsigned long dauerMs = 0;
  if (n == "normal") dauerMs = 110000UL;
  else if (n == "abbruch") dauerMs = 125000UL;
  else if (n == "ohne_imu2") dauerMs = 25000UL;
  else return 0;
  ArmSim sim(bewegungBauen(n), einbauStandard(), fehlerNurRauschen(), 42);
  size_t schritte = static_cast<size_t>(dauerMs / 20UL) + 5;
  s_tabelle.clear();
  for (size_t k = 0; k < schritte; k++) {
    ImuMessung oben;
    ImuMessung unten;
    sim.schritt(oben, unten);
    Zeile z;
    const ImuMessung *m[2] = {&oben, &unten};
    for (int i = 0; i < 2; i++) {
      z.v[i][0] = roh(m[i]->ax, 16384.0);
      z.v[i][1] = roh(m[i]->ay, 16384.0);
      z.v[i][2] = roh(m[i]->az, 16384.0);
      z.v[i][3] = roh(m[i]->gx, 65.5);
      z.v[i][4] = roh(m[i]->gy, 65.5);
      z.v[i][5] = roh(m[i]->gz, 65.5);
    }
    s_tabelle.push_back(z);
  }
  return dauerMs;
}

void sensorRoh(int imu, unsigned long ms, int16_t out[6]) {
  size_t k = static_cast<size_t>(ms / 20UL);
  if (s_tabelle.empty()) {
    for (int i = 0; i < 6; i++) out[i] = 0;
    return;
  }
  if (k >= s_tabelle.size()) k = s_tabelle.size() - 1;
  std::memcpy(out, s_tabelle[k].v[imu], 6 * sizeof(int16_t));
}
