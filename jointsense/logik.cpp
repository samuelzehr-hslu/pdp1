// logik.cpp
// Hier steht, WIE die Funktionen aus logik.h arbeiten.
// Keine Arduino-Befehle in dieser Datei (siehe Erklärung in logik.h).

#include <math.h>
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
