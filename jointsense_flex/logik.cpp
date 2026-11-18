// logik.cpp - Prototyp 2 (Flex-Sensor): Inhalt der Logik-Funktionen aus logik.h (REQ-008).
// Kein Arduino.h, kein millis(), kein Serial: die Zeit kommt als Parameter "jetztMs".
// Zeiten werden immer als Differenz "jetztMs - startMs" verglichen. So stimmt es auch,
// wenn millis() nach ca. 49 Tagen wieder bei 0 beginnt.

#include "logik.h"
#include <math.h>

// Hilfsfunktion: true, wenn x eine normale Zahl ist (nicht NaN, nicht unendlich).
static bool zahlOk(float x) {
  return !isnan(x) && !isinf(x);
}

// true, wenn der ADC-Wert im gültigen Bereich 100 bis 3995 liegt (AK-3).
bool flexWertGueltig(int adc) {
  return adc >= ADC_GUELTIG_MIN && adc <= ADC_GUELTIG_MAX;
}

// Widerstand aus dem Spannungsteiler: R_flex = 22000 x (4095 / adc - 1) (AK-2).
// Bei ungültigem Wert gibt es -1.0 (dann darf niemand damit rechnen).
float flexWiderstandOhm(int adc) {
  if (!flexWertGueltig(adc)) {
    return -1.0f;
  }
  float verhaeltnis = (float)ADC_MAX / (float)adc;
  return TEILER_WIDERSTAND_OHM * (verhaeltnis - 1.0f);
}

// Abweichung in Prozent: (R_referenz - R_flex) / R_referenz x 100 (AK-6).
// Ohne gültige Referenz (<= 0 oder NaN) gibt es 0.0.
float abweichungProzent(float widerstandOhm, float referenzOhm) {
  if (!(referenzOhm > 0.0f) || isinf(referenzOhm)) {
    return 0.0f;
  }
  return (referenzOhm - widerstandOhm) / referenzOhm * 100.0f;
}

// Zählt ungültige und gültige Werte in Folge (AK-4).
// 3 ungültige in Folge -> ausgefallen. Danach 3 gültige in Folge -> wieder ok.
bool ausfallAktualisieren(AusfallZustand &z, bool wertGueltig) {
  if (wertGueltig) {
    z.ungueltigInFolge = 0;
    if (z.gueltigInFolge < AUSFALL_GUELTIG_IN_FOLGE) {
      z.gueltigInFolge++;  // nicht weiter zählen als nötig (kein Überlauf)
    }
    if (z.ausgefallen && z.gueltigInFolge >= AUSFALL_GUELTIG_IN_FOLGE) {
      z.ausgefallen = false;
    }
  } else {
    z.gueltigInFolge = 0;
    if (z.ungueltigInFolge < AUSFALL_UNGUELTIG_IN_FOLGE) {
      z.ungueltigInFolge++;
    }
    if (z.ungueltigInFolge >= AUSFALL_UNGUELTIG_IN_FOLGE) {
      z.ausgefallen = true;
    }
  }
  return z.ausgefallen;
}

// Hilfsfunktion: Das 2-s-Fenster beginnt neu mit genau diesem einen Wert.
static void fensterNeuStarten(EinmessZustand &z, float widerstandOhm, uint32_t jetztMs) {
  z.fensterLeer = false;
  z.fensterStartMs = jetztMs;
  z.summeOhm = widerstandOhm;
  z.anzahl = 1;
  z.minOhm = widerstandOhm;
  z.maxOhm = widerstandOhm;
}

// Setzt alles für ein neues Einmessen zurück (AK-5).
void einmessenStarten(EinmessZustand &z, uint32_t jetztMs) {
  z.startMs = jetztMs;
  z.fensterLeer = true;
  z.fensterStartMs = jetztMs;
  z.summeOhm = 0.0;
  z.anzahl = 0;
  z.minOhm = 0.0f;
  z.maxOhm = 0.0f;
  z.referenzOhm = -1.0f;
  z.fertig = false;
  z.beendet = false;
}

// Ein Schritt des Einmessens (AK-5). Ablauf:
//  - ungültiger Wert oder kaputte Zahl -> Abbruch
//  - Wert ins 2-s-Fenster legen. Ist dann max - min > 3 % vom Mittelwert (Wackeln),
//    beginnt das Fenster neu, und zwar mit genau diesem Wert.
//  - Fenster 2 s lang ruhig -> fertig, Referenz = Mittelwert
//  - 30 s ohne Erfolg -> Abbruch
uint8_t einmessenSchritt(EinmessZustand &z, float widerstandOhm, bool wertGueltig, uint32_t jetztMs) {
  // Schon beendet? Dann das Ergebnis einfach wiederholen.
  if (z.beendet) {
    return z.fertig ? EINMESS_FERTIG : EINMESS_ABBRUCH;
  }

  // Ungültiger Wert, NaN, unendlich oder kein positiver Widerstand -> Abbruch (fail-safe)
  if (!wertGueltig || !zahlOk(widerstandOhm) || widerstandOhm <= 0.0f) {
    z.beendet = true;
    z.fertig = false;
    return EINMESS_ABBRUCH;
  }

  if (z.fensterLeer) {
    fensterNeuStarten(z, widerstandOhm, jetztMs);
  } else {
    // Wie sähe das Fenster mit dem neuen Wert aus?
    float neuMin = (widerstandOhm < z.minOhm) ? widerstandOhm : z.minOhm;
    float neuMax = (widerstandOhm > z.maxOhm) ? widerstandOhm : z.maxOhm;
    double neuSumme = z.summeOhm + widerstandOhm;
    uint32_t neuAnzahl = z.anzahl + 1;
    double mittelwert = neuSumme / (double)neuAnzahl;

    if ((double)(neuMax - neuMin) > mittelwert * EINMESS_RUHE_PROZENT / 100.0) {
      // Gewackelt: die 2 s beginnen mit diesem Wert neu (kein Abbruch)
      fensterNeuStarten(z, widerstandOhm, jetztMs);
    } else {
      z.minOhm = neuMin;
      z.maxOhm = neuMax;
      z.summeOhm = neuSumme;
      z.anzahl = neuAnzahl;
    }
  }

  // 2 s am Stück ruhig? -> fertig
  if (jetztMs - z.fensterStartMs >= EINMESS_RUHIG_MS) {
    z.referenzOhm = (float)(z.summeOhm / (double)z.anzahl);
    z.fertig = true;
    z.beendet = true;
    return EINMESS_FERTIG;
  }

  // 30 s vorbei ohne 2 ruhige Sekunden -> Abbruch
  if (jetztMs - z.startMs >= EINMESS_MAX_MS) {
    z.fertig = false;
    z.beendet = true;
    return EINMESS_ABBRUCH;
  }

  return EINMESS_LAEUFT;
}

// Referenz nach erfolgreichem Einmessen. Sonst -1.0 (= keine Referenz).
float einmessenReferenz(const EinmessZustand &z) {
  if (!z.fertig) {
    return -1.0f;
  }
  return z.referenzOhm;
}

// Begrenzt den Grenzwert auf 0 bis 20 % und rundet auf ganze Prozent (AK-7).
// Beispiel: 5,4 -> 5, 5,5 -> 6, 25 -> 20, -3 -> 0. NaN -> 5 (Vorgabe).
float grenzwertBegrenzen(float grenzwertProzent) {
  if (isnan(grenzwertProzent)) {
    return GRENZWERT_VORGABE_PROZENT;
  }
  float wert = grenzwertProzent;
  if (wert < GRENZWERT_MIN_PROZENT) {
    wert = GRENZWERT_MIN_PROZENT;  // fängt auch "minus unendlich" ab
  }
  if (wert > GRENZWERT_MAX_PROZENT) {
    wert = GRENZWERT_MAX_PROZENT;  // fängt auch "plus unendlich" ab
  }
  return floorf(wert + 0.5f);      // auf ganze Prozent runden (x,5 aufrunden)
}

// Entprellung und Hysterese (AK-8, AK-9):
//  - Motor aus: Abweichung > Grenzwert merkt sich die Zeit der ersten solchen Messung.
//    Sind seither >= entprellMs vergangen, geht der Motor an.
//    Eine Messung nicht über dem Grenzwert setzt die Entprellung zurück.
//  - Motor an: bleibt an, bis die Abweichung < Grenzwert - Hysterese ist, dann sofort aus.
//  - Kaputte Zahlen (NaN, unendlich): Motor sofort aus (fail-safe).
bool grenzwertPruefen(GrenzwertZustand &z, float abweichungProzent, uint32_t jetztMs,
                      float grenzwertProzent, float hystereseProzent, uint32_t entprellMs) {
  if (!zahlOk(abweichungProzent) || !zahlOk(grenzwertProzent) || !zahlOk(hystereseProzent)) {
    z.motorAn = false;
    z.ueberGrenze = false;
    return false;
  }
  float hysterese = (hystereseProzent < 0.0f) ? 0.0f : hystereseProzent;

  if (z.motorAn) {
    if (abweichungProzent < grenzwertProzent - hysterese) {
      // genug zurück -> sofort aus, Entprellung von vorne
      z.motorAn = false;
      z.ueberGrenze = false;
    }
    return z.motorAn;
  }

  if (abweichungProzent > grenzwertProzent) {
    if (!z.ueberGrenze) {
      z.ueberGrenze = true;      // erste Messung über dem Grenzwert
      z.ueberSeitMs = jetztMs;
    }
    if (jetztMs - z.ueberSeitMs >= entprellMs) {
      z.motorAn = true;          // lange genug drüber -> Motor an
    }
  } else {
    z.ueberGrenze = false;       // wieder drunter -> Entprellung zurücksetzen
  }
  return z.motorAn;
}

// Entprellt die Taste (AK-13). Ein neuer Zustand gilt erst, wenn der Rohwert
// entprellMs lang gleich geblieben ist. true genau beim Wechsel auf "gedrückt".
bool tasteNeuGedrueckt(TasteZustand &z, bool istGedrueckt, uint32_t jetztMs, uint32_t entprellMs) {
  if (istGedrueckt != z.letzterRohwert) {
    z.letzterRohwert = istGedrueckt;  // Rohwert hat gewechselt: Zeit neu merken
    z.wechselMs = jetztMs;
  }
  if (z.letzterRohwert != z.stabilGedrueckt && jetztMs - z.wechselMs >= entprellMs) {
    z.stabilGedrueckt = z.letzterRohwert;
    if (z.stabilGedrueckt) {
      return true;                    // neuer, entprellter Druck
    }
  }
  return false;
}

// Motor wegen Überstreckung nur, wenn alles in Ordnung ist (AK-4, AK-5, AK-10).
bool motorSollAn(bool sensorAusfall, bool referenzDa, bool einmessenLaeuft, bool zuWeit) {
  return zuWeit && referenzDa && !sensorAusfall && !einmessenLaeuft;
}

// Verarbeitet ein Zeichen von Serial (AK-13, B-1). Es wird nichts gespeichert,
// nur gezählt. So kann keine lange Eingabe den Speicher füllen.
//  - '\n', '\r' oder ' ' als erstes Zeichen einer Zeile -> Bestätigung
//  - "\r\n" gilt als EIN Zeilenende (keine zweite Bestätigung)
//  - Zeile mit mehr als 64 Zeichen -> bis zum Zeilenende ignorieren
//  - alle anderen Zeichen -> nie eine Bestätigung
uint8_t serialZeichenVerarbeiten(SerialZeile &z, char zeichen) {
  if (zeichen == '\r' || zeichen == '\n') {
    if (zeichen == '\n' && z.letztesWarCR) {
      z.letztesWarCR = false;  // gehört zum "\r" davor: kein neues Zeilenende
      return SERIAL_NICHTS;
    }
    uint8_t ergebnis = SERIAL_NICHTS;
    if (!z.verwerfen && z.laenge == 0) {
      ergebnis = SERIAL_BESTAETIGUNG;  // leere Zeile = Enter
    }
    // neue Zeile beginnt
    z.laenge = 0;
    z.verwerfen = false;
    z.letztesWarCR = (zeichen == '\r');
    return ergebnis;
  }

  z.letztesWarCR = false;
  if (z.verwerfen) {
    return SERIAL_NICHTS;  // Rest einer zu langen Zeile
  }

  uint8_t ergebnis = SERIAL_NICHTS;
  if (zeichen == ' ' && z.laenge == 0) {
    ergebnis = SERIAL_BESTAETIGUNG;    // Leerzeichen am Zeilenanfang
  }
  z.laenge++;                          // höchstens 65, dann wird verworfen
  if (z.laenge > SERIAL_MAX_ZEICHEN) {
    z.verwerfen = true;
    z.laenge = SERIAL_MAX_ZEICHEN + 1;
  }
  return ergebnis;
}

// Blinkmuster der Status-LED (AK-4, AK-10, AK-11):
//  bereit = dauernd an, keine Referenz / einmessen = 1 Hz, Ausfall = 5 Hz.
//  Unbekannter Zustand: wie Ausfall (5 Hz), damit ein Fehler sichtbar ist.
uint8_t ledMuster(uint8_t zustand, uint32_t jetztMs) {
  if (zustand == ZUSTAND_BEREIT) {
    return 1;
  }
  if (zustand == ZUSTAND_KEINE_REFERENZ || zustand == ZUSTAND_EINMESSEN) {
    return (jetztMs % 1000u < 500u) ? 1 : 0;  // 1 Hz: 500 ms an, 500 ms aus
  }
  return (jetztMs % 200u < 100u) ? 1 : 0;     // 5 Hz: 100 ms an, 100 ms aus
}
