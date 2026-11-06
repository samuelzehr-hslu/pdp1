// einmess_hilfe.h
// Gemeinsame Bausteine für die Einmess-Tests zu REQ-002 (Phase 3 "Hand drehen", AK-16 "schiefes Einmessen").
// Kein Produkt-Code, nur für die Tests. Wird von test_REQ-002_hand.cpp und test_REQ-002_schief_*.cpp benutzt.

#ifndef EINMESS_HILFE_H
#define EINMESS_HILFE_H

#include <cmath>
#include <cstdio>
#include <string>
#include "logik.h"
#include "sim_ellbogen.h"

const int ANZAHL_EINBAU_H = 5;

// Fünf verschiedene Einbaulagen der IMUs (Knochenachse = Sensor-Achse 0/1/2, verschieden verdreht, einmal ganz zufällig).
inline Einbau einbauNr(int k) {
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

// Oberarm in der Schulter gedreht (twist): bestimmt, in welche Richtung die Scharnier-Achse in der Welt zeigt.
inline double twistStart(int k) {
  return (k % 2 == 1) ? 90.0 : 0.0;
}

// Sensorfehler: Rauschen, bei zwei Einbauten zusätzlich Gyro-Offset +-2 Grad/s.
inline SensorFehler fehlerNr(int k) {
  if (k == 1) return fehlerMitOffset(0);
  if (k == 3) return fehlerMitOffset(1);
  return fehlerNurRauschen();
}

inline bool messOk(const Messreihe &m) {
  return m.unbekannt == 0 && m.maxFehler <= 2.5;
}

inline void merke(std::string &liste, int k) {
  if (!liste.empty()) liste += ", ";
  liste += std::to_string(k);
}

// Stellt die Hand-Drehung in Phase 3 für die Lebensdauer des Objekts ein und setzt sie danach wieder auf 60 Grad / Drehen.
struct Phase3Einstellung {
  double altGrad;
  int altArt;
  Phase3Einstellung(double grad, int art) : altGrad(phase3Grad()), altArt(phase3Art()) {
    phase3Grad() = grad;
    phase3Art() = art;
  }
  ~Phase3Einstellung() {
    phase3Grad() = altGrad;
    phase3Art() = altArt;
  }
};

#endif
