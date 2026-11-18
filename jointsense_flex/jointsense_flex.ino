// JointSense Prototyp 2 (Flex-Sensor) - Überstreckung des Ellbogens erkennen (REQ-008).
// Board: Adafruit Feather RP2040. Alle Pins und die I2C-Adresse stehen in pins.h
// (Quelle: README, Abschnitt "Verdrahtung Prototyp 2 (Flex)").
// Die reine Logik (Widerstand, Einmessen, Grenzwert, Taste, Serial, LED) steht in logik.h / logik.cpp.
// Diese Datei macht nur den Ablauf und redet mit der Hardware.
//
// So funktioniert es:
//   1. Ein Flex-Sensor liegt auf der Rückseite des Ellbogens (schon leicht gebogen).
//      Überstrecken = weniger Biegung = kleinerer Widerstand.
//   2. Alle 20 ms (50 Mal pro Sekunde) wird A0 gemessen (12 Bit) und daraus der Widerstand berechnet.
//   3. Einmessen: Taste drücken ODER Enter/Leertaste über Serial. Arm 2 s entspannt gestreckt
//      und ruhig halten. Der Mittelwert ist die Referenz. Danach vibriert der Motor 300 ms.
//      Nach dem Einschalten gibt es KEINE Referenz (kein automatisches Einmessen).
//   4. Abweichung in Prozent = (Referenz - Widerstand) / Referenz x 100. Positiv = Überstreckung.
//      Liegt sie länger als 80 ms über dem Grenzwert, vibriert der Motor, bis sie wieder
//      1 Prozentpunkt unter dem Grenzwert ist.
//   5. Sicherheit: 3 ungültige Messwerte in Folge = Sensor-Ausfall -> Motor aus.
//      Antwortet der Motortreiber nicht -> Motor-Ausfall melden, Messen läuft weiter.
//      Watchdog: hängt das Programm länger als 500 ms, startet der Feather neu.
//
// Status-LED: dauernd an = bereit, langsam blinken (1 Hz) = keine Referenz oder einmessen,
//             schnell blinken (5 Hz) = Sensor- oder Motor-Ausfall.
//
// Aufbau dieser Datei:
//   1. Einstellungen und Variablen
//   2. setup() und loop()
//   3. MESSEN und EINMESSEN
//   4. MOTOR (DRV2605L über I2C)
//   5. BEDIENUNG: Taste und Serial
//   6. MELDUNGEN: Zeilen für das GUI bzw. den Seriellen Monitor

#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pins.h"
#include "logik.h"

// ---------- 1. Einstellungen (hier dürft ihr Werte ändern) ----------

// Ausgabe-Modus (AK-14). Serial läuft immer mit 115200 Baud.
//   1 = GUI-Modus: nur Zeilen G, D, E für tools/gui_flex/jointsense_flex_live.html
//   0 = Text-Modus: lesbare deutsche Texte für den Seriellen Monitor
#define GUI_MODUS 1

const float GRENZWERT_PROZENT = 5.0f;   // Grenzwert 0 bis 20 %, ganze Prozent (AK-7)
const float HYSTERESE_PROZENT = 1.0f;   // Motor aus erst unter Grenzwert - 1 Prozentpunkt (AK-8)
const uint32_t ENTPRELL_MS = 80;        // so lange muss die Abweichung über dem Grenzwert sein (AK-8)
const uint8_t MOTOR_STAERKE = 100;      // Stärke der Vibration 0 bis 127 (AK-11)

// Feste Werte (nur ändern, wenn die Anforderung es sagt)
const unsigned long SERIAL_BAUD = 115200;    // AK-14
const uint32_t MESS_INTERVALL_MS = 20;       // 50 Messungen pro Sekunde (AK-1)
const int ADC_MITTEL_ANZAHL = 4;             // so viele ADC-Messungen pro Zyklus mitteln (AK-1)
const uint32_t TASTE_ENTPRELL_MS = 30;       // AK-13
const uint32_t EINMESS_STOSS_MS = 300;       // Vibrationsstoss nach dem Einmessen (AK-5)
const uint32_t MELDUNG_INTERVALL_MS = 1000;  // Fehler-Meldungen alle 1000 ms (AK-4, AK-10, AK-11)
const uint32_t WATCHDOG_MS = 500;            // AK-12
const uint32_t I2C_ZEITLIMIT_MS = 10;        // höchstens 10 ms pro I2C-Befehl (AK-13, B-3)
const uint8_t MOTOR_FEHLER_IN_FOLGE = 3;     // so viele Fehlschläge = Motor-Ausfall (AK-11)
const int SERIAL_MAX_PRO_LOOP = 64;          // höchstens so viele Zeichen pro loop() lesen (B-1)

// Der Compiler prüft die Stärke schon beim Kompilieren.
static_assert(MOTOR_STAERKE <= 127, "MOTOR_STAERKE muss 0 bis 127 sein");

// Register des DRV2605L (aus dem Datenblatt von Texas Instruments)
const uint8_t DRV_REG_STATUS = 0x00;    // Status (zum Suchen des Chips)
const uint8_t DRV_REG_MODUS = 0x01;     // Betriebsart
const uint8_t DRV_REG_RTP = 0x02;       // Stärke im Echtzeit-Modus (RTP)
const uint8_t DRV_REG_GO = 0x0C;        // Wellenform starten/stoppen
const uint8_t DRV_REG_FEEDBACK = 0x1A;  // Bit 7: 0 = ERM-Motor, 1 = LRA-Motor
const uint8_t DRV_REG_CONTROL3 = 0x1D;  // Bit 5: ERM ohne Regelung, Bit 3: Zahlenformat RTP
const uint8_t DRV_MODUS_INTERN = 0x00;  // aktiv, interner Auslöser (spielt nichts)
const uint8_t DRV_MODUS_RTP = 0x05;     // aktiv, Echtzeit-Modus: Stärke = Register RTP

// ---------- Variablen ----------
float grenzwert = GRENZWERT_VORGABE_PROZENT;  // geprüfter Grenzwert (beim Start gesetzt)

AusfallZustand sensorAusfallZustand{};
EinmessZustand einmessZustand{};
GrenzwertZustand grenzwertZustand{};
TasteZustand tasteZustand{};
SerialZeile serialZeile{};

float referenzOhm = -1.0f;        // -1 = keine Referenz
bool einmessenLaeuft = false;
bool stossAktiv = false;          // Vibrationsstoss nach dem Einmessen läuft
uint32_t stossStartMs = 0;

bool sensorAusgefallen = false;
bool motorAusgefallen = false;
uint8_t motorFehlerZaehler = 0;
bool motorAn = false;             // was wir dem Motor zuletzt befohlen haben

uint32_t letzteMessungMs = 0;
uint32_t letzteMeldungMs = 0;
bool usbWarVerbunden = false;

// ---------- 2. setup() und loop() ----------

// Wird einmal beim Start ausgeführt.
void setup() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  pinMode(PIN_TASTE, INPUT_PULLUP);

  // Zuerst den Motor ausschalten: Er kann noch von vor einem Neustart vibrieren (AK-12, B-9).
  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setTimeout(I2C_ZEITLIMIT_MS, true);  // true = Bus nach Zeitüberschreitung zurücksetzen
  motorAusgefallen = !motorTreiberEinrichten();

  Serial.begin(SERIAL_BAUD);  // nicht auf den PC warten (sonst läuft ohne USB nichts)
  analogReadResolution(12);   // 0 bis 4095 (AK-1)

  // Grenzwert prüfen (AK-7)
  grenzwert = grenzwertBegrenzen(GRENZWERT_PROZENT);

  rp2040.wdt_begin(WATCHDOG_MS);  // Watchdog an (AK-12)

  startMeldungen();
  letzteMessungMs = millis();
  letzteMeldungMs = millis();
}

// Läuft immer wieder. Jeder Schritt ist kurz, nichts wartet.
void loop() {
  rp2040.wdt_reset();  // "Ich lebe noch" für den Watchdog
  uint32_t jetzt = millis();

  tasteLesen(jetzt);
  serialLesen(jetzt);

  if (jetzt - letzteMessungMs >= MESS_INTERVALL_MS) {
    if (jetzt - letzteMessungMs > 5 * MESS_INTERVALL_MS) {
      letzteMessungMs = jetzt;  // weit im Rückstand: nicht nachholen, neu anfangen
    } else {
      letzteMessungMs += MESS_INTERVALL_MS;  // fester Takt: im Mittel genau 50 pro Sekunde
    }
    messZyklus(jetzt);
  }

  if (jetzt - letzteMeldungMs >= MELDUNG_INTERVALL_MS) {
    letzteMeldungMs = jetzt;
    sekundenAufgaben(jetzt);
  }

  digitalWrite(PIN_STATUS_LED, ledMuster(zustandBestimmen(), jetzt) ? HIGH : LOW);
  usbVerbindungPruefen();
}

// ---------- 3. MESSEN und EINMESSEN ----------

// Liest A0 mehrmals und gibt den Mittelwert zurück (0 bis 4095).
int flexAdcLesen() {
  long summe = 0;
  for (int i = 0; i < ADC_MITTEL_ANZAHL; i++) {
    summe += analogRead(PIN_FLEX);
  }
  long mittel = (summe + ADC_MITTEL_ANZAHL / 2) / ADC_MITTEL_ANZAHL;
  if (mittel < 0) mittel = 0;            // Sicherheit: im Bereich 0 bis 4095 bleiben
  if (mittel > ADC_MAX) mittel = ADC_MAX;
  return (int)mittel;
}

// Ein Messzyklus (alle 20 ms): messen, prüfen, einmessen, Grenzwert, Motor, Datenzeile.
void messZyklus(uint32_t jetzt) {
  int adc = flexAdcLesen();
  bool gueltig = flexWertGueltig(adc);
  float widerstand = flexWiderstandOhm(adc);  // -1 bei ungültigem Wert

  // Sensor-Ausfall (AK-4)
  bool ausfallJetzt = ausfallAktualisieren(sensorAusfallZustand, gueltig);
  if (ausfallJetzt && !sensorAusgefallen) {
    ereignisSenden("Sensor-Ausfall - Motor aus");
  } else if (!ausfallJetzt && sensorAusgefallen) {
    ereignisSenden("Sensor wieder ok");
  }
  sensorAusgefallen = ausfallJetzt;

  // Einmessen (AK-5)
  if (einmessenLaeuft) {
    einmessenWeiter(widerstand, gueltig, jetzt);
  }

  bool referenzDa = referenzOhm > 0.0f;

  // Abweichung nur aus gültigen Werten (AK-3)
  float abweichung = 0.0f;
  if (gueltig && referenzDa) {
    abweichung = abweichungProzent(widerstand, referenzOhm);
  }

  // Grenzwert mit Entprellung und Hysterese (AK-8, AK-9). Sonst Zustand zurücksetzen (Motor aus).
  bool zuWeit = false;
  if (gueltig && referenzDa && !einmessenLaeuft && !sensorAusgefallen) {
    zuWeit = grenzwertPruefen(grenzwertZustand, abweichung, jetzt, grenzwert,
                              HYSTERESE_PROZENT, ENTPRELL_MS);
  } else {
    grenzwertZustand = GrenzwertZustand{};
  }

  bool an = motorSollAn(sensorAusgefallen, referenzDa, einmessenLaeuft, zuWeit);

  // Vibrationsstoss nach dem Einmessen (300 ms), aber nie bei Sensor-Ausfall
  if (stossAktiv) {
    if (jetzt - stossStartMs < EINMESS_STOSS_MS && !sensorAusgefallen) {
      an = true;
    } else {
      stossAktiv = false;
    }
  }

  motorSetzen(an);
  datenzeileSenden(jetzt, adc, gueltig ? widerstand : 0.0f, abweichung);
}

// Startet das Einmessen (nur wenn nicht schon eines läuft, B-7).
void einmessenBeginnen(uint32_t jetzt) {
  if (einmessenLaeuft) {
    return;  // Bestätigung während des Einmessens wird ignoriert
  }
  einmessenStarten(einmessZustand, jetzt);
  einmessenLaeuft = true;
  grenzwertZustand = GrenzwertZustand{};  // keine Warnung beim Einmessen
  ereignisSenden("Einmessen gestartet - Arm entspannt gestreckt halten");
}

// Ein Schritt des Einmessens. Bei Erfolg neue Referenz und Stoss, bei Abbruch alte Referenz behalten.
void einmessenWeiter(float widerstand, bool gueltig, uint32_t jetzt) {
  uint8_t ergebnis = einmessenSchritt(einmessZustand, widerstand, gueltig, jetzt);
  if (ergebnis == EINMESS_LAEUFT) {
    return;
  }
  einmessenLaeuft = false;

  if (ergebnis == EINMESS_FERTIG) {
    float neueReferenz = einmessenReferenz(einmessZustand);
    if (neueReferenz > 0.0f && !isnan(neueReferenz) && !isinf(neueReferenz)) {
      referenzOhm = neueReferenz;
      stossAktiv = true;
      stossStartMs = jetzt;
      char text[64];
      snprintf(text, sizeof(text), "Einmessen fertig - Referenz %ld Ohm", lroundf(referenzOhm));
      ereignisSenden(text);
      return;
    }
    // unmögliches Ergebnis: lieber wie ein Abbruch behandeln (fail-safe)
  }

  if (!gueltig) {
    ereignisSenden("Einmessen abgebrochen - Sensorwert ungueltig");
  } else {
    ereignisSenden("Einmessen abgebrochen - 30 s ohne 2 ruhige Sekunden");
  }
}

// Welcher Zustand gilt gerade? (Nummern wie in der D-Zeile, AK-14)
uint8_t zustandBestimmen() {
  if (sensorAusgefallen) return ZUSTAND_SENSOR_AUSFALL;
  if (motorAusgefallen) return ZUSTAND_MOTOR_AUSFALL;
  if (einmessenLaeuft) return ZUSTAND_EINMESSEN;
  if (!(referenzOhm > 0.0f)) return ZUSTAND_KEINE_REFERENZ;
  return ZUSTAND_BEREIT;
}

// ---------- 4. MOTOR (DRV2605L über I2C) ----------

// Schreibt einen Wert in ein Register des DRV2605L. true = hat geklappt.
bool drvRegisterSchreiben(uint8_t reg, uint8_t wert) {
  Wire.beginTransmission(I2C_ADRESSE_DRV2605L);
  Wire.write(reg);
  Wire.write(wert);
  return Wire.endTransmission() == 0;
}

// Liest ein Register des DRV2605L. true = hat geklappt, der Wert steht dann in "wert".
bool drvRegisterLesen(uint8_t reg, uint8_t &wert) {
  Wire.beginTransmission(I2C_ADRESSE_DRV2605L);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom((uint8_t)I2C_ADRESSE_DRV2605L, (uint8_t)1) != 1) {
    return false;
  }
  int gelesen = Wire.read();
  if (gelesen < 0 || gelesen > 255) {
    return false;
  }
  wert = (uint8_t)gelesen;
  return true;
}

// Sucht den DRV2605L, schaltet den Motor aus und stellt ihn für einen ERM-Motor
// im Echtzeit-Modus ein (AK-11). true = alles hat geklappt.
bool motorTreiberEinrichten() {
  // Zuerst: Stärke 0 und Betriebsart "intern" -> eine alte Vibration hört sofort auf (B-9)
  bool ok = drvRegisterSchreiben(DRV_REG_RTP, 0);
  ok = drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_INTERN) && ok;
  ok = drvRegisterSchreiben(DRV_REG_GO, 0) && ok;

  uint8_t status = 0;
  if (!drvRegisterLesen(DRV_REG_STATUS, status)) {
    return false;  // Chip antwortet nicht
  }

  // ERM-Motor wählen (Bit 7 = 0)
  uint8_t feedback = 0;
  ok = drvRegisterLesen(DRV_REG_FEEDBACK, feedback) && ok;
  ok = drvRegisterSchreiben(DRV_REG_FEEDBACK, (uint8_t)(feedback & 0x7F)) && ok;

  // ERM ohne Regelung (Bit 5 = 1), RTP-Zahlen mit Vorzeichen (Bit 3 = 0): 127 = volle Stärke
  uint8_t control3 = 0;
  ok = drvRegisterLesen(DRV_REG_CONTROL3, control3) && ok;
  ok = drvRegisterSchreiben(DRV_REG_CONTROL3, (uint8_t)((control3 | 0x20) & ~0x08)) && ok;

  // Echtzeit-Modus mit Stärke 0: Motor ist aus, bis wir eine Stärke schreiben
  ok = drvRegisterSchreiben(DRV_REG_RTP, 0) && ok;
  ok = drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_RTP) && ok;
  return ok;
}

// Merkt sich einen Fehlschlag. 3 in Folge = Motor-Ausfall (AK-11).
void motorFehlerZaehlen(bool geklappt) {
  if (geklappt) {
    motorFehlerZaehler = 0;
    return;
  }
  if (motorFehlerZaehler < MOTOR_FEHLER_IN_FOLGE) {
    motorFehlerZaehler++;
  }
  if (motorFehlerZaehler >= MOTOR_FEHLER_IN_FOLGE && !motorAusgefallen) {
    motorAusgefallen = true;
    ereignisSenden("Motor-Ausfall - DRV2605L antwortet nicht");
  }
}

// Schaltet den Motor an oder aus. Wird in jedem Messzyklus aufgerufen, damit der
// DRV2605L immer den richtigen Wert hat und ein Fehler schnell auffällt.
void motorSetzen(bool an) {
  motorAn = an;
  if (motorAusgefallen) {
    return;  // neuer Versuch einmal pro Sekunde in sekundenAufgaben()
  }
  uint8_t staerke = an ? MOTOR_STAERKE : 0;
  motorFehlerZaehlen(drvRegisterSchreiben(DRV_REG_RTP, staerke));
}

// Einmal pro Sekunde: Motor-Treiber prüfen bzw. neu einrichten.
void motorTreiberPruefen() {
  if (motorAusgefallen) {
    // Neuer Versuch. Zuerst nur fragen, ob er antwortet (kurz, stört das Messen kaum).
    uint8_t status = 0;
    if (drvRegisterLesen(DRV_REG_STATUS, status) && motorTreiberEinrichten()) {
      motorAusgefallen = false;
      motorFehlerZaehler = 0;
      ereignisSenden("Motor wieder ok");
    }
  } else {
    // Betriebsart neu schreiben, falls der DRV2605L kurz neu gestartet hat (dann wäre er im Standby)
    motorFehlerZaehlen(drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_RTP));
  }
}

// ---------- 5. BEDIENUNG: Taste und Serial ----------

// Liest die Taste (gedrückt = LOW wegen Pullup) und startet bei einem Druck das Einmessen.
void tasteLesen(uint32_t jetzt) {
  bool gedrueckt = digitalRead(PIN_TASTE) == LOW;
  if (tasteNeuGedrueckt(tasteZustand, gedrueckt, jetzt, TASTE_ENTPRELL_MS)) {
    einmessenBeginnen(jetzt);
  }
}

// Liest höchstens 64 Zeichen von Serial. Enter oder Leertaste am Zeilenanfang startet das Einmessen.
void serialLesen(uint32_t jetzt) {
  int gelesen = 0;
  while (gelesen < SERIAL_MAX_PRO_LOOP && Serial.available() > 0) {
    int zeichen = Serial.read();
    gelesen++;
    if (zeichen < 0 || zeichen > 255) {
      continue;  // kein gültiges Zeichen
    }
    if (serialZeichenVerarbeiten(serialZeile, (char)zeichen) == SERIAL_BESTAETIGUNG) {
      einmessenBeginnen(jetzt);
    }
  }
}

// ---------- 6. MELDUNGEN ----------

// Schickt eine fertige Zeile nur, wenn im USB-Puffer genug Platz ist.
// So bleibt das Programm nie hängen, wenn der PC nicht mitliest (B-3).
void zeileSenden(const char *text) {
  size_t laenge = strlen(text);
  if (Serial.availableForWrite() < (int)laenge) {
    return;
  }
  Serial.write((const uint8_t *)text, laenge);
}

// Ereignis melden: im GUI-Modus als "E,<text>", sonst als lesbarer Text.
// Text immer ohne Komma und höchstens 60 Zeichen.
void ereignisSenden(const char *text) {
  char zeile[72];
#if GUI_MODUS
  snprintf(zeile, sizeof(zeile), "E,%.60s\n", text);
#else
  snprintf(zeile, sizeof(zeile), "%.60s\n", text);
#endif
  zeileSenden(zeile);
}

// Begrenzt eine Zahl, bevor sie in eine ganze Zahl umgewandelt wird (kein Überlauf).
long sicherRunden(float wert) {
  if (isnan(wert)) return 0;
  if (wert > 999999999.0f) return 999999999L;
  if (wert < -999999999.0f) return -999999999L;
  return lroundf(wert);
}

// Pro Messzyklus: im GUI-Modus genau eine D-Zeile (AK-14).
void datenzeileSenden(uint32_t jetzt, int adc, float widerstand, float abweichung) {
#if GUI_MODUS
  char zeile[80];
  snprintf(zeile, sizeof(zeile), "D,%lu,%d,%ld,%ld,%d,%u\n",
           (unsigned long)jetzt, adc, sicherRunden(widerstand), sicherRunden(abweichung * 10.0f),
           motorAn ? 1 : 0, (unsigned)zustandBestimmen());
  zeileSenden(zeile);
#else
  (void)jetzt;
  (void)adc;
  (void)widerstand;
  (void)abweichung;
#endif
}

// G-Zeile (GUI-Modus): Grenzwert, Hysterese, Motorstärke.
void einstellungenSenden() {
#if GUI_MODUS
  char zeile[48];
  snprintf(zeile, sizeof(zeile), "G,%ld,%ld,%u\n", sicherRunden(grenzwert * 10.0f),
           sicherRunden(HYSTERESE_PROZENT * 10.0f), (unsigned)MOTOR_STAERKE);
  zeileSenden(zeile);
#endif
}

// Meldungen direkt nach dem Start.
void startMeldungen() {
  einstellungenSenden();
#if !GUI_MODUS
  zeileSenden("JointSense Prototyp 2 (Flex) gestartet.\n");
#endif
  if (isnan(GRENZWERT_PROZENT) || grenzwert != GRENZWERT_PROZENT) {
    char text[64];
    snprintf(text, sizeof(text), "Grenzwert begrenzt auf %ld Prozent", sicherRunden(grenzwert));
    ereignisSenden(text);
  }
  if (motorAusgefallen) {
    ereignisSenden("Motor-Ausfall - DRV2605L antwortet nicht");
  }
}

// Wird der PC neu verbunden (z. B. GUI öffnet den Port), schicken wir die G-Zeile noch einmal.
// Sonst würde das GUI die Einstellungen nie sehen, weil die erste G-Zeile beim Start verloren geht.
void usbVerbindungPruefen() {
  bool verbunden = (bool)Serial;
  if (verbunden && !usbWarVerbunden) {
    einstellungenSenden();
  }
  usbWarVerbunden = verbunden;
}

// Einmal pro Sekunde: Fehler-Meldungen wiederholen (AK-4, AK-10, AK-11) und Motor prüfen.
void sekundenAufgaben(uint32_t jetzt) {
  (void)jetzt;
  motorTreiberPruefen();

  if (sensorAusgefallen) {
    ereignisSenden("Sensor-Ausfall - Motor aus");
  }
  if (motorAusgefallen) {
    ereignisSenden("Motor-Ausfall - DRV2605L antwortet nicht");
  }
  if (!(referenzOhm > 0.0f) && !einmessenLaeuft) {
    ereignisSenden("Keine Referenz - Taste oder Enter zum Einmessen");
  }

#if !GUI_MODUS
  // Text-Modus: einmal pro Sekunde eine Übersicht für den Seriellen Monitor
  static const char *const zustandsTexte[] = {"Keine Referenz", "Bereit", "Einmessen",
                                              "Sensor-Ausfall", "Motor-Ausfall"};
  uint8_t zustand = zustandBestimmen();
  const char *zustandText = (zustand < 5) ? zustandsTexte[zustand] : "?";
  int adc = flexAdcLesen();
  float widerstand = flexWiderstandOhm(adc);
  long zehntel = 0;
  if (flexWertGueltig(adc) && referenzOhm > 0.0f) {
    zehntel = sicherRunden(abweichungProzent(widerstand, referenzOhm) * 10.0f);
  }
  long betrag = (zehntel < 0) ? -zehntel : zehntel;
  char zeile[120];
  snprintf(zeile, sizeof(zeile), "ADC %d | Widerstand %ld Ohm | Abweichung %s%ld.%ld %% | Motor %s | %s\n",
           adc, flexWertGueltig(adc) ? sicherRunden(widerstand) : 0L, (zehntel < 0) ? "-" : "",
           betrag / 10, betrag % 10, motorAn ? "an" : "aus", zustandText);
  zeileSenden(zeile);
#endif
}
