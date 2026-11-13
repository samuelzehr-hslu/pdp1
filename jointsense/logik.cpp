// logik.cpp
// Hier steht, WIE die Funktionen aus logik.h arbeiten.
// Keine Arduino-Befehle in dieser Datei (siehe Erklärung in logik.h).
// Die Erklärung der Ein- und Ausgaben jeder öffentlichen Funktion steht in logik.h.
//
// Aufbau dieser Datei:
//   1. Einfache Funktionen: Neigung, Grenzwert, Taste, Ausfall, Fehlermuster
//   2. Winkelmessung (REQ-002): Einstellungen, kleine Rechen-Helfer (Vektoren, Quaternionen),
//      laufende Messung
//   3. Einmessen (REQ-005): Schritt 1 still, Warte-Zustand, Schritt 2 frei bewegen, Achsen-Suche
//   4. Die Funktionen aus logik.h für Winkelmessung und Einmessen
//   5. Bestätigung beim Einmessen (REQ-004)

#include <math.h>
#include <stddef.h>   // für size_t
#include "logik.h"

// Die Hilfsfunktionen sollen nicht in jede Stelle hineinkopiert werden (spart Platz im Flash des Arduino).
#define NICHT_INLINE __attribute__((noinline))

// Die Kreiszahl Pi. Eigener Name, weil Arduino schon ein "PI" hat
// und wir diese Datei auch ohne Arduino benutzen wollen.
const double LOGIK_PI = 3.1415926535897932384626433832795;


// Neigung einer IMU in Grad aus der Beschleunigung (siehe logik.h).
float neigungAusBeschleunigung(float x, float y, float z) {
  float neigung = atan2(x, sqrt(y * y + z * z)) * 180.0 / LOGIK_PI;
  return neigung;
}


// Ellbogenwinkel = Neigung Unterarm - Neigung Oberarm (siehe logik.h).
float ellbogenWinkel(float neigungOberarm, float neigungUnterarm) {
  return neigungUnterarm - neigungOberarm;
}


// Mittelwert summe / anzahl, ohne durch 0 zu teilen (siehe logik.h).
float referenzBerechnen(float summe, int anzahl) {
  if (anzahl <= 0) {
    return 0.0;                       // nie durch 0 teilen
  }
  return summe / anzahl;
}


// Grenzwert mit Hysterese und Entprellung: true = Motor soll vibrieren (siehe logik.h).
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


// true, wenn die Taste gerade eben neu gedrückt wurde, entprellt (siehe logik.h).
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


// Zählt Fehlschläge einer IMU und merkt, ob sie ausgefallen ist (siehe logik.h).
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


// Fehlermuster bei Ausfall: 3 kurze Stösse alle 5 s (siehe logik.h).
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


// Entscheidet, ob der Motor vibrieren soll; Ausfall hat Vorrang (siehe logik.h).
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
static const float ZWISCHEN_SKALA       = 16000.0f; // gemerkte Lagen sind Quaternion-Zahlen mal diesen Wert
static const float BIAS_AUSREISSER     = 1.5f;    // Grad/s: Gyro-Werte in Schritt 1, die weiter vom Mittelwert weg sind, zählen nicht
static const float RUHIG_MAX_BETRAG     = 10.0f;   // Grad/s: so schnell darf sich jede IMU in Schritt 1 höchstens drehen (AK-3)
static const float STILL_ZEIT           = 1.999f;  // s: so lange am Stück ruhig = Schritt 1 fertig (2000 ms, AK-3)
static const uint16_t MIN_ANZAHL_ACHSE  = 8;       // so viele Messungen braucht die Achsen-Suche mindestens
// Achsen-Suche: angenommene Messfehler (nur für die Gewichtung der Messungen)
static const float GEWICHT_SCHWERKRAFT  = 3000.0f; // 1 / (Fehler der Schwerkraft-Bedingung) ^ 2
static const float GEWICHT_KNOCHEN      = 3000.0f; // so stark zählt: Scharnier-Achse steht quer zur Knochenachse des Unterarms
static const float ACHSE_LAMBDA_MIN     = 3000.0f; // so stark muss die schwächste Richtung der Achsen-Bestimmung belegt sein (sonst nicht eindeutig)
static const float ACHSE_FEHLER_MAX     = 30.0f;   // Rest-Fehler pro Messung darf höchstens so gross sein
static const float DREH_MIN_TEMPO       = 0.35f;   // rad/s (20 Grad/s): ab diesem Tempo zählt eine Handdrehung für die Richtung der Knochenachse
static const float DREH_MIN_WEG         = 0.70f;   // rad (40 Grad): so weit muss die Hand insgesamt gedreht werden (hin und zurück)
static const float DREH_MAX_OBERARM     = 0.5f;    // Handdrehung nur, wenn der Oberarm höchstens halb so schnell dreht wie der Unterarm
static const float DREH_QUER_MAX        = 0.5f;    // so viel (Cosinus) darf die Scharnier-Achse in Richtung der Knochenachse zeigen
// Einordnen in Schritt 2 (REQ-005): Der Unterarm hing in Schritt 1 höchstens 30 Grad schief.
static const float HAND_COS_MIN         = 0.8f;    // Handdrehung: Drehung höchstens ca. 37 Grad neben der Richtung aus Schritt 1
static const float ACHSE_DREH_ANTEIL    = 0.6f;    // nicht in die Achsen-Suche: mehr als 60 % der Drehung um die Knochenachse ...
static const float ACHSE_DREH_MIN       = 0.05f;   // ... und schneller als 0,05 rad/s (3 Grad/s) um die Knochenachse
static const int JACOBI_MAX_RUNDEN      = 2;       // so viele Jacobi-Durchgänge pro Sekunde höchstens (Rechenzeit, AK-17)
static const uint8_t POSEN_SCHAETZUNG   = 2;       // posen: Platz 0 und 1 = Extreme der Beugung (mit der Achse aus der Achsen-Suche),
                                                   // Platz 2 und 3 = Extreme der vorläufigen Beugung (nur aus dem Unterarm-Gyro)
static const uint8_t POSEN_RING_START   = 4;       // ab Platz 4 die neuen Lagen seit der letzten Auswertung
static const float BEUG_MAX_OBERARM     = 0.3f;    // vorläufige Beugung: Oberarm dreht höchstens 30 % so schnell wie der Unterarm ...
static const float BEUG_OBERARM_RUHIG   = 0.1f;    // ... und langsamer als 0,1 rad/s (6 Grad/s), sonst würde sie zu viel zählen
static const uint8_t POSEN_ABSTAND      = 5;       // jede 5. Messung (10 Mal pro s) wird die Lage gemerkt

// Nummern im Feld "phase" (REQ-005). 0 = kein Einmessen.
static const uint8_t PHASE_STILL        = 1;       // Schritt 1: still halten
static const uint8_t PHASE_WARTEN       = 2;       // Warte-Zustand: wartet auf die Bestätigung
static const uint8_t PHASE_FREI         = 3;       // Schritt 2: frei bewegen
static const float WARTEN_BIAS_LERNRATE = 0.05f;   // 1/s^2: so schnell wird der Gyro-Fehler im Warte-Zustand und in Schritt 2 nachgeführt

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

// Dreht die Drehung q zusätzlich um den Drehvektor phi (von links: q = Drehung(phi) * q).
// So zieht die Schwerkraft eine Drehung ein kleines Stück zurecht.
NICHT_INLINE static void qVorneDrehen(const float *phi, float *q) {
  float dq[4];
  float tq[4];
  qAusDrehvektor(phi, dq);
  qMal(dq, q, tq);
  for (int i = 0; i < 4; i++) {
    q[i] = tq[i];
  }
  qNormieren(q);
}

// Gibt den Wert zurück, aber höchstens 1 (für die Stärke einer Korrektur pro Messschritt).
NICHT_INLINE static float hoechstensEins(float wert) {
  if (wert > 1.0f) {
    wert = 1.0f;
  }
  return wert;
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

  float k = hoechstensEins(KORREKTUR_RATE * dt);
  float kh = hoechstensEins(KORREKTUR_RATE_SCHARNIER * dt);
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
  qVorneDrehen(phi, z.q);
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

// Lage wiederfinden (nach einem Ausfall): die Schwerkraft sagt uns den Winkel.
// Beim ersten brauchbaren Wert wird die Drehung neu gesetzt. Danach läuft die normale Messung weiter
// (Gyro und Schwerkraft), bis LAGE_MIN_ANZAHL Messungen in Folge dazu passen. So mittelt sich das Rauschen weg.
// Spätestens nach LAGE_MAX_ZEIT gilt die Lage wieder als bekannt (AK-13).
//   a1n, a2n     : Schwerkraft-Richtung (Länge 1) von IMU 1 und IMU 2, nur gültig mit schwerkraftOk
//   schwerkraftOk: true = bei beiden IMUs ist der Betrag der Beschleunigung nahe 1 g
static void lageWiederfinden(EllbogenZustand &z, const float *a1n, const float *a2n, bool schwerkraftOk,
                             float dt) {
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
          z.q[i] = qNeu[i];               // Start (oder die Werte sind weggelaufen): neu setzen
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
    lageWiederfinden(z, a1n, a2n, schwerkraftOk, dt);   // nach einem Ausfall
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


// ---------- Einmessen (REQ-005): Schritt 1 still, Warte-Zustand, Schritt 2 frei bewegen ----------

// Setzt n Zahlen ab x auf 0.
NICHT_INLINE static void zahlenNullen(float *x, int n) {
  for (int i = 0; i < n; i++) {
    x[i] = 0.0f;
  }
}

// Setzt die Lage beider IMUs auf "keine Drehung" (Quaternion 1, 0, 0, 0).
NICHT_INLINE static void lagenNullen(EllbogenZustand &z) {
  zahlenNullen(z.lage[0], 4);
  zahlenNullen(z.lage[1], 4);
  z.lage[0][0] = 1.0f;
  z.lage[1][0] = 1.0f;
}

// Setzt die Sammelfächer von Schritt 1 auf null (beim Start, oder wenn der Arm nicht ruhig war).
NICHT_INLINE static void stillZuruecksetzen(EllbogenZustand &z) {
  z.ruhigZeit = 0.0f;
  z.anzahl1 = 0;
  z.anzahlW = 0;
  zahlenNullen(z.summeA1, 3);
  zahlenNullen(z.summeA2, 3);
  zahlenNullen(z.summeW1, 3);
  zahlenNullen(z.summeW2, 3);
}

// Schritt 1 (still): Mittelwerte sammeln (Schwerkraft-Richtung und Gyro-Fehler).
// Der Gyro-Fehler wird als laufender Mittelwert geführt. Ausreisser (z. B. ein Messwert, bei dem der
// Arm schon losläuft) zählen nicht mit, sonst wäre der Gyro-Fehler falsch und der Winkel driftet.
NICHT_INLINE static void stillSammeln(EllbogenZustand &z, const float *a1, const float *a2,
                                      const float *w1, const float *w2) {
  float d1[3];                            // Abstand des Gyro-Werts vom bisherigen Mittelwert (IMU 1)
  float d2[3];                            // ... (IMU 2)
  for (int i = 0; i < 3; i++) {
    z.summeA1[i] += a1[i];
    z.summeA2[i] += a2[i];
    d1[i] = w1[i] - z.summeW1[i];
    d2[i] = w2[i] - z.summeW2[i];
  }
  bool nimmW = (z.anzahl1 == 0) || (betrag3(d1) < BIAS_AUSREISSER && betrag3(d2) < BIAS_AUSREISSER);
  if (nimmW) {
    float n = (float)z.anzahlW + 1.0f;
    for (int i = 0; i < 3; i++) {
      z.summeW1[i] += d1[i] / n;          // laufender Mittelwert (beim ersten Wert: gleich w)
      z.summeW2[i] += d2[i] / n;
    }
    z.anzahlW = z.anzahlW + 1;
  }
  if (z.anzahl1 < 65535) {
    z.anzahl1 = z.anzahl1 + 1;
  }
}

// Schritt 1 (still), ein Messzyklus (AK-3):
// Ruhig heisst: Bei BEIDEN IMUs ist der Betrag der Drehgeschwindigkeit höchstens 10 Grad/s.
// Ein unruhiger Zyklus setzt alles auf 0 zurück (kein Abbruch, einfach neu zählen).
// Nach 2 s am Stück ruhig: Referenz-Schwerkraft fertig, weiter im Warte-Zustand.
NICHT_INLINE static void stillSchritt(EllbogenZustand &z, const float *a1, const float *a2,
                                      const float *w1, const float *w2, float dt) {
  if (betrag3(w1) > RUHIG_MAX_BETRAG || betrag3(w2) > RUHIG_MAX_BETRAG) {
    stillZuruecksetzen(z);                // Wackeln: neu zählen
    return;
  }
  stillSammeln(z, a1, a2, w1, w2);
  z.ruhigZeit += dt;
  if (z.ruhigZeit < STILL_ZEIT) {
    return;                               // noch nicht lange genug ruhig
  }
  // Fertig: Richtung der Summe = mittlere Schwerkraft-Richtung = Referenz-Lage
  // (Teilen durch die Anzahl ist nicht nötig, normieren macht die Länge sowieso 1. summeW ist schon ein Mittelwert.)
  if (!normieren3(z.summeA1) || !normieren3(z.summeA2)) {
    stillZuruecksetzen(z);                // keine brauchbare Schwerkraft: neu zählen
    return;
  }
  lagenNullen(z);                         // ab jetzt zählt die Drehung jeder IMU seit der Referenz-Lage
  z.ratenGueltig2 = false;
  z.phase = PHASE_WARTEN;
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

// Dreht die Lage beider IMUs (lage[0] und lage[1]) mit den Gyros um einen Messschritt weiter.
// Der Gyro-Fehler (summeW1, summeW2) wird abgezogen. r bekommt die Drehgeschwindigkeiten in rad/s.
NICHT_INLINE static void lagenWeiterdrehen(EllbogenZustand &z, const float *w1, const float *w2,
                                           float dt, float r[2][3]) {
  const float *w[2] = {w1, w2};
  const float *bias[2] = {z.summeW1, z.summeW2};
  for (int u = 0; u < 2; u++) {
    float m[3];                           // Mittelwert aus letzter und jetziger Geschwindigkeit
    for (int i = 0; i < 3; i++) {
      r[u][i] = (w[u][i] - bias[u][i]) * DEG_ZU_RAD;
      m[i] = z.ratenGueltig2 ? 0.5f * (z.rateP[u][i] + r[u][i]) : r[u][i];
      z.rateP[u][i] = r[u][i];
    }
    relativDrehen(z.lage[u], nullVektor, m, dt);   // nur diese IMU weiterdrehen
  }
  z.ratenGueltig2 = true;
}

// Warte-Zustand und Schritt 2: Die Lage jeder IMU läuft mit ihrem Gyro weiter (seit der Referenz-Lage),
// und die Schwerkraft zieht jede IMU einzeln ein kleines Stück zurecht.
// In der Referenz-Lage zeigte die Schwerkraft in Richtung summeA1 bzw. summeA2.
// Warum: Ein Gyro-Fehler würde sich in bis zu 60 s zu mehreren Grad aufsummieren. Die Schwerkraft
// verhindert das für das Kippen. Nur die Drehung um die Senkrechte kann die Schwerkraft nicht sehen.
// r bekommt die Drehgeschwindigkeiten ohne Gyro-Fehler (rad/s), g die Schwerkraft-Richtungen (Länge 1).
// Rückgabe: true = bei beiden IMUs ist der Betrag der Beschleunigung nahe 1 g (g ist dann brauchbar).
NICHT_INLINE static bool lageStuetzen(EllbogenZustand &z, const float *a1, const float *a2,
                                      const float *w1, const float *w2, float dt,
                                      float r[2][3], float g[2][3]) {
  lagenWeiterdrehen(z, w1, w2, dt, r);
  float k = hoechstensEins(KORREKTUR_RATE * dt);
  const float *a[2] = {a1, a2};
  const float *soll[2] = {z.summeA1, z.summeA2};
  bool beideOk = true;
  for (int u = 0; u < 2; u++) {
    float n = betrag3(a[u]);
    if (!(n >= G_MIN && n <= G_MAX)) {
      beideOk = false;
      continue;                           // Arm bewegt sich zu wild: diesmal nur Gyro
    }
    for (int i = 0; i < 3; i++) {
      g[u][i] = a[u][i] / n;
    }
    // Gemessene Schwerkraft in die Achsen der Referenz-Lage drehen und mit dem Soll vergleichen
    float m[9];
    float gl[3];
    float e[3];
    qZuMatrix(z.lage[u], m);
    matMalVektor(m, g[u], gl);
    kreuz3(gl, soll[u], e);               // Drehachse, Länge = Fehlerwinkel (Bogenmass)
    // Gyro-Fehler nachführen: Bleibt die Schwerkraft immer auf derselben Seite daneben,
    // ist der Gyro-Fehler falsch und wird langsam korrigiert (in den Achsen der IMU).
    float rt[3];
    matMalVektor(m, e, rt, true);         // Fehler in die Achsen der IMU drehen
    float *bias = (u == 0) ? z.summeW1 : z.summeW2;
    float ki = WARTEN_BIAS_LERNRATE * dt * RAD_ZU_GRAD;
    for (int i = 0; i < 3; i++) {
      bias[i] -= ki * rt[i];
      e[i] *= k;
    }
    qVorneDrehen(e, z.lage[u]);
  }
  return beideOk;
}

// Merkt die Lage beider IMUs (je 4 Zahlen des Quaternions, mal 16000) im Platz "platz" von posen.
NICHT_INLINE static void lageMerken(EllbogenZustand &z, uint8_t platz) {
  for (int i = 0; i < 8; i++) {
    z.posen[platz][i] = (int16_t)floorf(z.lage[i / 4][i % 4] * ZWISCHEN_SKALA + 0.5f);
  }
}

// Holt die gemerkte Lage beider IMUs aus dem Platz "platz" zurück.
NICHT_INLINE static void lageHolen(const EllbogenZustand &z, uint8_t platz, float qA[2][4]) {
  for (int i = 0; i < 8; i++) {
    qA[i / 4][i % 4] = (float)z.posen[platz][i] / ZWISCHEN_SKALA;
  }
}

// Neigung des Oberarms: Die Achse der Oberarm-IMU, die in Schritt 1 nach unten zeigte (summeA1),
// wo zeigt sie jetzt hin? s[0] und s[1] = ihre zwei waagrechten Anteile (Sinus der Neigung in zwei
// Richtungen, z. B. vorn/hinten und seitlich), gerechnet in den Achsen der Referenz-Lage.
NICHT_INLINE static void oberarmNeigung(const EllbogenZustand &z, float *s) {
  float m[9];
  float d[3];
  float e1[3];
  float e2[3];
  qZuMatrix(z.lage[0], m);
  matMalVektor(m, z.summeA1, d);
  senkrecht3(z.summeA1, e1);              // erste waagrechte Richtung
  kreuz3(z.summeA1, e1, e2);              // zweite waagrechte Richtung (quer zur ersten)
  s[0] = punkt3(d, e1);
  s[1] = punkt3(d, e2);
}

// Zählt die Drehgeschwindigkeit r zur Summe einer Drehachse (summe[0..2]) dazu. Immer auf die bisherige
// Richtung ausgerichtet, damit "hin" und "zurück" sich nicht aufheben.
NICHT_INLINE static void achseSammeln(float *summe, const float *r) {
  float vorz = (punkt3(r, summe) < 0.0f) ? -1.0f : 1.0f;
  for (int i = 0; i < 3; i++) {
    summe[i] += vorz * r[i];
  }
}

// Merkt den kleinsten und grössten Wert. Rückgabe: 1 = neuer kleinster, 2 = neuer grösster, 0 = nichts Neues.
NICHT_INLINE static uint8_t grenzenMerken(float wert, float &kleinster, float &groesster) {
  if (wert < kleinster) {
    kleinster = wert;
    return 1;
  }
  if (wert > groesster) {
    groesster = wert;
    return 2;
  }
  return 0;
}

// Schritt 2: Sammelt aus einer Messung die Daten für die Suche der Scharnier-Achse (in matrixM).
// Die Scharnier-Achse j1 (in den Achsen von IMU 1) und j2 (in den Achsen von IMU 2) sind gesucht.
// Beide sehen entlang der Scharnier-Achse gleich viel Schwerkraft (A) und gleich viel Änderung davon (B):
//   A: g1.j1 - g2.j2 = 0            B: (w1 x g1).j1 - (w2 x g2).j2 = 0
// Jede Messung ergibt zwei Zeilen p (6 Zahlen) mit p.x = 0 für x = (j1, j2).
//   r: Drehgeschwindigkeit beider IMUs ohne Gyro-Fehler (rad/s), g: Schwerkraft-Richtung (Länge 1)
static void scharnierDatenSammeln(EllbogenZustand &z, float r[2][3], float g[2][3]) {
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

// Schritt 2 (frei bewegen), ein Messzyklus:
//   1. Lage beider IMUs weiterführen (von der Schwerkraft gestützt)
//   2. Neigung des Oberarms: kleinster und grösster Wert je Richtung (für den Oberarm-Wert, AK-8)
//   3. Alle 5 Messungen (10 Mal pro s) die Lage merken (für die Beugung, wird jede Sekunde ausgewertet)
//   4. Messung einordnen: Handdrehung oder nicht (Physik-Annahme "Einordnen" in REQ-005)
//   5. Vorläufige Beugung, wenn sich nur der Unterarm dreht (AK-8, ohne die ganze Achsen-Suche)
//   6. Wenn es keine Handdrehung ist: Daten für die Suche der Scharnier-Achse sammeln
NICHT_INLINE static void freiSammeln(EllbogenZustand &z, const float *a1, const float *a2,
                                     const float *w1, const float *w2, float dt) {
  float r[2][3];                          // Drehgeschwindigkeit ohne Gyro-Fehler in rad/s
  float g[2][3];                          // Schwerkraft-Richtung (Länge 1)
  bool schwerkraftOk = lageStuetzen(z, a1, a2, w1, w2, dt, r, g);

  // 2. Oberarm
  float s[2];
  oberarmNeigung(z, s);
  for (int k = 0; k < 2; k++) {
    grenzenMerken(s[k], z.oberMin[k], z.oberMax[k]);
  }

  // 3. Lage merken
  z.zyklen = z.zyklen + 1;
  if (z.zyklen >= POSEN_ABSTAND) {
    z.zyklen = 0;
    if (z.posenAnzahl < ELLBOGEN_POSEN) {
      lageMerken(z, z.posenAnzahl);
      z.posenAnzahl = z.posenAnzahl + 1;
    }
  }

  // 4. Einordnen. summeA2 = Schwerkraft der Unterarm-IMU in Schritt 1. Der Unterarm hing ungefähr
  //    senkrecht, also zeigt summeA2 ungefähr entlang der Knochenachse (nur zum Einordnen benutzt).
  //    Handdrehung = die Unterarm-IMU dreht sich vor allem um diese Richtung, der Oberarm ist ruhig.
  float n1 = betrag3(r[0]);
  float n2 = betrag3(r[1]);
  float c = punkt3(r[1], z.summeA2);      // Anteil der Drehung um die (ungefähre) Knochenachse
  bool handDreht = (n2 > 0.0f) && (n1 <= DREH_MAX_OBERARM * n2) && (fabsf(c) >= HAND_COS_MIN * n2);
  if (handDreht) {
    z.dreh[6] += c * dt;                  // Drehung der Hand mit Vorzeichen (hin und zurück heben sich auf)
    float h = fabsf(z.dreh[6]) * RAD_ZU_GRAD;
    if (h > z.handMax) {
      z.handMax = h;                      // Scheitelwert, wird nie kleiner (AK-8)
    }
    if (n2 > DREH_MIN_TEMPO) {
      achseSammeln(&z.dreh[3], r[1]);     // Richtung der Knochenachse sammeln
      z.dreh[7] += n2 * dt;               // Weg der Hand (hin und zurück zählen beide)
    }
  }
  // Dreht sich der Unterarm deutlich um die Knochenachse (auch gleichzeitig mit Beugen),
  // passt die Messung nicht zur Achsen-Suche (dort darf es nur EINE Drehachse geben).
  bool drehtUmKnochen = (fabsf(c) > ACHSE_DREH_ANTEIL * n2 && fabsf(c) > ACHSE_DREH_MIN);

  // 5. Vorläufige Beugung: Ist der Oberarm fast ruhig und dreht die Hand nicht, dreht sich der Unterarm
  //    nur um die Scharnier-Achse. Ihre Richtung in der Unterarm-IMU (dreh[0..2]) und der Winkel
  //    (theta[0]) kommen direkt aus dem Gyro. So steht die Beugung im Fortschritt, auch wenn Oberarm
  //    und Hand noch nicht bewegt wurden. Die Lagen beim kleinsten und grössten Winkel werden gemerkt
  //    (Platz 2 und 3), damit die Auswertung sie später mit der eindeutigen Achse nachrechnen kann.
  //    Bewegt sich der Oberarm (z. B. Kreis), zählt sie nicht: Dann käme seine Drehung dazu, und der Wert
  //    wäre zu gross. Diese Beugung zählt dann erst über die Auswertung mit der eindeutigen Achse.
  if (!drehtUmKnochen && n1 <= BEUG_MAX_OBERARM * n2 && n1 < BEUG_OBERARM_RUHIG) {
    if (n2 > DREH_MIN_TEMPO) {
      achseSammeln(z.dreh, r[1]);
    }
    float u[3] = {z.dreh[0], z.dreh[1], z.dreh[2]};
    if (normieren3(u)) {
      z.theta[0] += punkt3(r[1], u) * dt;
      uint8_t neu = grenzenMerken(z.theta[0], z.theta[1], z.theta[2]);
      if (neu > 0) {
        lageMerken(z, POSEN_SCHAETZUNG + neu - 1);   // Platz 2 = kleinster, Platz 3 = grösster Winkel
      }
      float spanne = (z.theta[2] - z.theta[1]) * RAD_ZU_GRAD;
      if (spanne > z.beugBest) {
        z.beugBest = spanne;              // wird nie kleiner (AK-8)
      }
    }
  }
  if (drehtUmKnochen) {
    return;
  }

  // 6. Nur wenn die Schwerkraft bei beiden IMUs brauchbar ist (Betrag nahe 1 g)
  if (!schwerkraftOk) {
    return;
  }
  scharnierDatenSammeln(z, r, g);
}

// Dreht zwei Reihen x und y (je 6 Zahlen im Abstand "schritt": 1 = Zeile, 6 = Spalte einer 6x6-Matrix)
// um einen Winkel mit Cosinus c und Sinus s. Hilfe für das Jacobi-Verfahren (spart Flash).
NICHT_INLINE static void reiheDrehen(float *x, float *y, int schritt, float c, float s) {
  for (int k = 0; k < 6; k++) {
    float xk = *x;
    float yk = *y;
    *x = c * xk - s * yk;
    *y = s * xk + c * yk;
    x += schritt;                         // zur nächsten Zahl der Reihe
    y += schritt;
  }
}

// Jacobi-Verfahren: Findet die Eigenwerte und Eigenvektoren einer symmetrischen 6x6-Matrix.
// Es wird NICHTS invertiert (genau und stabil, auch wenn ein Eigenwert nahe 0 ist).
// Neu (REQ-005): Es rechnet ab den Eigenvektoren der letzten Sekunde weiter. Die Matrix kommt darum
// schon in dieser Basis herein und ist fast fertig: meist reichen 1 bis 2 Durchgänge (Rechenzeit, AK-17).
//   a        : die Matrix in der Basis von v (wird verändert; am Ende stehen die Eigenwerte auf der Diagonale)
//   v        : die gemerkten Eigenvektoren als Spalten (werden mitgedreht)
//   maxRunden: höchstens so viele Durchgänge
// Rückgabe: true = fertig gerechnet (Werte neben der Diagonale vernachlässigbar klein).
NICHT_INLINE static bool jacobiEigen(float a[6][6], float v[6][6], int maxRunden) {
  for (int runde = 0; ; runde++) {
    // Wie gross sind die Werte neben der Diagonale noch? Klein genug = fertig.
    float aussen = 0.0f;
    float spur = 0.0f;
    for (int p = 0; p < 6; p++) {
      spur += fabsf(a[p][p]);
      for (int q = p + 1; q < 6; q++) {
        aussen += a[p][q] * a[p][q];
      }
    }
    if (!(aussen < 1.0e30f && spur < 1.0e30f)) {
      return false;                       // ungültige Zahlen (NaN oder unendlich): nicht fertig
    }
    if (!(aussen > 1.0e-12f * spur * spur)) {
      return true;                        // fertig
    }
    if (runde >= maxRunden) {
      return false;                       // nächste Sekunde geht es von hier aus weiter
    }
    // Jedes Paar (p, q) der Reihe nach so drehen, dass der Wert neben der Diagonale null wird
    for (int p = 0; p < 5; p++) {
      for (int q = p + 1; q < 6; q++) {
        if (!(fabsf(a[p][q]) > 1.0e-7f * (fabsf(a[p][p]) + fabsf(a[q][q])))) {
          continue;                       // schon klein genug (spart Rechenzeit)
        }
        float theta = (a[q][q] - a[p][p]) / (2.0f * a[p][q]);
        float t = 1.0f / (fabsf(theta) + sqrtf(theta * theta + 1.0f));
        if (theta < 0.0f) {
          t = -t;
        }
        float c = 1.0f / sqrtf(t * t + 1.0f);   // Cosinus des Drehwinkels
        float s = t * c;                         // Sinus des Drehwinkels
        reiheDrehen(&a[0][p], &a[0][q], 6, c, s);   // Spalten p und q drehen
        reiheDrehen(a[p], a[q], 1, c, s);           // Zeilen p und q drehen (die Matrix bleibt symmetrisch)
        reiheDrehen(&v[0][p], &v[0][q], 6, c, s);   // die Eigenvektoren mitdrehen
      }
    }
  }
}

// Setzt die gemerkten Eigenvektoren auf die Einheitsmatrix (Start von Schritt 2 oder nach einem Rechenfehler).
NICHT_INLINE static void eigenVNullen(EllbogenZustand &z) {
  // Alle 36 Zahlen auf 0 (sie liegen im Speicher direkt hintereinander, wie bei ellbogenZuruecksetzen),
  // dann die Diagonale auf 1.
  zahlenNullen(reinterpret_cast<float *>(z.eigenV), 36);
  for (int i = 0; i < 6; i++) {
    z.eigenV[i][i] = 1.0f;
  }
}

// Ein Wert (Zeile i, Spalte j) der Sammelmatrix (gespeichert ist nur die obere Hälfte).
// Mit handOk kommt die Bedingung "Scharnier-Achse j2 steht quer zur Knochenachse f2" dazu.
NICHT_INLINE static float matrixWert(const EllbogenZustand &z, int i, int j, const float *f2, bool handOk) {
  if (i > j) {
    int t = i;
    i = j;
    j = t;
  }
  int index = 6 * i - (i * (i - 1)) / 2 + (j - i);   // Platz in der oberen Hälfte (Zeile für Zeile)
  float wert = z.matrixM[index];
  if (handOk && i >= 3) {
    wert += GEWICHT_KNOCHEN * f2[i - 3] * f2[j - 3];
  }
  return wert;
}

// Rechnet eine 6x6-Matrix a mit den Eigenvektoren v um, ohne zweite Matrix (nur eine Reihe als Zwischenspeicher):
//   vonLinks = false: a = a * v   (Zeile für Zeile)
//   vonLinks = true : a = v^T * a (Spalte für Spalte)
NICHT_INLINE static void matrixBasis(float a[6][6], const float v[6][6], bool vonLinks) {
  float reihe[6];
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++) {
      reihe[j] = 0.0f;
      for (int k = 0; k < 6; k++) {
        // vonLinks: Spalte i von a, sonst Zeile i von a
        reihe[j] += v[k][j] * (vonLinks ? a[k][i] : a[i][k]);
      }
    }
    for (int j = 0; j < 6; j++) {
      float *ziel = vonLinks ? &a[j][i] : &a[i][j];   // Spalte i bzw. Zeile i ersetzen
      *ziel = reihe[j];
    }
  }
}

// Sucht die Scharnier-Achse: den Eigenvektor zum kleinsten Eigenwert der Sammelmatrix.
// Rechnet ab den Eigenvektoren der letzten Sekunde weiter (z.eigenV).
//   x    : Ausgabe, die Achse (j1 in x[0..2], j2 in x[3..5])
//   lam1 : kleinster Eigenwert (0 = die Daten passen perfekt)
//   lam2 : zweitkleinster Eigenwert (gross = die Achse ist eindeutig)
//   fertig: true = fertig gerechnet (nur dann darf das Einmessen abschliessen)
// Rückgabe: false, wenn die Rechnung nicht geht (NaN, unendlich).
NICHT_INLINE static bool achseSuchen(EllbogenZustand &z, const float *f2, bool handOk, float *x,
                                     float &lam1, float &lam2, bool &fertig) {
  // a = V^T * M * V (die Matrix in der Basis der gemerkten Eigenvektoren)
  float a[6][6];
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++) {
      a[i][j] = matrixWert(z, i, j, f2, handOk);
    }
  }
  matrixBasis(a, z.eigenV, false);        // a = M * V
  matrixBasis(a, z.eigenV, true);         // a = V^T * (M * V)
  fertig = jacobiEigen(a, z.eigenV, JACOBI_MAX_RUNDEN);
  // Den kleinsten und den zweitkleinsten Eigenwert (auf der Diagonale) suchen
  int k1 = 0;
  for (int i = 1; i < 6; i++) {
    if (a[i][i] < a[k1][k1]) {
      k1 = i;
    }
  }
  int k2 = (k1 == 0) ? 1 : 0;
  for (int i = 0; i < 6; i++) {
    if (i != k1 && a[i][i] < a[k2][k2]) {
      k2 = i;
    }
  }
  lam1 = a[k1][k1];
  lam2 = a[k2][k2];
  for (int i = 0; i < 6; i++) {
    x[i] = z.eigenV[i][k1];               // (NaN in x fängt später normieren3 ab)
  }
  if (!(fabsf(lam1) < 1.0e30f && fabsf(lam2) < 1.0e30f)) {   // NaN oder unendlich
    eigenVNullen(z);                      // Rechenfehler: nächste Sekunde von vorne rechnen
    fertig = false;
    return false;
  }
  return true;
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

// Die relative Drehung q = (IMU-1-Lage rückwärts) * qRef * (IMU-2-Lage).
// Die beiden Lagen sind die Drehungen der IMUs seit der Referenz-Lage (Schritt 1).
NICHT_INLINE static void paarDrehung(const float *lageA, const float *lageB, const float *qRef, float *q) {
  float qAu[4];
  float tq[4];
  qUmkehr(lageA, qAu);
  qMal(qAu, qRef, tq);
  qMal(tq, lageB, q);
  qNormieren(q);
}

// Ergebnis der Achsen-Rechnung beim Einmessen (nur kurz auf dem Stapel, nicht im Zustand).
struct EinmessModell {
  float j1[3];        // Scharnier-Achse in den Achsen von IMU 1 (Länge 1)
  float j2[3];        // Scharnier-Achse in den Achsen von IMU 2 (Länge 1)
  float f2[3];        // Knochenachse des Unterarms in den Achsen von IMU 2 (vorläufig: Ersatz quer zu j2)
  float w0[3];        // Knochenachse in der Referenz-Lage, in den Achsen von IMU 1
  float qRef[4];      // Referenz-Drehung
};

// Baut aus der Achse x (und der Knochenachse f2, wenn handOk) das (vorläufige) Modell.
// Ohne Handdrehung wird als Knochenachse ersatzweise die Schwerkraft aus Schritt 1 quer zu j2 genommen
// (reicht für den Beuge-Wert, für das Abschliessen braucht es die echte Handdrehung).
// false, wenn eine Achse nicht brauchbar ist.
NICHT_INLINE static bool modellBauen(const EllbogenZustand &z, const float *x, const float *f2, bool handOk,
                                     EinmessModell &m) {
  for (int i = 0; i < 3; i++) {
    m.j1[i] = x[i];
    m.j2[i] = x[3 + i];
  }
  if (!normieren3(m.j1) || !normieren3(m.j2)) {
    return false;
  }
  float s = punkt3(z.summeA2, m.j2);
  for (int i = 0; i < 3; i++) {
    m.f2[i] = handOk ? f2[i] : z.summeA2[i] - s * m.j2[i];
  }
  if (!normieren3(m.f2)) {
    senkrecht3(m.j2, m.f2);
  }
  referenzDrehung(z.summeA1, z.summeA2, m.j1, m.j2, m.qRef);
  float mRef[9];
  qZuMatrix(m.qRef, mRef);
  matMalVektor(mRef, m.f2, m.w0);
  return true;
}

// Kopiert eine gemerkte Lage (8 Zahlen) von "von" nach "nach".
NICHT_INLINE static void poseKopieren(const int16_t *von, int16_t *nach) {
  for (int i = 0; i < 8; i++) {
    nach[i] = von[i];
  }
}

// Beuge-Wert (AK-8): Rechnet mit dem jetzigen (vorläufigen) Modell den Ellbogenwinkel
//   - der beiden gemerkten Extreme (Platz 0 = kleinster, Platz 1 = grösster Winkel), jedes Mal neu,
//     damit sie immer zur neuesten Achse passen, und
//   - der beiden Extreme der vorläufigen Beugung (Platz 2 und 3), und
//   - aller neuen Lagen seit der letzten Auswertung (ab Platz POSEN_RING_START).
// Merkt die neuen Extreme (beugMin, beugMax). In beugBest zählen sie erst, wenn die Scharnier-Achse
// eindeutig ist (siehe einmessenAbschliessen).
NICHT_INLINE static void beugungPruefen(EllbogenZustand &z, const EinmessModell &m) {
  int16_t poseMin[8];
  int16_t poseMax[8];
  float wMin = 0.0f;
  float wMax = 0.0f;
  bool leer = true;
  uint8_t start = z.beugGueltig ? 0 : POSEN_SCHAETZUNG;   // Platz 2 und 3 sind in Schritt 2 immer gültig
  for (uint8_t p = start; p < z.posenAnzahl; p++) {
    float qA[2][4];
    float qC[4];
    float w = 0.0f;
    lageHolen(z, p, qA);
    paarDrehung(qA[0], qA[1], m.qRef, qC);
    if (!winkelAusLage(qC, m.j1, m.f2, m.w0, w)) {
      continue;
    }
    if (leer || w < wMin) {
      wMin = w;
      poseKopieren(z.posen[p], poseMin);
    }
    if (leer || w > wMax) {
      wMax = w;
      poseKopieren(z.posen[p], poseMax);
    }
    leer = false;
  }
  z.posenAnzahl = POSEN_RING_START;       // die neuen Lagen sind ausgewertet
  if (leer) {
    return;
  }
  poseKopieren(poseMin, z.posen[0]);
  poseKopieren(poseMax, z.posen[1]);
  z.beugGueltig = true;
  z.beugMin = wMin;
  z.beugMax = wMax;
}

// Oberarm-Wert (AK-8): Spannweite der Neigung je Richtung, der kleinere der beiden Werte.
NICHT_INLINE static float oberarmGrad(const EllbogenZustand &z) {
  // (Liegt ein Sinus durch Rundung knapp über 1, gibt asin NaN: dann ist der Wert nie OK, also sicher.)
  float g0 = (asinf(z.oberMax[0]) - asinf(z.oberMin[0])) * RAD_ZU_GRAD;
  float g1 = (asinf(z.oberMax[1]) - asinf(z.oberMin[1])) * RAD_ZU_GRAD;
  return (g0 < g1) ? g0 : g1;
}


// Knochenachse des Unterarms aus der Handdrehung (Summe der Drehachse in dreh[3..5]).
// Nur wenn die Hand weit genug gedreht wurde (DREH_MIN_WEG). f2 bekommt die Achse (Länge 1).
// Rückgabe: true = f2 ist brauchbar.
static bool knochenachseAusHand(const EllbogenZustand &z, float *f2) {
  bool handOk = false;
  if (z.dreh[7] >= DREH_MIN_WEG) {
    for (int i = 0; i < 3; i++) {
      f2[i] = z.dreh[3 + i];
    }
    handOk = normieren3(f2);
  }
  return handOk;
}

// Wird nach einem erfolgreichen Einmessen aufgerufen: übernimmt das neue Modell m als Referenz und Achse,
// setzt die jetzige Lage und beendet das Einmessen.
// Vorzeichen: Die grössere Auslenkung von der Referenz ist die Beugung (soll negativ sein).
static void modellUebernehmen(EllbogenZustand &z, const EinmessModell &m) {
  bool umkehren = (z.beugMax > -z.beugMin);
  float vorz = umkehren ? -1.0f : 1.0f;   // beide Achsen kippen zusammen: qRef bleibt gleich
  float qC[4];
  paarDrehung(z.lage[0], z.lage[1], m.qRef, qC);   // jetzige Lage, seit der Referenz-Lage weitergeführt
  for (int i = 0; i < 3; i++) {
    z.achse1[i] = vorz * m.j1[i];
    z.knochen2[i] = m.f2[i];
    z.knochen0[i] = m.w0[i];
    z.bias1[i] = z.summeW1[i];
    z.bias2[i] = z.summeW2[i];
  }
  for (int i = 0; i < 4; i++) {
    z.qRef[i] = m.qRef[i];
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
  z.phase = 0;                            // Einmessen beendet
}


// ---------- Funktionen aus logik.h ----------

// Löscht alles: keine Referenz, keine Achse, keine Lage (siehe logik.h).
void ellbogenZuruecksetzen(EllbogenZustand &z) {
  // Jedes Byte auf 0 setzen: ergibt überall 0 bzw. false
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&z);
  for (size_t i = 0; i < sizeof(z); i++) {
    bytes[i] = 0;
  }
}

// Verarbeitet EINEN Messzyklus: prüfen, beim Einmessen sammeln, Winkel nachführen (siehe logik.h).
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
  if (z.phase == PHASE_STILL) {
    stillSchritt(z, a1, a2, w1, w2, dtSekunden);
  } else if (z.phase == PHASE_WARTEN) {
    float r[2][3];
    float g[2][3];
    lageStuetzen(z, a1, a2, w1, w2, dtSekunden, r, g);   // nur die Lage weiterführen, nichts sammeln
  } else if (z.phase == PHASE_FREI) {
    freiSammeln(z, a1, a2, w1, w2, dtSekunden);
  }

  // 3. Laufende Messung mit der bisherigen Referenz und Achse (läuft immer weiter)
  if (z.achseBekannt) {
    laufendNachfuehren(z, a1, a2, w1, w2, dtSekunden);
  }
  return true;
}

// Beginnt Schritt 1 (still) des Einmessens (siehe logik.h).
void einmessenStart(EllbogenZustand &z) {
  z.phase = PHASE_STILL;
  z.grund = EINMESSEN_GRUND_KEINER;
  stillZuruecksetzen(z);
}

// true, sobald Schritt 1 fertig ist (Warte-Zustand oder schon Schritt 2) (siehe logik.h).
bool einmessenStillFertig(const EllbogenZustand &z) {
  return z.phase == PHASE_WARTEN || z.phase == PHASE_FREI;
}

// Beginnt Schritt 2 (frei bewegen) nach der Bestätigung (siehe logik.h).
void einmessenBewegungStart(EllbogenZustand &z) {
  if (z.phase != PHASE_WARTEN) {
    return;                               // ohne fertigen Schritt 1 gibt es keinen Schritt 2
  }
  // Die Lage der IMUs läuft weiter (NICHT auf null setzen): Der Arm muss bei der Bestätigung
  // nicht in der Referenz-Lage sein.
  z.phase = PHASE_FREI;
  z.anzahlG = 0;
  z.zyklen = 0;
  z.posenAnzahl = POSEN_RING_START;
  z.beugGueltig = false;
  z.beugMin = 0.0f;
  z.beugMax = 0.0f;
  z.beugBest = 0.0f;
  z.handMax = 0.0f;
  z.achseProzent = 0;
  zahlenNullen(z.matrixM, 21);
  zahlenNullen(z.dreh, 8);
  zahlenNullen(z.theta, 3);
  lageMerken(z, POSEN_SCHAETZUNG);        // Start der vorläufigen Beugung: Winkel 0
  lageMerken(z, POSEN_SCHAETZUNG + 1);
  eigenVNullen(z);
  // Ausgangslage des Oberarms = Lage zu Beginn von Schritt 2 (AK-7)
  float s[2];
  oberarmNeigung(z, s);
  for (int k = 0; k < 2; k++) {
    z.oberMin[k] = s[k];
    z.oberMax[k] = s[k];
  }
}

// Füllt f mit den Zahlen für die Fortschritts-Anzeige (siehe logik.h).
void einmessenFortschritt(const EllbogenZustand &z, EinmessFortschritt &f) {
  f.ruhigSekunden = 0.0f;
  f.beugungGrad = 0.0f;
  f.oberarmGrad = 0.0f;
  f.handGrad = 0.0f;
  f.achseProzent = 0;
  if (z.phase == PHASE_STILL) {
    f.ruhigSekunden = z.ruhigZeit;
  } else if (z.phase == PHASE_FREI) {
    f.beugungGrad = z.beugBest;
    f.oberarmGrad = oberarmGrad(z);
    f.handGrad = z.handMax;
    f.achseProzent = z.achseProzent;
  }
}

// Der erste noch nicht erfüllte Wert in Schritt 2, oder EINMESSEN_GRUND_KEINER (siehe logik.h).
uint8_t einmessenFehlenderGrund(const EllbogenZustand &z) {
  // Beugung: Anzeige-Wert zu klein, oder (mit eindeutiger Achse) die ECHTE Spannweite zu klein.
  // Für den Erfolg zählt immer die echte Spannweite, nie nur die vorläufige Schätzung (B-6).
  // (Ohne gültige Extreme sind beugMin und beugMax 0, die Spannweite also 0: kein Erfolg.)
  if (!(z.beugBest >= EINMESSEN_BEUGUNG_MIN) ||
      (z.achseProzent >= 100 && !(z.beugMax - z.beugMin >= EINMESSEN_BEUGUNG_MIN))) {
    return EINMESSEN_GRUND_BEUGUNG;
  }
  if (!(oberarmGrad(z) >= EINMESSEN_OBERARM_MIN)) {
    return EINMESSEN_GRUND_OBERARM;
  }
  if (!(z.handMax >= EINMESSEN_HAND_MIN)) {
    return EINMESSEN_GRUND_HAND;
  }
  if (z.achseProzent < 100) {
    return EINMESSEN_GRUND_OBERARM;       // Achse nicht eindeutig (wie REQ-002 AK-7d)
  }
  return EINMESSEN_GRUND_KEINER;
}

// Einmal pro Sekunde in Schritt 2: Achse suchen, Fortschritt rechnen, wenn alles reicht abschliessen
// (siehe logik.h).
bool einmessenAbschliessen(EllbogenZustand &z) {
  if (z.phase != PHASE_FREI) {
    return false;                         // Schritt 2 läuft nicht
  }

  // 1. Knochenachse des Unterarms aus der Handdrehung
  float f2[3] = {0.0f, 0.0f, 0.0f};
  bool handOk = knochenachseAusHand(z, f2);

  // 2. Scharnier-Achse suchen und das (vorläufige) Modell bauen
  float x[6];
  float lam1 = 0.0f;
  float lam2 = 0.0f;
  bool fertig = false;
  EinmessModell m;
  bool gefunden = achseSuchen(z, f2, handOk, x, lam1, lam2, fertig);
  if (gefunden) {
    gefunden = modellBauen(z, x, f2, handOk, m);
  }

  // 3. Beuge-Wert nachführen (mit dem vorläufigen Modell)
  if (gefunden) {
    beugungPruefen(z, m);
  } else {
    z.posenAnzahl = POSEN_RING_START;     // ohne Achse nicht auswertbar: neue Lagen verwerfen
  }

  // 4. Achse in Prozent (AK-8): 100 genau dann, wenn die Prüfung bestanden ist
  //    (d) Scharnier-Achse eindeutig: genug Daten, schwächste Richtung gut belegt, kleiner Rest-Fehler
  //    (e) Knochenachse aus der Handdrehung, Scharnier-Achse quer dazu
  float grenzeFehler = ACHSE_FEHLER_MAX * (float)z.anzahlG;
  bool scharnierOk = gefunden && fertig && z.anzahlG >= MIN_ANZAHL_ACHSE && lam2 >= ACHSE_LAMBDA_MIN &&
                     lam1 <= grenzeFehler;
  // Mit eindeutiger Scharnier-Achse ist die Spannweite aus der Auswertung die echte: sie zählt für die Anzeige.
  // (Vorher nicht: eine noch falsche Achse könnte eine zu grosse Spannweite ergeben, die nie mehr sinkt.)
  if (scharnierOk && z.beugMax - z.beugMin > z.beugBest) {   // (ohne gültige Extreme ist beugMax - beugMin = 0)
    z.beugBest = z.beugMax - z.beugMin;
  }
  bool achseOk = scharnierOk && handOk && fabsf(punkt3(m.j2, m.f2)) <= DREH_QUER_MAX;
  if (achseOk) {
    z.achseProzent = 100;
  } else {
    float teil = 0.0f;
    if (gefunden) {
      teil = lam2 / ACHSE_LAMBDA_MIN;     // wie gut ist die schwächste Richtung belegt?
    }
    if (!(teil > 0.0f)) {
      teil = 0.0f;                        // auch bei NaN
    }
    if (teil > 0.99f) {
      teil = 0.99f;                       // nicht bestanden: höchstens 99 %
    }
    z.achseProzent = (uint8_t)(teil * 100.0f);
  }

  // 5. Reicht alles? Sonst weiter sammeln (z bleibt in Schritt 2, Referenz und Achse unverändert)
  if (einmessenFehlenderGrund(z) != EINMESSEN_GRUND_KEINER) {
    return false;
  }

  // 6. Neues Modell übernehmen (erst jetzt, damit sonst alles beim Alten bleibt)
  modellUebernehmen(z, m);
  return true;
}

// Bricht ein laufendes Einmessen ab. Referenz und Achse bleiben (siehe logik.h).
void einmessenAbbrechen(EllbogenZustand &z, uint8_t grund) {
  z.grund = grund;                        // für einmessenGrund()
  z.phase = 0;                            // Referenz und Achse bleiben wie sie waren
}

// Warum wurde das letzte Einmessen abgebrochen? (siehe logik.h)
uint8_t einmessenGrund(const EllbogenZustand &z) {
  return z.grund;
}

// Gibt den Winkel gegenüber der Referenz, wenn Referenz, Achse und Lage bekannt sind (siehe logik.h).
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

// Nach einem IMU-Ausfall: die Lage gilt als unbekannt, Referenz und Achse bleiben (siehe logik.h).
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


// ---------- Bestätigung beim Einmessen (REQ-004) ----------

// true für Leerzeichen, Zeilenende und Wagenrücklauf (siehe logik.h).
bool istBestaetigungsZeichen(char zeichen) {
  // Nur diese drei Zeichen zählen. Alles andere wird ohne Wirkung verworfen (B-1, B-7).
  return zeichen == ' ' || zeichen == '\n' || zeichen == '\r';
}

// true, wenn seit wartenSeitMs mindestens maxMs vergangen sind, auch beim Überlauf (siehe logik.h).
bool wartezeitAbgelaufen(unsigned long wartenSeitMs, unsigned long jetztMs, unsigned long maxMs) {
  // Rechnen mit 32-Bit-Zahlen ohne Vorzeichen (gleich wie millis() auf dem Arduino):
  // "jetzt - seit" stimmt dann auch, wenn millis() dazwischen übergelaufen ist.
  // (Auf dem PC ist unsigned long oft 64 Bit lang, darum hier ausdrücklich 32 Bit.)
  uint32_t vergangen = (uint32_t)jetztMs - (uint32_t)wartenSeitMs;
  uint32_t grenze = 0xFFFFFFFFUL;
  if (maxMs < 0xFFFFFFFFUL) {
    grenze = (uint32_t)maxMs;
  }
  return vergangen >= grenze;
}

