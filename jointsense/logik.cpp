// logik.cpp
// Hier steht, WIE die Funktionen aus logik.h arbeiten.
// Keine Arduino-Befehle in dieser Datei (siehe Erklärung in logik.h).

#include <math.h>
#ifdef LOGIK_DEBUG
#include <stdio.h>
extern float logikDbgAnstieg, logikDbgRival, logikDbgFpm;
#endif
#include "logik.h"

// Die Kreiszahl Pi. Eigener Name, weil Arduino schon ein "PI" hat
// und wir diese Datei auch ohne Arduino benutzen wollen.
const double LOGIK_PI = 3.1415926535897932384626433832795;


float neigungAusBeschleunigung(float x, float y, float z) {
  float neigung = atan2(x, sqrt(y * y + z * z)) * 180.0 / LOGIK_PI;
  return neigung;
}


float ellbogenWinkel(float neigungOberarm, float neigungUnterarm) {
  return neigungUnterarm - neigungOberarm;
}


float referenzBerechnen(float summe, int anzahl) {
  if (anzahl <= 0) {
    return 0.0;                       // nie durch 0 teilen
  }
  return summe / anzahl;
}


bool grenzwertPruefen(GrenzwertZustand &zustand, float abweichungGrad, uint32_t jetztMs,
                      float grenzwertGrad, float hystereseGrad, uint32_t entprellMs) {
  // Sicherheit (fail-safe): Kein gültiger Wert -> Motor aus.
  // NaN = "keine Zahl", z. B. nach einem Rechenfehler. isinf = unendlich gross.
  if (isnan(abweichungGrad) || isinf(abweichungGrad)) {
    zustand.zuWeitSeit = 0;
    zustand.warnungAktiv = false;
    return false;
  }

  // Hysterese: Läuft der Motor schon, gilt eine etwas tiefere Grenze.
  // So flackert der Motor nicht, wenn der Arm genau an der Grenze ist.
  float grenze = grenzwertGrad;
  if (zustand.warnungAktiv) {
    grenze = grenzwertGrad - hystereseGrad;
  }

  if (abweichungGrad <= grenze) {
    zustand.zuWeitSeit = 0;           // Arm ist im erlaubten Bereich
    zustand.warnungAktiv = false;
    return false;
  }

  // Arm ist zu weit. Seit wann?
  if (zustand.zuWeitSeit == 0) {
    zustand.zuWeitSeit = jetztMs;
  }

  // Entprellung: Erst warnen, wenn es lange genug zu weit ist
  if (jetztMs - zustand.zuWeitSeit >= entprellMs) {
    zustand.warnungAktiv = true;
    return true;
  }

  // Noch nicht lange genug zu weit: bisherigen Zustand beibehalten
  return zustand.warnungAktiv;
}


bool tasteNeuGedrueckt(TasteZustand &zustand, bool istGedrueckt, uint32_t jetztMs,
                       uint32_t prellMs) {
  if (istGedrueckt == zustand.warGedrueckt) {
    return false;                     // nichts hat sich geändert
  }
  if (jetztMs - zustand.letzteAenderung < prellMs) {
    return false;                     // Taste "prellt" noch, ignorieren
  }
  zustand.letzteAenderung = jetztMs;
  zustand.warGedrueckt = istGedrueckt;
  return istGedrueckt;                // nur beim Drücken true, nicht beim Loslassen
}


bool imuStatusAktualisieren(ImuStatus &status, bool messungOk, uint8_t schwelleFehler) {
  if (messungOk) {
    status.fehlerInFolge = 0;         // Messung hat geklappt: alles wieder gut
    status.ausgefallen = false;
    return false;
  }

  if (status.fehlerInFolge < 255) {
    status.fehlerInFolge = status.fehlerInFolge + 1;   // bei 255 stehen bleiben (kein Überlauf)
  }

  if (schwelleFehler == 0) {
    schwelleFehler = 1;               // Schwelle 0 gilt wie 1
  }
  status.ausgefallen = (status.fehlerInFolge >= schwelleFehler);
  return status.ausgefallen;
}


bool fehlerMusterMotorAn(FehlerMusterZustand &zustand, bool ausfall, uint32_t jetztMs) {
  if (!ausfall) {
    zustand.aktiv = false;            // kein Ausfall: Muster zurücksetzen, Motor nicht wegen Fehler
    zustand.startMs = 0;
    return false;
  }

  if (!zustand.aktiv) {
    zustand.aktiv = true;             // Der Ausfall beginnt gerade
    zustand.startMs = jetztMs;
  }

  // Wie lange läuft das Muster schon in der aktuellen 5-Sekunden-Runde?
  // Die Subtraktion funktioniert auch, wenn millis() überläuft (uint32_t).
  uint32_t t = (jetztMs - zustand.startMs) % 5000;

  // 3 Stösse: 0-100 ms, 200-300 ms, 400-500 ms. Sonst ist der Motor aus.
  if (t < 100) {
    return true;
  }
  if (t >= 200 && t < 300) {
    return true;
  }
  if (t >= 400 && t < 500) {
    return true;
  }
  return false;
}


bool motorSollAn(bool sensorAusfall, bool zuWeit, bool fehlerMusterAn) {
  if (sensorAusfall) {
    return fehlerMusterAn;            // Ausfall hat Vorrang: Überstreckung wird ignoriert
  }
  return zuWeit;
}


// =====================================================================
// Winkelmessung um die Scharnier-Achse (REQ-002, Runde 2)
// =====================================================================
// Erklärung der Namen:
//   IMU 1 = Oberarm, IMU 2 = Unterarm.
//   q          = Drehung, die einen Vektor aus den Achsen von IMU 2 in die Achsen von IMU 1 bringt.
//   achse1     = Scharnier-Achse des Ellbogens (in den Achsen von IMU 1).
//   knochen2   = Knochenachse des Unterarms (in den Achsen von IMU 2). Drehen des Unterarms
//                (Hand drehen) dreht die IMU 2 genau um diese Achse.
//   Ellbogenwinkel = Drehung der Knochenachse des Unterarms um die Scharnier-Achse.

#include <stddef.h>   // für size_t

// Die Hilfsfunktionen sollen nicht in jede Stelle hineinkopiert werden (spart Platz im Flash des Arduino).
#define NICHT_INLINE __attribute__((noinline))

// ---------- Einstellungen der Winkelmessung ----------
static const float MAX_BESCHLEUNIGUNG   = 16.0f;   // g: mehr ist physikalisch unmöglich (AK-14)
static const float MAX_DREHGESCHWINDIGKEIT = 2000.0f; // Grad/s: mehr ist unmöglich (AK-14)
static const float MAX_DT               = 0.5f;    // s: längere Pausen zwischen Messungen gelten als ungültig
static const float G_MIN                = 0.92f;   // Beschleunigung zählt nur, wenn ihr Betrag
static const float G_MAX                = 1.08f;   //   nahe bei 1 g liegt (sonst bewegt sich der Arm zu wild)
static const float KORREKTUR_RATE       = 0.8f;    // 1/s: so stark zieht die Schwerkraft die Drehung zurecht
static const float KORREKTUR_RATE_SCHARNIER = 0.3f; // 1/s: so stark zieht die Scharnier-Bedingung die Drehung zurecht
static const float BIAS_LERNRATE       = 0.03f;   // 1/s^2: so schnell wird der Gyro-Fehler aus der Schwerkraft nachgeführt
static const float LAGE_MAX_ZEIT        = 4.5f;    // s: spätestens dann gilt die Lage wieder als bekannt (AK-13)
static const uint8_t LAGE_MIN_ANZAHL    = 25;      // so viele gleichmässige Schwerkraft-Werte finden die Lage wieder
static const float LAGE_MIN_AMPLITUDE   = 0.25f;   // Schwerkraft muss genug quer zur Scharnier-Achse liegen
static const float LAGE_MAX_ABWEICHUNG  = 0.15f;   // Schwerkraft entlang der Scharnier-Achse darf bei beiden IMUs so weit abweichen
static const float ZWISCHEN_ABSTAND     = 0.25f;   // s zwischen zwei Zwischenwerten beim Einmessen
static const float ZWISCHEN_SKALA       = 16000.0f; // Zwischenwerte sind Quaternion-Zahlen mal diesen Wert
static const float BIAS_AUSREISSER     = 1.5f;    // Grad/s: Gyro-Werte in Phase 1, die weiter vom Mittelwert weg sind, zählen nicht
static const float RUHIG_MAX_BETRAG     = 10.0f;   // Grad/s: Phase 1 darf im Mittel nicht schneller sein (AK-7 b)
static const uint16_t MIN_ANZAHL_PHASE1 = 5;       // so wenige Messungen brauchen wir mindestens in Phase 1
static const uint16_t MIN_ANZAHL_ACHSE  = 8;       // ... und für die Achsen-Suche in Phase 2
// Achsen-Suche: angenommene Messfehler (nur für die Gewichtung der Messungen)
static const float GEWICHT_SCHWERKRAFT  = 3000.0f; // 1 / (Fehler der Schwerkraft-Bedingung) ^ 2
static const float GEWICHT_KNOCHEN      = 3000.0f; // so stark zählt: Scharnier-Achse steht quer zur Knochenachse des Unterarms
static const float OBERARM_KIPP_COS     = 0.99692f; // Cosinus von 4,5 Grad: so weit muss sich der Oberarm in Phase 2 mindestens neigen
static const float KIPPEN_GLAETTUNG     = 0.2f;    // Glättung der Schwerkraft-Richtung des Oberarms (pro Messung)
static const float ACHSE_LAMBDA_MIN     = 3000.0f; // so stark muss die schwächste Richtung der Achsen-Bestimmung belegt sein (sonst nicht eindeutig)
static const float ACHSE_FEHLER_MAX     = 30.0f;   // Rest-Fehler pro Messung darf höchstens so gross sein
static const float DREH_MIN_TEMPO       = 0.35f;   // rad/s (20 Grad/s): so schnell muss sich die Hand in Phase 3 drehen
static const float DREH_MIN_WEG         = 0.70f;   // rad (40 Grad): so weit muss die Hand in Phase 3 insgesamt gedreht werden (hin und zurück)
static const float DREH_MAX_OBERARM     = 0.5f;    // der Oberarm darf in Phase 3 höchstens halb so viel drehen wie der Unterarm
static const float DREH_QUER_MAX        = 0.5f;    // so viel (Cosinus) darf die Scharnier-Achse in Richtung der Knochenachse zeigen

// ---------- kleine Helfer für 3er-Vektoren (v[0], v[1], v[2]) ----------

// Skalarprodukt zweier Vektoren.
NICHT_INLINE static float punkt3(const float *a, const float *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Länge eines Vektors.
NICHT_INLINE static float betrag3(const float *a) {
  return sqrtf(punkt3(a, a));
}

// Kreuzprodukt r = a x b (r darf nicht gleich a oder b sein).
NICHT_INLINE static void kreuz3(const float *a, const float *b, float *r) {
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

// Macht den Vektor genau 1 lang. false, wenn er zu kurz (oder ungültig) ist.
NICHT_INLINE static bool normieren3(float *v) {
  float n = betrag3(v);
  if (!(n > 1e-6f)) {
    return false;
  }
  v[0] /= n;
  v[1] /= n;
  v[2] /= n;
  return true;
}

// Ist der Wert eine gültige Zahl und im erlaubten Bereich?
NICHT_INLINE static bool wertGueltig(float wert, float maxBetrag) {
  if (isnan(wert) || isinf(wert)) {
    return false;
  }
  return fabsf(wert) <= maxBetrag;
}

// Packt die Messung in zwei 3er-Vektoren (Beschleunigung und Gyro).
NICHT_INLINE static void messungZerlegen(const ImuMessung &m, float *a, float *w) {
  a[0] = m.ax; a[1] = m.ay; a[2] = m.az;
  w[0] = m.gx; w[1] = m.gy; w[2] = m.gz;
}

static const float DEG_ZU_RAD = (float)(LOGIK_PI / 180.0);
static const float RAD_ZU_GRAD = (float)(180.0 / LOGIK_PI);

// Winkel in Grad auf den Bereich -180 bis +180 bringen (Eingabe liegt zwischen -540 und +540).
NICHT_INLINE static float winkelKuerzen(float w) {
  if (w > 180.0f) {
    w -= 360.0f;
  } else if (w <= -180.0f) {
    w += 360.0f;
  }
  return w;
}

// Ein Vektor e, der senkrecht auf a steht (a hat Länge 1). e hat Länge 1.
NICHT_INLINE static void senkrecht3(const float *a, float *e) {
  // Kreuzprodukt mit der Achse, in die a am wenigsten zeigt
  float t[3] = {0.0f, 0.0f, 0.0f};
  int k = 0;
  if (fabsf(a[1]) < fabsf(a[k])) {
    k = 1;
  }
  if (fabsf(a[2]) < fabsf(a[k])) {
    k = 2;
  }
  t[k] = 1.0f;
  kreuz3(a, t, e);
  normieren3(e);
}

// ---------- kleine Helfer für Quaternionen q[0..3] = (w, x, y, z) ----------
// Ein Quaternion beschreibt eine Drehung im Raum mit 4 Zahlen.

// r = a * b (erst b, dann a drehen). r darf nicht gleich a oder b sein.
NICHT_INLINE static void qMal(const float *a, const float *b, float *r) {
  float k[3];
  kreuz3(a + 1, b + 1, k);
  r[0] = a[0] * b[0] - punkt3(a + 1, b + 1);
  for (int i = 0; i < 3; i++) {
    r[1 + i] = a[0] * b[1 + i] + b[0] * a[1 + i] + k[i];
  }
}

// Umgekehrte Drehung.
NICHT_INLINE static void qUmkehr(const float *a, float *r) {
  r[0] = a[0];
  r[1] = -a[1];
  r[2] = -a[2];
  r[3] = -a[3];
}

// Länge 1 machen (gegen Rechenfehler). Bei ungültigem Wert: keine Drehung.
NICHT_INLINE static void qNormieren(float *q) {
  float n2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
  if (!(n2 > 1e-12f) || isinf(n2)) {
    q[0] = 1.0f;
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = 0.0f;
    return;
  }
  float f = 1.0f / sqrtf(n2);
  for (int i = 0; i < 4; i++) {
    q[i] *= f;
  }
}

// Drehung aus einem Drehvektor (Richtung = Achse, Länge = Winkel in Bogenmass).
NICHT_INLINE static void qAusDrehvektor(const float *phi, float *q) {
  float h = 0.5f * betrag3(phi);          // halber Winkel
  float sproh = 1.0f;                     // sin(h) / h
  if (h > 1.0e-4f) {
    sproh = sinf(h) / h;
  }
  q[0] = cosf(h);
  for (int i = 0; i < 3; i++) {
    q[1 + i] = 0.5f * sproh * phi[i];
  }
}

// Drehung um eine Achse (Länge 1) mit Winkel in Grad.
NICHT_INLINE static void qAusAchse(const float *achse, float winkelGrad, float *q) {
  float h = 0.5f * winkelGrad * DEG_ZU_RAD;
  float s = sinf(h);
  q[0] = cosf(h);
  for (int i = 0; i < 3; i++) {
    q[1 + i] = s * achse[i];
  }
}

// Drehmatrix m (3x3, Zeile für Zeile) aus dem Quaternion (Länge 1).
NICHT_INLINE static void qZuMatrix(const float *q, float *m) {
  float ww = q[0] * q[0] - punkt3(q + 1, q + 1);
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m[3 * i + j] = 2.0f * q[1 + i] * q[1 + j] + ((i == j) ? ww : 0.0f);
    }
  }
  float wx = 2.0f * q[0] * q[1], wy = 2.0f * q[0] * q[2], wz = 2.0f * q[0] * q[3];
  m[1] -= wz;
  m[2] += wy;
  m[3] += wz;
  m[5] -= wx;
  m[6] -= wy;
  m[7] += wx;
}

// r = m * v (Matrix mal Vektor). Mit transponiert = true: r = m^T * v.
NICHT_INLINE static void matMalVektor(const float *m, const float *v, float *r, bool transponiert = false) {
  for (int i = 0; i < 3; i++) {
    r[i] = 0.0f;
    for (int k = 0; k < 3; k++) {
      r[i] += m[transponiert ? 3 * k + i : 3 * i + k] * v[k];
    }
  }
}

static const float nullVektor[3] = {0.0f, 0.0f, 0.0f};

// Dreht die relative Drehung q um einen Messschritt weiter.
//   r1, r2: Drehgeschwindigkeit von IMU 1 und IMU 2 in rad/s (ohne Gyro-Fehler)
//   dt: Dauer des Schritts in s
// Neue Drehung = (Schritt von IMU 1 rückwärts) * q * (Schritt von IMU 2).
NICHT_INLINE static void relativDrehen(float *q, const float *r1, const float *r2, float dt) {
  float phi1[3];
  float phi2[3];
  for (int i = 0; i < 3; i++) {
    phi1[i] = r1[i] * dt;
    phi2[i] = r2[i] * dt;
  }
  float d1[4];
  float d2[4];
  float d1u[4];
  float t[4];
  qAusDrehvektor(phi1, d1);
  qAusDrehvektor(phi2, d2);
  qUmkehr(d1, d1u);
  qMal(d1u, q, t);
  qMal(t, d2, q);
  qNormieren(q);
}

// ---------- Winkel aus der Lage ----------

// Rechnet den Ellbogenwinkel (Grad) aus der relativen Drehung q.
// Die Knochenachse des Unterarms (knochen2) wird in die Achsen von IMU 1 gedreht.
// Der Winkel ist ihre Drehung um die Scharnier-Achse, gegenüber der Referenz-Lage (knochen0).
// false, wenn das nicht geht (Knochenachse zeigt in Richtung der Scharnier-Achse).
NICHT_INLINE static bool winkelAusLage(const float *q, const float *achse1, const float *knochen2,
                          const float *knochen0, float &winkelGrad) {
  float m[9];
  float w[3];
  float p[3];
  float c[3];
  qZuMatrix(q, m);
  matMalVektor(m, knochen2, w);
  float s = punkt3(w, achse1);
  for (int i = 0; i < 3; i++) {
    p[i] = w[i] - s * achse1[i];          // Anteil quer zur Scharnier-Achse
  }
  if (!(punkt3(p, p) > 0.01f)) {
    return false;
  }
  kreuz3(knochen0, p, c);
  float y = punkt3(achse1, c);
  float x = punkt3(knochen0, p);
  winkelGrad = atan2f(y, x) * RAD_ZU_GRAD;
  return true;
}


// ---------- laufende Messung ----------

// Schwerkraft zieht die relative Drehung q ein kleines Stück zurecht.
// a1n, a2n: Schwerkraft-Richtung (Länge 1) in den Achsen von IMU 1 und IMU 2.
// Stimmen beide nicht überein, wird q so gedreht, dass sie übereinstimmen.
// Bleibt der Fehler über lange Zeit auf einer Seite, ist der Gyro-Fehler (bias1, bias2) falsch.
// Dann wird er ganz langsam mitkorrigiert (je zur Hälfte auf die beiden IMUs verteilt).
// Weil sich der Arm in alle Lagen bewegt, wird so mit der Zeit der ganze Gyro-Fehler gefunden.
NICHT_INLINE static void schwerkraftKorrektur(EllbogenZustand &z, const float *a1n, const float *a2n, float dt) {
  float m[9];
  float a2in1[3];
  float e[3];
  qZuMatrix(z.q, m);
  matMalVektor(m, a2n, a2in1);
  kreuz3(a2in1, a1n, e);                  // Drehachse, Länge = Fehlerwinkel (Bogenmass)

  // Zweiter Fehler: Die Knochenachse des Unterarms muss quer zur Scharnier-Achse liegen
  // (der Ellbogen ist ein Scharnier). Liegt sie daneben, ist die Drehung verdreht.
  // Das sieht man auch dann, wenn die Schwerkraft nichts verrät (Drehung um die Senkrechte).
  float w[3];
  float u[3];
  float eh[3] = {0.0f, 0.0f, 0.0f};
  matMalVektor(m, z.knochen2, w);
  float s = punkt3(w, z.achse1);
  kreuz3(w, z.achse1, u);
  float u2 = punkt3(u, u);
  if (u2 > 0.25f) {
    for (int i = 0; i < 3; i++) {
      eh[i] = -s * u[i] / u2;             // Drehung, die die Knochenachse in die Scharnier-Ebene bringt
    }
  }

  float k = KORREKTUR_RATE * dt;
  float kh = KORREKTUR_RATE_SCHARNIER * dt;
  if (k > 1.0f) {
    k = 1.0f;
  }
  if (kh > 1.0f) {
    kh = 1.0f;
  }
  // Gyro-Fehler nachführen: b1 += c/2, b2 -= R^T c / 2 mit c = KI * (e + eh) (in Grad/s)
  float ki = BIAS_LERNRATE * dt * RAD_ZU_GRAD * 0.5f;
  float gesamt[3];
  for (int i = 0; i < 3; i++) {
    gesamt[i] = e[i] + eh[i];
  }
  for (int i = 0; i < 3; i++) {
    z.bias1[i] += ki * gesamt[i];
  }
  float rt[3];
  matMalVektor(m, gesamt, rt, true);      // R^T * gesamt
  for (int i = 0; i < 3; i++) {
    z.bias2[i] -= ki * rt[i];
  }
  float phi[3];
  for (int i = 0; i < 3; i++) {
    phi[i] = k * e[i] + kh * eh[i];
  }
  float dq[4];
  float tq[4];
  qAusDrehvektor(phi, dq);
  qMal(dq, z.q, tq);
  for (int i = 0; i < 4; i++) {
    z.q[i] = tq[i];
  }
  qNormieren(z.q);
}

// Versucht, die Lage nur aus der Schwerkraft zu finden (nach einem Ausfall).
// Geht davon aus, dass sich der Unterarm während dem Ausfall nicht um seine Achse gedreht hat
// (das Gerät hat in dieser Zeit keine Gyro-Daten). Dann gibt es genau einen Ellbogenwinkel,
// bei dem die Schwerkraft beider IMUs zusammenpasst.
// Gibt true zurück, wenn es einen Wert gibt: winkelGrad und qNeu (die passende Drehung).
NICHT_INLINE static bool lageAusSchwerkraft(const EllbogenZustand &z, const float *a1n, const float *a2n,
                               float &winkelGrad, float *qNeu) {
  // Schwerkraft von IMU 2 mit der gemerkten Verdrehung des Unterarms, in der Referenz-Lage der Drehung
  float a2dreh[3];
  float mRef[9];
  float c[3];
  float qd[4];
  float md[9];
  qAusAchse(z.knochen2, z.drehungLetzte, qd);
  qZuMatrix(qd, md);
  matMalVektor(md, a2n, a2dreh);
  qZuMatrix(z.qRef, mRef);
  matMalVektor(mRef, a2dreh, c);
  // Entlang der Scharnier-Achse muss die Schwerkraft gleich gross sein, sonst passt es nicht zusammen
  float cs = punkt3(c, z.achse1);
  float as = punkt3(a1n, z.achse1);
  if (fabsf(cs - as) > LAGE_MAX_ABWEICHUNG) {
    return false;
  }
  float cq[3];
  float aq[3];
  for (int i = 0; i < 3; i++) {
    cq[i] = c[i] - cs * z.achse1[i];
    aq[i] = a1n[i] - as * z.achse1[i];
  }
  // Quer zur Scharnier-Achse muss die Schwerkraft genug zu sehen sein (sonst ist der Winkel unbestimmt)
  if (!(punkt3(cq, cq) >= LAGE_MIN_AMPLITUDE * LAGE_MIN_AMPLITUDE) ||
      !(punkt3(aq, aq) >= LAGE_MIN_AMPLITUDE * LAGE_MIN_AMPLITUDE)) {
    return false;
  }
  float kr[3];
  kreuz3(cq, aq, kr);
  float theta = atan2f(punkt3(kr, z.achse1), punkt3(cq, aq)) * RAD_ZU_GRAD;
  float qa[4];
  float qb[4];
  float tq[4];
  qAusAchse(z.achse1, theta, qa);
  qAusAchse(z.knochen2, z.drehungLetzte, qb);
  qMal(qa, z.qRef, tq);
  qMal(tq, qb, qNeu);
  qNormieren(qNeu);
  winkelGrad = winkelKuerzen(theta);
  return true;
}

// Führt die Winkelmessung um einen Messzyklus nach (mit eingemessener Achse).
NICHT_INLINE static void laufendNachfuehren(EllbogenZustand &z, const float *a1, const float *a2,
                               const float *w1, const float *w2, float dt) {
  // Gyro-Werte ohne den bekannten Fehler (Offset): v in Grad/s, r in rad/s
  float r1[3];
  float r2[3];
  float v1[3];
  float v2[3];
  for (int i = 0; i < 3; i++) {
    v1[i] = w1[i] - z.bias1[i];
    v2[i] = w2[i] - z.bias2[i];
    r1[i] = v1[i] * DEG_ZU_RAD;
    r2[i] = v2[i] * DEG_ZU_RAD;
  }

  // Mittelwert aus letzter und jetziger Geschwindigkeit (genauer als nur der jetzige Wert)
  float m1[3];
  float m2[3];
  for (int i = 0; i < 3; i++) {
    m1[i] = z.ratenGueltig ? 0.5f * (z.rate1[i] + r1[i]) : r1[i];
    m2[i] = z.ratenGueltig ? 0.5f * (z.rate2[i] + r2[i]) : r2[i];
    z.rate1[i] = r1[i];
    z.rate2[i] = r2[i];
  }
  z.ratenGueltig = true;

  // Die Drehung mit beiden Gyros weiterführen
  relativDrehen(z.q, m1, m2, dt);

  // Schwerkraft prüfen und (wenn sie stimmt) die Drehung zurechtziehen
  float n1 = betrag3(a1);
  float n2 = betrag3(a2);
  bool schwerkraftOk = (n1 >= G_MIN && n1 <= G_MAX && n2 >= G_MIN && n2 <= G_MAX);
  float a1n[3] = {0.0f, 0.0f, 0.0f};
  float a2n[3] = {0.0f, 0.0f, 0.0f};
  if (schwerkraftOk) {
    for (int i = 0; i < 3; i++) {
      a1n[i] = a1[i] / n1;
      a2n[i] = a2[i] / n2;
    }
  }

  if (!z.lageBekannt) {
    // Lage wiederfinden (nach einem Ausfall): die Schwerkraft sagt uns den Winkel.
    // Beim ersten brauchbaren Wert wird die Drehung neu gesetzt. Danach läuft die normale Messung weiter
    // (Gyro und Schwerkraft), bis LAGE_MIN_ANZAHL Messungen in Folge dazu passen. So mittelt sich das Rauschen weg.
    z.lageZeit += dt;
    bool passt = false;
    if (schwerkraftOk) {
      float winkelS = 0.0f;
      float qNeu[4];
      if (lageAusSchwerkraft(z, a1n, a2n, winkelS, qNeu)) {
        float jetzt = 0.0f;
        bool hatWinkel = winkelAusLage(z.q, z.achse1, z.knochen2, z.knochen0, jetzt);
        if (z.lageAnzahl == 0 || !hatWinkel || fabsf(winkelKuerzen(winkelS - jetzt)) > 3.0f) {
          for (int i = 0; i < 4; i++) {
            z.q[i] = qNeu[i];             // Start (oder die Werte sind weggelaufen): neu setzen
          }
          z.lageAnzahl = 0;
        }
        passt = true;
      }
      schwerkraftKorrektur(z, a1n, a2n, dt);
    }
    if (passt) {
      if (z.lageAnzahl < 255) {
        z.lageAnzahl = z.lageAnzahl + 1;
      }
    } else {
      z.lageAnzahl = 0;
    }
    float w = 0.0f;
    bool ok = winkelAusLage(z.q, z.achse1, z.knochen2, z.knochen0, w);
    if (ok && (z.lageAnzahl >= LAGE_MIN_ANZAHL || z.lageZeit >= LAGE_MAX_ZEIT)) {
      // Gefunden (oder nichts Besseres da: dann gilt die mit den Gyros weitergeführte Drehung)
      z.winkel = w;
      z.lageBekannt = true;
      z.lageAnzahl = 0;
    }
    return;
  }

  if (schwerkraftOk) {
    schwerkraftKorrektur(z, a1n, a2n, dt);
  }

  float winkelNeu = 0.0f;
  if (!winkelAusLage(z.q, z.achse1, z.knochen2, z.knochen0, winkelNeu) ||
      isnan(winkelNeu) || isinf(winkelNeu)) {
    // Sicherheit: kein gültiger Winkel -> keine Warnung, bis die Lage wieder bekannt ist
    z.lageBekannt = false;
    z.lageZeit = 0.0f;
    z.lageAnzahl = 0;
    return;
  }
  z.winkel = winkelNeu;
}


// ---------- Einmessen: Phase 1, 2 und 3 ----------

// Phase 1 (Still): Mittelwerte sammeln (Schwerkraft-Richtung, Gyro-Fehler, Ruhe).
// Der Gyro-Fehler wird als laufender Mittelwert geführt. Ausreisser (z. B. ein Messwert, bei dem der
// Arm schon losläuft) zählen nicht mit, sonst wäre der Gyro-Fehler falsch und der Winkel driftet.
NICHT_INLINE static void phase1Sammeln(EllbogenZustand &z, const float *a1, const float *a2,
                          const float *w1, const float *w2) {
  const float *a[2] = {a1, a2};
  const float *w[2] = {w1, w2};
  float *summeA[2] = {z.summeA1, z.summeA2};
  float *mittelW[2] = {z.summeW1, z.summeW2};
  float *summeBetrag[2] = {&z.summeBetrag1, &z.summeBetrag2};
  bool nimmW = true;
  float n = (float)z.anzahlW + 1.0f;
  for (int u = 0; u < 2; u++) {
    float d[3];
    for (int i = 0; i < 3; i++) {
      d[i] = w[u][i] - mittelW[u][i];
    }
    if (z.anzahl1 > 0 && betrag3(d) >= BIAS_AUSREISSER) {
      nimmW = false;
    }
    *summeBetrag[u] += betrag3(w[u]);
    for (int i = 0; i < 3; i++) {
      summeA[u][i] += a[u][i];
    }
  }
  if (nimmW) {
    for (int u = 0; u < 2; u++) {
      for (int i = 0; i < 3; i++) {
        mittelW[u][i] += (w[u][i] - mittelW[u][i]) / n;   // laufender Mittelwert (beim ersten Wert: gleich w)
      }
    }
    z.anzahlW = z.anzahlW + 1;
  }
  if (z.anzahl1 < 65535) {
    z.anzahl1 = z.anzahl1 + 1;
  }
}

// Zählt eine Zeile p (n Zahlen, Gewicht w) in die Sammelmatrix (obere Hälfte, n x n) dazu.
NICHT_INLINE static void matrixZaehlen(float *mat, int n, const float *p, float w) {
  int index = 0;
  for (int a = 0; a < n; a++) {
    for (int b = a; b < n; b++) {
      mat[index] += w * p[a] * p[b];
      index++;
    }
  }
}

// Rechnet x^T * M * x für eine Matrix M, die nur mit ihrer oberen Hälfte gespeichert ist (n x n).
NICHT_INLINE static float formWert(const float *mat, int n, const float *x) {
  float summe = 0.0f;
  int index = 0;
  for (int a = 0; a < n; a++) {
    for (int b = a; b < n; b++) {
      float f = (a == b) ? 1.0f : 2.0f;
      summe += f * mat[index] * x[a] * x[b];
      index++;
    }
  }
  return summe;
}

// Merkt die Drehung beider IMUs seit Beginn von Phase 2 als Zwischenwert (für die Beuge-Prüfung am Ende).
NICHT_INLINE static void zwischenMerken(EllbogenZustand &z) {
  for (int i = 0; i < 8; i++) {
    z.zwischen[z.zwischenAnzahl][i] = (int16_t)floorf(z.lage[i / 4][i % 4] * ZWISCHEN_SKALA + 0.5f);
  }
  z.zwischenAnzahl = z.zwischenAnzahl + 1;
}

// Phase 2 (Bewegung) und Phase 3 (Drehung): Drehung beider IMUs mitzählen und Daten sammeln.
//   Phase 2: Zwischenwerte merken und Daten für die Achsen-Suche sammeln.
//   Phase 3: Die Drehung von IMU 2 (Hand drehen) zeigt in Richtung der Knochenachse des Unterarms.
// Die Zeile u = 0 gehört zu IMU 1 (Oberarm), u = 1 zu IMU 2 (Unterarm).
NICHT_INLINE static void phase2Sammeln(EllbogenZustand &z, const float *a1, const float *a2,
                          const float *w1, const float *w2, float dt) {
  if (z.grund != EINMESSEN_GRUND_KEINER) {
    return;                               // Phase 1 war schon schlecht: nichts mehr sammeln
  }
  const float *a[2] = {a1, a2};
  const float *w[2] = {w1, w2};
  const float *bias[2] = {z.summeW1, z.summeW2};   // Gyro-Fehler aus Phase 1
  float r[2][3];                          // Drehgeschwindigkeit ohne Gyro-Fehler in rad/s
  float g[2][3];                          // Schwerkraft-Richtung (Länge 1)
  float n[2];
  for (int u = 0; u < 2; u++) {
    float m[3];                           // Mittelwert aus letzter und jetziger Geschwindigkeit
    for (int i = 0; i < 3; i++) {
      r[u][i] = (w[u][i] - bias[u][i]) * DEG_ZU_RAD;
      m[i] = z.ratenGueltig2 ? 0.5f * (z.rateP[u][i] + r[u][i]) : r[u][i];
      z.rateP[u][i] = r[u][i];
    }
    relativDrehen(z.lage[u], nullVektor, m, dt);   // nur diese IMU weiterdrehen
    n[u] = betrag3(a[u]);
  }
  z.ratenGueltig2 = true;
  z.zeit2 += dt;

  if (z.phase == 3) {
    // Phase 3: Weg der beiden IMUs zählen. Dreht sich IMU 2 deutlich, zählt ihre Richtung
    // (immer auf die erste Richtung ausgerichtet, damit "hin" und "zurück" sich nicht aufheben).
    float n2 = betrag3(r[1]);
    z.dreh[6] += betrag3(r[0]) * dt;
    z.dreh[7] += n2 * dt;
    if (n2 > DREH_MIN_TEMPO) {
      if (!(betrag3(z.dreh) > 0.5f)) {
        for (int i = 0; i < 3; i++) {
          z.dreh[i] = r[1][i] / n2;
        }
      }
      float vorz = (punkt3(r[1], z.dreh) < 0.0f) ? -1.0f : 1.0f;
      for (int i = 0; i < 3; i++) {
        z.dreh[3 + i] += vorz * r[1][i];
      }
    }
    return;
  }

  // Alle 0,25 s einen Zwischenwert merken (für die Beuge-Prüfung am Ende)
  if (z.zwischenAnzahl < ELLBOGEN_ZWISCHENWERTE &&
      z.zeit2 >= (float)(z.zwischenAnzahl + 1) * ZWISCHEN_ABSTAND) {
    zwischenMerken(z);
  }

  // Schwerkraft-Richtungen beider IMUs (nur wenn der Betrag nahe 1 g ist)
  if (!(n[0] >= G_MIN && n[0] <= G_MAX && n[1] >= G_MIN && n[1] <= G_MAX)) {
    return;
  }
  for (int u = 0; u < 2; u++) {
    for (int i = 0; i < 3; i++) {
      g[u][i] = a[u][i] / n[u];
    }
  }
  // Wie weit neigt sich der Oberarm? (Schwerkraft-Richtung von IMU 1 gegenüber der Referenz, geglättet)
  for (int i = 0; i < 3; i++) {
    if (z.anzahlG == 0) {
      z.kippGlatt[i] = g[0][i];
    } else {
      z.kippGlatt[i] += KIPPEN_GLAETTUNG * (g[0][i] - z.kippGlatt[i]);
    }
  }
  float nk = betrag3(z.kippGlatt);
  if (nk > 0.5f) {
    float cw = punkt3(z.kippGlatt, z.summeA1) / nk;
    if (cw < z.kippCosMin) {
      z.kippCosMin = cw;                  // kleinster Cosinus = grösste Neigung
    }
  }

  // Die Scharnier-Achse j1 (in den Achsen von IMU 1) und j2 (in den Achsen von IMU 2) sind gesucht.
  // Beide sehen entlang der Scharnier-Achse gleich viel Schwerkraft (A) und gleich viel Änderung davon (B):
  //   A: g1.j1 - g2.j2 = 0            B: (w1 x g1).j1 - (w2 x g2).j2 = 0
  // Jede Messung ergibt zwei Zeilen p (6 Zahlen) mit p.x = 0 für x = (j1, j2).
  float pA[6];
  float pB[6];
  float tempo2Summe = 0.0f;               // (rad/s)^2
  for (int u = 0; u < 2; u++) {
    float vorz = (u == 0) ? 1.0f : -1.0f;
    float y[3];
    kreuz3(r[u], g[u], y);
    tempo2Summe += punkt3(r[u], r[u]);
    for (int i = 0; i < 3; i++) {
      pA[3 * u + i] = vorz * g[u][i];
      pB[3 * u + i] = vorz * y[i];
    }
  }
  float wB = 1.0f / (2.0e-4f * tempo2Summe + 1.0e-5f);
  matrixZaehlen(z.matrixM, 6, pA, GEWICHT_SCHWERKRAFT);
  matrixZaehlen(z.matrixM, 6, pB, wB);
  if (z.anzahlG < 65535) {
    z.anzahlG = z.anzahlG + 1;
  }
}

// Sucht den kleinsten und den zweitkleinsten Eigenwert der 6x6-Matrix (nur obere Hälfte gespeichert)
// mit der inversen Iteration. x1 = Eigenvektor zum kleinsten Wert (Länge 1).
// Der kleinste Wert (lam1) sagt, wie gut die Daten passen (0 = perfekt), der zweitkleinste (lam2),
// wie sicher die Achse bestimmt ist (gross = eindeutig). false, wenn die Rechnung nicht geht.
NICHT_INLINE static bool eigenKleinste(const float *mat, float *x1, float &lam1, float &lam2) {
  float a[6][6];
  int idx = 0;
  float spur = 0.0f;
  for (int i = 0; i < 6; i++) {
    for (int j = i; j < 6; j++) {
      a[i][j] = mat[idx];
      a[j][i] = mat[idx];
      idx++;
    }
    spur += a[i][i];
  }
  if (!(spur > 0.0f)) {
    return false;
  }
  for (int i = 0; i < 6; i++) {
    a[i][i] += 1.0e-6f * spur;            // verhindert eine unlösbare Matrix
  }
  // Matrix umkehren (Gauss-Jordan, in der Matrix selbst)
  for (int k = 0; k < 6; k++) {
    float piv = a[k][k];
    if (!(piv > 0.0f)) {
      return false;
    }
    a[k][k] = 1.0f;
    for (int j = 0; j < 6; j++) {
      a[k][j] /= piv;
    }
    for (int i = 0; i < 6; i++) {
      if (i != k) {
        float f = a[i][k];
        a[i][k] = 0.0f;
        for (int j = 0; j < 6; j++) {
          a[i][j] -= f * a[k][j];
        }
      }
    }
  }
  float x[6];
  for (int durchgang = 0; durchgang < 2; durchgang++) {
    for (int i = 0; i < 6; i++) {
      x[i] = 0.3f + 0.1f * (float)i;
    }
    for (int runde = 0; runde < 40; runde++) {
      float y[6];
      float n2 = 0.0f;
      for (int i = 0; i < 6; i++) {
        y[i] = 0.0f;
        for (int k = 0; k < 6; k++) {
          y[i] += a[i][k] * x[k];
        }
      }
      if (durchgang == 1) {               // zweiter Durchgang: den ersten Eigenvektor herausrechnen
        float s = 0.0f;
        for (int i = 0; i < 6; i++) {
          s += y[i] * x1[i];
        }
        for (int i = 0; i < 6; i++) {
          y[i] -= s * x1[i];
        }
      }
      for (int i = 0; i < 6; i++) {
        n2 += y[i] * y[i];
      }
      if (!(n2 > 0.0f) || isinf(n2)) {
        return false;
      }
      float f = 1.0f / sqrtf(n2);
      for (int i = 0; i < 6; i++) {
        x[i] = y[i] * f;
      }
    }
    if (durchgang == 0) {
      for (int i = 0; i < 6; i++) {
        x1[i] = x[i];
      }
      lam1 = formWert(mat, 6, x);
    } else {
      lam2 = formWert(mat, 6, x);
    }
  }
  return !(isnan(lam1) || isnan(lam2));
}


// ---------- Funktionen aus logik.h ----------

void ellbogenZuruecksetzen(EllbogenZustand &z) {
  // Jedes Byte auf 0 setzen: ergibt überall 0 bzw. false
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&z);
  for (size_t i = 0; i < sizeof(z); i++) {
    bytes[i] = 0;
  }
}

bool ellbogenAktualisieren(EllbogenZustand &z, const ImuMessung &oberarm,
                           const ImuMessung &unterarm, float dtSekunden) {
  // 1. Alles prüfen, bevor etwas verändert wird (fail-safe, B-2)
  if (!(dtSekunden > 0.0f && dtSekunden <= MAX_DT)) {   // false auch bei NaN
    return false;
  }
  float a1[3];
  float w1[3];
  float a2[3];
  float w2[3];
  messungZerlegen(oberarm, a1, w1);
  messungZerlegen(unterarm, a2, w2);
  for (int i = 0; i < 3; i++) {
    if (!wertGueltig(a1[i], MAX_BESCHLEUNIGUNG) || !wertGueltig(a2[i], MAX_BESCHLEUNIGUNG) ||
        !wertGueltig(w1[i], MAX_DREHGESCHWINDIGKEIT) ||
        !wertGueltig(w2[i], MAX_DREHGESCHWINDIGKEIT)) {
      return false;
    }
  }

  // 2. Daten fürs Einmessen sammeln (falls eines läuft). Das ändert nur die Sammelfächer,
  //    nie die laufende Messung mit der alten Referenz und Achse.
  if (z.phase == 1) {
    phase1Sammeln(z, a1, a2, w1, w2);
  } else if (z.phase == 2 || z.phase == 3) {
    phase2Sammeln(z, a1, a2, w1, w2, dtSekunden);
  }

  // 3. Laufende Messung mit der bisherigen Referenz und Achse (läuft immer weiter)
  if (z.achseBekannt) {
    laufendNachfuehren(z, a1, a2, w1, w2, dtSekunden);
  }
  return true;
}

void einmessenStart(EllbogenZustand &z) {
  z.phase = 1;
  z.grund = EINMESSEN_GRUND_KEINER;
  z.anzahl1 = 0;
  z.anzahlW = 0;
  z.anzahlG = 0;
  z.summeBetrag1 = 0.0f;
  z.summeBetrag2 = 0.0f;
  z.zeit2 = 0.0f;
  z.zwischenAnzahl = 0;
  z.kippCosMin = 1.0f;
  z.ratenGueltig2 = false;
  for (int i = 0; i < 3; i++) {
    z.summeA1[i] = 0.0f;
    z.summeA2[i] = 0.0f;
    z.summeW1[i] = 0.0f;
    z.summeW2[i] = 0.0f;
  }
  for (int i = 0; i < 21; i++) {
    z.matrixM[i] = 0.0f;
  }
  for (int i = 0; i < 8; i++) {
    z.dreh[i] = 0.0f;
  }
}

void einmessenBewegungStart(EllbogenZustand &z) {
  if (z.phase != 1) {
    return;                               // ohne Phase 1 gibt es keine Phase 2
  }
  z.phase = 2;
  if (z.anzahl1 < MIN_ANZAHL_PHASE1) {
    z.grund = EINMESSEN_GRUND_IMU;        // zu wenig Messwerte
    return;
  }
  float n = (float)z.anzahl1;
  // (b) War der Arm ruhig? Mittelwert vom Betrag der Drehgeschwindigkeit, für beide IMUs.
  if (z.summeBetrag1 / n > RUHIG_MAX_BETRAG || z.summeBetrag2 / n > RUHIG_MAX_BETRAG) {
    z.grund = EINMESSEN_GRUND_RUHIG;
    return;
  }
  // Mittelwert der Schwerkraft-Richtung der Referenz-Lage (summeW ist schon ein Mittelwert)
  for (int i = 0; i < 3; i++) {
    z.summeA1[i] /= n;
    z.summeA2[i] /= n;
  }
  if (!normieren3(z.summeA1) || !normieren3(z.summeA2)) {
    z.grund = EINMESSEN_GRUND_RUHIG;      // keine brauchbare Schwerkraft gemessen
    return;
  }
  for (int k = 0; k < 4; k++) {
    z.lage[0][k] = (k == 0) ? 1.0f : 0.0f;
    z.lage[1][k] = (k == 0) ? 1.0f : 0.0f;
  }
}

void einmessenDrehungStart(EllbogenZustand &z) {
  if (z.phase != 2) {
    return;                               // ohne Phase 2 gibt es keine Phase 3
  }
  if (z.grund == EINMESSEN_GRUND_KEINER && z.zwischenAnzahl < ELLBOGEN_ZWISCHENWERTE) {
    zwischenMerken(z);                    // letzter Wert von Phase 2 für die Beuge-Prüfung
  }
  z.phase = 3;
}

// Die Referenz-Drehung qRef: Sie bringt die Schwerkraft a2n und die Scharnier-Achse j2 (in den Achsen
// von IMU 2) auf die Schwerkraft a1n und die Scharnier-Achse j1 (in den Achsen von IMU 1).
// Zwei Schritte: erst a2n auf a1n drehen, dann um a1n so drehen, dass auch die Achsen übereinstimmen.
NICHT_INLINE static void referenzDrehung(const float *a1n, const float *a2n, const float *j1,
                                         const float *j2, float *qRef) {
  float ax[3];
  kreuz3(a2n, a1n, ax);
  float sn = betrag3(ax);
  float cs = punkt3(a2n, a1n);
  if (sn > 1.0e-4f) {
    for (int i = 0; i < 3; i++) {
      ax[i] /= sn;
    }
  } else {
    senkrecht3(a2n, ax);                  // (fast) parallel oder gegenläufig: irgendeine Achse quer dazu
  }
  float qa[4];
  qAusAchse(ax, atan2f(sn, cs) * RAD_ZU_GRAD, qa);
  float m[9];
  float u[3];
  qZuMatrix(qa, m);
  matMalVektor(m, j2, u);                 // j2 nach dem ersten Schritt
  float uq[3];
  float jq[3];
  float us = punkt3(u, a1n);
  float js = punkt3(j1, a1n);
  for (int i = 0; i < 3; i++) {
    uq[i] = u[i] - us * a1n[i];           // nur der Anteil quer zur Schwerkraft zählt
    jq[i] = j1[i] - js * a1n[i];
  }
  float kr[3];
  kreuz3(uq, jq, kr);
  float qb[4];
  qAusAchse(a1n, atan2f(punkt3(kr, a1n), punkt3(uq, jq)) * RAD_ZU_GRAD, qb);
  qMal(qb, qa, qRef);
  qNormieren(qRef);
}

// Die relative Drehung q = (IMU-1-Lage rückwärts) * qRef * (IMU-2-Lage). Die beiden Lagen sind
// die Drehungen der IMUs seit Beginn von Phase 2.
NICHT_INLINE static void paarDrehung(const float *lageA, const float *lageB, const float *qRef, float *q) {
  float qAu[4];
  float tq[4];
  qUmkehr(lageA, qAu);
  qMal(qAu, qRef, tq);
  qMal(tq, lageB, q);
  qNormieren(q);
}

bool einmessenAbschliessen(EllbogenZustand &z, float minBeugungGrad) {
  if (z.phase != 2 && z.phase != 3) {
    return false;                         // es läuft kein Einmessen (oder es wurde abgebrochen)
  }
  bool hatDrehung = (z.phase == 3);
  z.phase = 0;
  if (z.grund != EINMESSEN_GRUND_KEINER) {
    return false;                         // Phase 1 war schon schlecht
  }
  // Ohne genug Bewegungsdaten gibt es keine Beugung
  if (isnan(minBeugungGrad) || isinf(minBeugungGrad) || z.anzahlG < MIN_ANZAHL_ACHSE) {
    z.grund = EINMESSEN_GRUND_BEUGUNG;
    return false;
  }

  // 1. Knochenachse des Unterarms aus der Drehung der Hand in Phase 3 (e)
  float f2[3] = {z.summeA2[0], z.summeA2[1], z.summeA2[2]};   // ersatzweise die Schwerkraft (nur für die Beuge-Prüfung)
  bool handOk = false;
  if (hatDrehung && z.dreh[7] >= DREH_MIN_WEG && z.dreh[6] <= DREH_MAX_OBERARM * z.dreh[7]) {
    float h[3] = {z.dreh[3], z.dreh[4], z.dreh[5]};
    if (normieren3(h)) {
      for (int i = 0; i < 3; i++) {
        f2[i] = h[i];
      }
      handOk = true;
    }
  }
  if (handOk) {
    // Zusätzliche Bedingung: Die Scharnier-Achse j2 steht quer zur Knochenachse: j2.f2 = 0
    float pf[6] = {0.0f, 0.0f, 0.0f, f2[0], f2[1], f2[2]};
    matrixZaehlen(z.matrixM, 6, pf, GEWICHT_KNOCHEN);
  }

  // 2. Scharnier-Achsen beider IMUs suchen (kleinster Eigenvektor)
  float x[6];
  float lam1 = 0.0f;
  float lam2 = 0.0f;
  float j1[3];
  float j2[3];
  bool gefunden = eigenKleinste(z.matrixM, x, lam1, lam2);
  if (gefunden) {
    for (int i = 0; i < 3; i++) {
      j1[i] = x[i];
      j2[i] = x[3 + i];
    }
    gefunden = normieren3(j1) && normieren3(j2);
  }
  if (!gefunden) {
    senkrecht3(z.summeA1, j1);            // irgendeine Achse, damit die Beuge-Prüfung (a) trotzdem laufen kann
    senkrecht3(f2, j2);
  }

  // 3. Referenz-Drehung und Knochenachse der Referenz-Lage (in den Achsen von IMU 1)
  float qRef[4];
  referenzDrehung(z.summeA1, z.summeA2, j1, j2, qRef);
  float mRef[9];
  float w0[3];
  qZuMatrix(qRef, mRef);
  matMalVektor(mRef, f2, w0);

  // 4. Winkelverlauf in Phase 2 nachrechnen (Start = 0 Grad = Referenz): kleinster und grösster Winkel
  bool verlaufOk = true;
  float kleinster = 0.0f;
  float groesster = 0.0f;
  float qA[2][4];
  float qC[4];
  for (int k = 0; k < z.zwischenAnzahl; k++) {
    for (int i = 0; i < 8; i++) {
      qA[i / 4][i % 4] = (float)z.zwischen[k][i] / ZWISCHEN_SKALA;
    }
    paarDrehung(qA[0], qA[1], qRef, qC);
    float w = 0.0f;
    if (!winkelAusLage(qC, j1, f2, w0, w)) {
      verlaufOk = false;
    }
    if (w < kleinster) {
      kleinster = w;
    }
    if (w > groesster) {
      groesster = w;
    }
  }

  // 5. Vorzeichen: Die grössere Auslenkung von der Referenz ist die Beugung (negativ).
  bool umkehren = (groesster > -kleinster);
  float spannweite = groesster - kleinster;   // Spannweite = grösster minus kleinster Winkel

  // (a) Beugung gross genug?
  if (!verlaufOk || isnan(spannweite) || isinf(spannweite) || spannweite < minBeugungGrad) {
    z.grund = EINMESSEN_GRUND_BEUGUNG;
    return false;
  }

#ifdef LOGIK_DEBUG
  logikDbgAnstieg = lam2;
  logikDbgRival = lam1;
  fprintf(stderr, "  [achse] gefunden %d lam1 %.4g lam2 %.4g n %d kippcos %.5f handOk %d quer %.3f spann %.1f%c", (int)gefunden, lam1, lam2, (int)z.anzahlG, z.kippCosMin, (int)handOk, punkt3(j2, f2), spannweite, 10);
#endif
  // (d) Ist die Scharnier-Achse eindeutig? Sonst Abbruch: das Gerät rät nie.
  //     Dazu muss sich der Oberarm genug bewegt haben, die Daten müssen gut passen und die Achse muss klar bestimmt sein.
  if (!gefunden || z.kippCosMin > OBERARM_KIPP_COS || lam2 < ACHSE_LAMBDA_MIN ||
      lam1 > ACHSE_FEHLER_MAX * (float)z.anzahlG) {
    z.grund = EINMESSEN_GRUND_OBERARM;
    return false;
  }

  // (e) Ist die Knochenachse des Unterarms sicher bestimmt (Hand gedreht, Scharnier-Achse quer dazu)?
  if (!handOk || fabsf(punkt3(j2, f2)) > DREH_QUER_MAX) {
    z.grund = EINMESSEN_GRUND_HAND;
    return false;
  }

  // 6. Neues Modell übernehmen (erst jetzt, damit bei einem Abbruch alles beim Alten bleibt)
  float vorz = umkehren ? -1.0f : 1.0f;   // beide Achsen kippen zusammen: qRef bleibt gleich
  paarDrehung(z.lage[0], z.lage[1], qRef, qC);
  for (int i = 0; i < 3; i++) {
    z.achse1[i] = vorz * j1[i];
    z.knochen2[i] = f2[i];
    z.knochen0[i] = w0[i];
    z.bias1[i] = z.summeW1[i];
    z.bias2[i] = z.summeW2[i];
  }
  for (int i = 0; i < 4; i++) {
    z.qRef[i] = qRef[i];
    z.q[i] = qC[i];
  }
  float winkelEnde = 0.0f;
  if (!winkelAusLage(z.q, z.achse1, z.knochen2, z.knochen0, winkelEnde)) {
    winkelEnde = 0.0f;                    // (kann nicht vorkommen)
  }
  z.winkel = winkelEnde;
  z.achseBekannt = true;
  z.lageBekannt = true;
  z.ratenGueltig = false;
  z.lageZeit = 0.0f;
  z.lageAnzahl = 0;
  z.lageLetzter = 0.0f;
  z.drehungLetzte = 0.0f;
  z.grund = EINMESSEN_GRUND_KEINER;
  return true;
}

void einmessenAbbrechen(EllbogenZustand &z) {
  if (z.phase != 0) {
    z.grund = EINMESSEN_GRUND_IMU;        // IMU-Ausfall hat Vorrang vor allen anderen Gründen
  }
  z.phase = 0;                            // Referenz und Achse bleiben wie sie waren
}

uint8_t einmessenGrund(const EllbogenZustand &z) {
  return z.grund;
}

bool ellbogenAbweichung(const EllbogenZustand &z, float &abweichungGrad) {
  if (!z.achseBekannt || !z.lageBekannt) {
    return false;
  }
  if (isnan(z.winkel) || isinf(z.winkel)) {
    return false;
  }
  abweichungGrad = z.winkel;
  return true;
}

void ellbogenLageVerwerfen(EllbogenZustand &z) {
  if (z.achseBekannt && z.lageBekannt) {
    // Verdrehung des Unterarms um seine Achse merken (wird nach dem Ausfall als unverändert angenommen).
    // Es gilt: q = Drehung(Scharnier, Winkel) * qRef * Drehung(Knochenachse, Verdrehung)
    float qa[4];
    float t[4];
    float qm[4];
    float qru[4];
    qAusAchse(z.achse1, -z.winkel, qa);
    qMal(qa, z.q, t);
    qUmkehr(z.qRef, qru);
    qMal(qru, t, qm);
    qNormieren(qm);
    float s = qm[1] * z.knochen2[0] + qm[2] * z.knochen2[1] + qm[3] * z.knochen2[2];
    z.drehungLetzte = winkelKuerzen(2.0f * atan2f(s, qm[0]) * RAD_ZU_GRAD);
  }
  z.lageBekannt = false;
  z.lageZeit = 0.0f;
  z.lageAnzahl = 0;
  z.lageLetzter = 0.0f;
  z.ruheZeit = 0.0f;
  z.ratenGueltig = false;
}

