// logik.h - Prototyp 2 (Flex-Sensor): reine Logik OHNE Arduino-Befehle (REQ-008).
// Hier steht die Liste der Funktionen, in logik.cpp ihr Inhalt.
// Die Zeit (millis()) wird immer als Wert "jetztMs" übergeben, nie hier abgefragt.
// So kann der Tester die Logik mit g++ auf dem PC testen.
//
// Alle Zustände (struct) sind mit "T z{};" (alles 0) gültig angelegt:
// kein Ausfall, Motor aus, Taste nicht gedrückt, leere Zeile.
// Nur EinmessZustand wird mit einmessenStarten() vorbereitet.

#ifndef LOGIK_H
#define LOGIK_H

#include <stdint.h>

// ---------- Feste Werte der Schaltung (einzige Stelle, AK-17) ----------
const float TEILER_WIDERSTAND_OHM = 22000.0f; // Festwiderstand im Spannungsteiler
const int ADC_MAX = 4095;                     // grösster ADC-Wert bei 12 Bit

// Gültiger Bereich des ADC-Werts (AK-3): darunter Kabel ab, darüber Kurzschluss
const int ADC_GUELTIG_MIN = 100;
const int ADC_GUELTIG_MAX = 3995;

// Sensor-Ausfall (AK-4): so viele Werte in Folge
const uint8_t AUSFALL_UNGUELTIG_IN_FOLGE = 3;
const uint8_t AUSFALL_GUELTIG_IN_FOLGE = 3;

// Einmessen (AK-5)
const uint32_t EINMESS_RUHIG_MS = 2000;    // so lange muss der Arm am Stück ruhig sein
const uint32_t EINMESS_MAX_MS = 30000;     // danach Abbruch
const float EINMESS_RUHE_PROZENT = 3.0f;   // max - min höchstens 3 % vom Mittelwert

// Grenzwert (AK-7)
const float GRENZWERT_MIN_PROZENT = 0.0f;
const float GRENZWERT_MAX_PROZENT = 20.0f;
const float GRENZWERT_VORGABE_PROZENT = 5.0f;

// Serielle Eingabe (AK-13): höchstens so viele Zeichen pro Zeile
const uint8_t SERIAL_MAX_ZEICHEN = 64;

// Rückgaben von einmessenSchritt()
const uint8_t EINMESS_LAEUFT = 0;
const uint8_t EINMESS_FERTIG = 1;
const uint8_t EINMESS_ABBRUCH = 2;

// Rückgaben von serialZeichenVerarbeiten()
const uint8_t SERIAL_NICHTS = 0;
const uint8_t SERIAL_BESTAETIGUNG = 1;

// Zustände des Geräts (gleiche Nummern wie in der D-Zeile, AK-14)
const uint8_t ZUSTAND_KEINE_REFERENZ = 0;
const uint8_t ZUSTAND_BEREIT = 1;
const uint8_t ZUSTAND_EINMESSEN = 2;
const uint8_t ZUSTAND_SENSOR_AUSFALL = 3;
const uint8_t ZUSTAND_MOTOR_AUSFALL = 4;

// ---------- Zustände (struct) ----------

// Merkt sich, wie viele ungültige bzw. gültige Werte in Folge kamen (AK-4).
struct AusfallZustand {
  bool ausgefallen;          // true = Sensor gilt als ausgefallen
  uint8_t ungueltigInFolge;  // Zähler ungültige Werte
  uint8_t gueltigInFolge;    // Zähler gültige Werte
};

// Merkt sich das laufende Einmessen (AK-5).
struct EinmessZustand {
  uint32_t startMs;          // wann das Einmessen begonnen hat (für die 30 s)
  bool fensterLeer;          // true = das 2-s-Fenster hat noch keinen Wert
  uint32_t fensterStartMs;   // Zeit des ersten Werts im Fenster
  double summeOhm;           // Summe aller Widerstände im Fenster
  uint32_t anzahl;           // Anzahl Werte im Fenster
  float minOhm;              // kleinster Wert im Fenster
  float maxOhm;              // grösster Wert im Fenster
  float referenzOhm;         // Ergebnis nach EINMESS_FERTIG
  bool fertig;               // true = Einmessen erfolgreich beendet
  bool beendet;              // true = fertig oder abgebrochen
};

// Merkt sich Entprellung und Hysterese der Überstreckungs-Warnung (AK-8, AK-9).
struct GrenzwertZustand {
  bool motorAn;              // true = Motor soll wegen Überstreckung laufen
  bool ueberGrenze;          // true = letzte Messung war über dem Grenzwert
  uint32_t ueberSeitMs;      // Zeit der ersten Messung über dem Grenzwert
};

// Merkt sich die Entprellung der Taste (AK-13).
struct TasteZustand {
  bool stabilGedrueckt;      // entprellter Zustand
  bool letzterRohwert;       // zuletzt gelesener (unentprellter) Zustand
  uint32_t wechselMs;        // Zeit des letzten Wechsels des Rohwerts
};

// Merkt sich die aktuelle Zeile über Serial (AK-13, B-1). Kein Puffer nötig:
// wir zählen nur die Zeichen, denn nur das erste Zeichen einer Zeile ist wichtig.
struct SerialZeile {
  uint8_t laenge;            // Anzahl Zeichen in der aktuellen Zeile (höchstens 65)
  bool verwerfen;            // true = Zeile war zu lang, bis zum Zeilenende ignorieren
  bool letztesWarCR;         // true = das letzte Zeichen war '\r'
};

// ---------- Funktionen ----------

// true, wenn der ADC-Wert gültig ist (100 bis 3995, AK-3).
bool flexWertGueltig(int adc);

// Widerstand des Flex-Sensors in Ohm aus dem ADC-Wert (AK-2). Ungültiger Wert: -1.0
float flexWiderstandOhm(int adc);

// Abweichung in Prozent vom eingemessenen Widerstand (AK-6).
// Positiv = weniger Biegung = Richtung Überstreckung. Ist referenzOhm <= 0: 0.0
float abweichungProzent(float widerstandOhm, float referenzOhm);

// Zählt gültige/ungültige Werte. true = Sensor ausgefallen (AK-4).
bool ausfallAktualisieren(AusfallZustand &z, bool wertGueltig);

// Bereitet das Einmessen vor (AK-5).
void einmessenStarten(EinmessZustand &z, uint32_t jetztMs);

// Ein Schritt des Einmessens, einmal pro Messzyklus (AK-5).
// Rückgabe: EINMESS_LAEUFT, EINMESS_FERTIG oder EINMESS_ABBRUCH.
uint8_t einmessenSchritt(EinmessZustand &z, float widerstandOhm, bool wertGueltig, uint32_t jetztMs);

// Referenz-Widerstand in Ohm nach EINMESS_FERTIG (sonst -1.0).
float einmessenReferenz(const EinmessZustand &z);

// Begrenzt den Grenzwert auf 0 bis 20 % und rundet auf ganze Prozent. NaN: 5 % (AK-7).
float grenzwertBegrenzen(float grenzwertProzent);

// Entprellung und Hysterese. true = Motor soll wegen Überstreckung an (AK-8, AK-9).
bool grenzwertPruefen(GrenzwertZustand &z, float abweichungProzent, uint32_t jetztMs,
                      float grenzwertProzent, float hystereseProzent, uint32_t entprellMs);

// Entprellt die Taste. true genau einmal pro entprelltem Druck (AK-13).
bool tasteNeuGedrueckt(TasteZustand &z, bool istGedrueckt, uint32_t jetztMs, uint32_t entprellMs);

// true nur, wenn zu weit UND Referenz da UND kein Ausfall UND kein Einmessen.
bool motorSollAn(bool sensorAusfall, bool referenzDa, bool einmessenLaeuft, bool zuWeit);

// Verarbeitet ein Zeichen von Serial (AK-13, B-1).
// Rückgabe: SERIAL_BESTAETIGUNG bei '\n', '\r' oder ' ' am Zeilenanfang, sonst SERIAL_NICHTS.
uint8_t serialZeichenVerarbeiten(SerialZeile &z, char zeichen);

// Blinkmuster der Status-LED: 0 = aus, 1 = an (AK-4, AK-10, AK-11).
uint8_t ledMuster(uint8_t zustand, uint32_t jetztMs);

#endif
