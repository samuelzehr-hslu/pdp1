// frei_hilfe.h
// Gemeinsame Bausteine für die Tests zu REQ-005 (Einmessen: still halten, dann FREI bewegen).
// Kein Produkt-Code, nur für die Tests. Benutzt von test_REQ-005.cpp und test_REQ-005_frei.cpp.
//
// Idee: Ein "Plan" ist eine Liste von Bewegungen (Beugen, Oberarm-Kreis, Hand drehen, Schwenken ...), jede mit Startzeit,
// Grösse und Tempo. Der Plan beginnt erst, wenn das Einmessen in Schritt 2 ist (die Person hat bestätigt). Vorher bleibt der
// Arm in der Grundlage (oder macht eine Störung, siehe "vorher"). So kann der Test den Arm "frei" bewegen, ohne dass die
// Zeitpunkte von Schritt 1 und 2 fest im Skript stehen. Die Winkel-Logik wird NICHT nachgebaut: der Simulator (sim_ellbogen.h)
// erzeugt nur Sensorwerte, die echte Logik (logik.cpp) muss selbst entscheiden.
//
// Jede Bewegung geht am Ende in die Ausgangslage zurück (hin und zurück). Mehrere Bewegungen dürfen sich überlappen
// (gleichzeitig), dann addieren sich ihre Ausschläge.

#ifndef FREI_HILFE_H
#define FREI_HILFE_H

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>
#include "logik.h"
#include "sim_ellbogen.h"
#include "einmess_hilfe.h"

// Arten von Bewegung
enum {
  T_BEUGEN = 0,      // Ellbogen von 0 bis -groesse und zurück, 'n' Mal
  T_KREIS = 1,       // Oberarm-Kreis: nach vorn 'groesse' Grad und dabei seitlich hin und her (Spannweite je 'groesse' Grad)
  T_OBERARM_VORN = 2,// Oberarm nur vor und zurück (nur EINE Richtung), 'groesse' Grad
  T_HAND = 3,        // Unterarm um die eigene Achse drehen: 0 bis groesse und zurück ('n' Mal)
  T_SCHWENK = 4      // ganzer Arm schwenkt waagrecht (um die Senkrechte), +- groesse Grad
};

struct Teil {
  int art;
  double start;    // s seit Beginn von Schritt 2
  double dauer;    // s
  double groesse;  // Grad
  double freq;     // Hz (Tempo)
};

struct Plan {
  std::vector<Teil> teile;
  double ende;     // s seit Beginn von Schritt 2, wann die letzte Bewegung fertig ist
  Plan() : ende(0.0) {}
};

// Neue Bewegung nach der letzten (oder gleichzeitig mit der vorigen). n = Anzahl Hin-und-zurück.
inline void planTeil(Plan &pl, int art, double groesse, double freq, double n, bool gleichzeitig = false) {
  Teil t;
  t.art = art;
  t.groesse = groesse;
  t.freq = freq;
  t.dauer = n / freq;
  t.start = (gleichzeitig && !pl.teile.empty()) ? pl.teile.back().start : pl.ende;
  if (t.start + t.dauer > pl.ende) pl.ende = t.start + t.dauer;
  pl.teile.push_back(t);
}

inline void planPause(Plan &pl, double sekunden) {
  pl.ende += sekunden;
}

// Pose zur Zeit s (Sekunden seit Beginn von Schritt 2), nur durch den Plan (ohne Prüfphase).
inline Pose planPose(const Plan &pl, const Pose &basis, double s) {
  Pose p = basis;
  for (size_t i = 0; i < pl.teile.size(); i++) {
    const Teil &t = pl.teile[i];
    double u = s - t.start;
    if (u < 0.0 || u > t.dauer) continue;
    double w = 2.0 * PI_ZAHL * t.freq * u;
    double k = 0.5 * (1.0 - std::cos(w));   // 0 -> 1 -> 0 pro Hin-und-zurück
    if (t.art == T_BEUGEN) {
      p.flex -= t.groesse * k;
    } else if (t.art == T_KREIS) {
      p.pitch -= t.groesse * k;
      p.seit += 0.5 * t.groesse * std::sin(w);
    } else if (t.art == T_OBERARM_VORN) {
      p.pitch -= t.groesse * k;
    } else if (t.art == T_HAND) {
      p.pron += t.groesse * k;
    } else if (t.art == T_SCHWENK) {
      p.yaw += t.groesse * std::sin(w);
    }
  }
  return p;
}

// Ein Skript = Grundlage + Plan + (optional) Störung vor Schritt 2 + Prüfphase am Ende ("Alltag", 6 s).
struct Skript {
  Pose basis;
  Plan plan;
  std::shared_ptr<double> t2;                          // Beginn von Schritt 2 in Simulationszeit, -1 = noch nicht
  double pruefAb;                                      // s nach Beginn von Schritt 2: ab hier Alltags-Bewegung (6 s)
  std::function<Pose(double, const Pose &)> vorher;    // Bewegung vor Schritt 2 (absolute Zeit), leer = still
  Skript() : basis(poseHaengend()), t2(new double(-1.0)), pruefAb(0.0) {}
};

inline void skriptFertig(Skript &sk) {
  sk.pruefAb = sk.plan.ende + 3.5;   // 3,5 s still halten (Platz zum Messen der Referenz), dann Alltag
}

inline double skriptEnde(const Skript &sk) {
  return sk.pruefAb + 6.0;
}

inline Bewegung skriptBewegung(const Skript &sk) {
  Skript k = sk;   // Kopie, der Zeiger t2 wird mit dem Original geteilt
  return [k](double t) -> Pose {
    if (*k.t2 < 0.0) {
      if (k.vorher) return k.vorher(t, k.basis);
      return k.basis;
    }
    double s = t - *k.t2;
    if (s >= k.pruefAb) return alltagPose(k.basis, s - k.pruefAb, 60.0);
    return planPose(k.plan, k.basis, s);
  };
}

// ---------- Ablauf wie im Betrieb ----------

inline bool alleOk(const EinmessFortschritt &f) {
  return f.beugungGrad >= 30.0f && f.oberarmGrad >= 15.0f && f.handGrad >= 45.0f && f.achseProzent == 100;
}

struct Ergebnis {
  bool ok;                 // Einmessen erfolgreich (einmessenAbschliessen lieferte true)
  double stillS;           // Dauer von Schritt 1 in s
  double dauerS2;          // Dauer von Schritt 2 bis zum Erfolg (oder bis zum Abbruch) in s
  int widerspruch;         // Polls, bei denen "alleOk(Fortschritt)" und "einmessenAbschliessen" nicht übereinstimmten
  int fehlenderPasst;      // Polls, bei denen einmessenFehlenderGrund == KEINER nicht zu alleOk passte
  int rueckwaerts;         // Polls, bei denen einer der drei Grad-Werte kleiner wurde
  int ungueltig;           // Polls mit NaN/unendlich/negativen Grad-Werten oder Achse über 100
  int unbekannt;           // nach Erfolg: Zyklen ohne bekannte Abweichung
  double maxFehler;        // nach Erfolg: grösster Winkelfehler
  double mittelStill;      // nach Erfolg: Mittelwert der Abweichung im Stillhalten nach dem Plan (NAN = nicht gemessen)
  uint8_t grund;           // bei Misserfolg: einmessenFehlenderGrund vor dem Abbruch
  uint8_t grundGespeichert;// bei Misserfolg: einmessenGrund nach dem Abbruch
  EinmessFortschritt letzte;
};

inline Ergebnis ergebnisLeer() {
  Ergebnis e;
  e.ok = false;
  e.stillS = 0.0;
  e.dauerS2 = 0.0;
  e.widerspruch = 0;
  e.fehlenderPasst = 0;
  e.rueckwaerts = 0;
  e.ungueltig = 0;
  e.unbekannt = 0;
  e.maxFehler = 0.0;
  e.mittelStill = NAN;
  e.grund = EINMESSEN_GRUND_KEINER;
  e.grundGespeichert = EINMESSEN_GRUND_KEINER;
  e.letzte = EinmessFortschritt();
  return e;
}

// Prüft einen Satz Fortschrittswerte auf Unsinn und merkt, ob ein Grad-Wert kleiner wurde.
inline void fortschrittKontrolle(const EinmessFortschritt &f, const EinmessFortschritt &alt, Ergebnis &e) {
  bool endlich = std::isfinite(f.ruhigSekunden) && std::isfinite(f.beugungGrad) && std::isfinite(f.oberarmGrad) &&
                 std::isfinite(f.handGrad);
  if (!endlich || f.beugungGrad < 0.0f || f.oberarmGrad < 0.0f || f.handGrad < 0.0f || f.achseProzent > 100) {
    e.ungueltig++;
    return;
  }
  if (f.beugungGrad < alt.beugungGrad - 1e-3f || f.oberarmGrad < alt.oberarmGrad - 1e-3f ||
      f.handGrad < alt.handGrad - 1e-3f) {
    e.rueckwaerts++;
  }
}

// Ein ganzes Einmessen wie im Betrieb (REQ-005), mit sofortigen Bestätigungen:
//  - Schritt 1: Zyklen, bis einmessenStillFertig (höchstens 30 s, sonst Abbruch 'ruhig')
//  - wartenS: Sekunden im Warte-Zustand (der Arm macht dort, was 'vorher' sagt; es läuft nur die Messung)
//  - Schritt 2: alle 1000 ms einmessenFortschritt + einmessenAbschliessen, höchstens 30 s
//  - bei Erfolg: bis zum Ende des Skripts weitermessen (Fehler, Stillhalte-Mittelwert), bei Misserfolg: Abbruch mit dem
//    ersten fehlenden Grund
inline Ergebnis frei(ArmSim &sim, EllbogenZustand &z, Skript &sk, double wartenS = 0.0) {
  Ergebnis e = ergebnisLeer();
  einmessenStart(z);
  int n = 0;
  while (!einmessenStillFertig(z) && n < 1500) {
    zyklenOhne(sim, z, 1);
    n++;
  }
  e.stillS = n * sim.dt;
  if (!einmessenStillFertig(z)) {
    e.grund = EINMESSEN_GRUND_RUHIG;
    einmessenAbbrechen(z, e.grund);
    e.grundGespeichert = einmessenGrund(z);
    return e;
  }
  if (wartenS > 0.0) zyklenOhne(sim, z, static_cast<int>(wartenS / sim.dt + 0.5));
  *sk.t2 = sim.t;
  einmessenBewegungStart(z);
  EinmessFortschritt alt = EinmessFortschritt();
  for (int i = 1; i <= 1500; i++) {
    zyklenOhne(sim, z, 1);
    if (i % 50 != 0) continue;
    // Reihenfolge laut REQ-005 (Schnittstelle): erst die Auswertung, danach der Fortschritt (stammt aus dieser Auswertung).
    bool klappt = einmessenAbschliessen(z);
    if (klappt) {
      // Nach dem Erfolg ist Schritt 2 vorbei (Fortschritt = 0). Die vier Werte stehen aber noch: Erfolg nur, wenn alle OK.
      if (einmessenFehlenderGrund(z) != EINMESSEN_GRUND_KEINER) e.widerspruch++;
      e.ok = true;
      e.dauerS2 = i * sim.dt;
      break;
    }
    EinmessFortschritt f = EinmessFortschritt();
    einmessenFortschritt(z, f);
    fortschrittKontrolle(f, alt, e);
    alt = f;
    e.letzte = f;
    bool alles = alleOk(f);
    bool keiner = (einmessenFehlenderGrund(z) == EINMESSEN_GRUND_KEINER);
    if (keiner != alles) e.fehlenderPasst++;
    if (alles) e.widerspruch++;   // alle vier OK, aber kein Erfolg: darf nicht sein
  }
  if (!e.ok) {
    e.dauerS2 = 30.0;
    e.grund = einmessenFehlenderGrund(z);
    einmessenAbbrechen(z, e.grund);
    e.grundGespeichert = einmessenGrund(z);
    return e;
  }
  // Nach dem Erfolg: Fehler messen bis zum Ende des Skripts. Dazwischen 1 s Stillhalten nach dem Plan: daraus der
  // Mittelwert der Abweichung (= Fehler der Referenz). Liegt der Erfolg zu spät dafür, bleibt mittelStill = NAN.
  double fensterVon = std::fmax(*sk.t2 + sk.plan.ende + 0.3, sim.t + 0.3);
  double fensterBis = fensterVon + 1.0;
  Messreihe a = Messreihe();
  double groesster = 0.0;
  if (fensterBis <= *sk.t2 + sk.pruefAb) {
    a = laufe(sim, z, fensterVon - sim.t, 0.0, sk.basis.flex);
    Messreihe b = laufe(sim, z, 1.0, 0.0, sk.basis.flex);
    e.mittelStill = b.mittel;
    e.unbekannt += b.unbekannt;
    groesster = b.maxFehler;
  }
  double rest = *sk.t2 + skriptEnde(sk) - sim.t;
  if (rest < 2.0) rest = 2.0;
  if (rest > 10.0) rest = 10.0;   // Skripte ohne Prüfphase (pruefAb sehr gross): höchstens 10 s weitermessen
  Messreihe c = laufe(sim, z, rest, 0.0, sk.basis.flex);
  e.unbekannt += a.unbekannt + c.unbekannt;
  e.maxFehler = std::fmax(groesster, std::fmax(a.maxFehler, c.maxFehler));
  return e;
}

// Grundlage: Arm hängt, Oberarm je nach Einbau etwas verdreht (wie in den älteren Tests).
inline Pose grundLage(int einbauNummer) {
  Pose b = poseHaengend();
  b.twist = twistStart(einbauNummer);
  return b;
}

#endif
