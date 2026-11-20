// flex_stufen.ino - Stufen-Test für Prototyp 2 (Flex-Sensor, Adafruit Feather RP2040)
//
// Wozu? Schritt für Schritt prüfen, ob die Hardware geht. Jede Stufe bringt EIN neues Teil dazu:
//   Stufe 1  Sensor      : nur A0 lesen (ADC-Rohwert und Widerstand)
//   Stufe 2  Grenzwert   : + Referenz setzen, Abweichung in %, Grenzwert 5 % mit Hysterese
//   Stufe 3  Taste + LED : + Taste D5 setzt die Referenz, rote LED auf dem Board = "zu weit"
//   Stufe 4  Motor       : + DRV2605L, Motor vibriert bei "zu weit"
//   Stufe 5  Komplett    : das ist NICHT dieser Sketch, sondern jointsense_flex/jointsense_flex.ino
//
// Nur EINMAL hochladen. Die Stufe wechselt man im GUI (tools/gui_flex/jointsense_flex_live.html)
// oder im Seriellen Monitor (115200 Baud) mit den Zeichen 1, 2, 3 oder 4.
// Enter = Referenz setzen (ab Stufe 2). Nach dem Einschalten läuft Stufe 1.
//
// Absichtlich NICHT drin (das kommt erst in Stufe 5): Watchdog, automatisches Neu-Verbinden des
// Motors, Fehlermeldungen jede Sekunde. Fehler werden hier nur EINMAL gemeldet.
//
// Zeilen an den PC (gleiches Format wie jointsense_flex, darum versteht sie das gleiche GUI):
//   S,<stufe>                         welche Stufe läuft (beim Start, beim Verbinden, beim Wechsel)
//   G,<grenzwert x10>,<hysterese x10>,<motorstärke>
//   D,<zeit ms>,<adc>,<widerstand Ohm>,<abweichung x10>,<zu weit 0/1>,<zustand 0-4>   50 Mal pro Sekunde
//   E,<text>                          Ereignis
//
// Aufbau dieser Datei:
//   1. Einstellungen und Variablen
//   2. setup() und loop()
//   3. STUFE wechseln
//   4. MESSEN und REFERENZ
//   5. TASTE und SERIAL
//   6. MOTOR (DRV2605L)
//   7. MELDUNGEN an den PC

#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pins.h"
#include "stufen_logik.h"

// ---------- 1. Einstellungen (hier dürft ihr Werte ändern) ----------

const float GRENZWERT_PROZENT = 5.0f;   // ab so viel Abweichung ist es "zu weit"
const float HYSTERESE_PROZENT = 1.0f;   // "zu weit" endet erst unter Grenzwert - Hysterese
const uint32_t ENTPRELL_MS = 80;        // so lange muss die Abweichung über dem Grenzwert sein
const uint8_t MOTOR_STAERKE = 100;      // Stärke der Vibration 0 bis 127

// Feste Werte
const unsigned long SERIAL_BAUD = 115200;
const uint32_t MESS_INTERVALL_MS = 20;      // 50 Messungen pro Sekunde
const int ADC_MITTEL_ANZAHL = 4;            // so viele ADC-Messungen pro Zyklus mitteln
const int REFERENZ_ANZAHL = 50;             // Referenz = Mittelwert aus 50 Messungen (1 Sekunde)
const uint32_t TASTE_ENTPRELL_MS = 30;      // Taste muss 30 ms ruhig sein
const uint32_t I2C_ZEITLIMIT_MS = 10;       // höchstens 10 ms pro I2C-Befehl
const int SERIAL_MAX_PRO_LOOP = 64;         // höchstens so viele Zeichen pro loop() lesen

static_assert(MOTOR_STAERKE <= 127, "MOTOR_STAERKE muss 0 bis 127 sein");

// Zustandsnummern für die D-Zeile (gleich wie in jointsense_flex, damit das GUI sie kennt)
const uint8_t ZUSTAND_KEINE_REFERENZ = 0;
const uint8_t ZUSTAND_BEREIT = 1;
const uint8_t ZUSTAND_EINMESSEN = 2;     // hier: Referenz wird gerade gemessen
const uint8_t ZUSTAND_SENSOR_FEHLER = 3; // ADC-Wert ungültig
const uint8_t ZUSTAND_MOTOR_FEHLER = 4;  // nur Stufe 4: DRV2605L antwortet nicht

// Register des DRV2605L (aus dem Datenblatt von Texas Instruments)
const uint8_t DRV_REG_STATUS = 0x00;
const uint8_t DRV_REG_MODUS = 0x01;
const uint8_t DRV_REG_RTP = 0x02;        // Stärke im Echtzeit-Modus
const uint8_t DRV_REG_GO = 0x0C;
const uint8_t DRV_REG_FEEDBACK = 0x1A;   // Bit 7: 0 = ERM-Motor
const uint8_t DRV_REG_CONTROL3 = 0x1D;   // Bit 5: ERM ohne Regelung, Bit 3: Zahlenformat
const uint8_t DRV_MODUS_INTERN = 0x00;   // aktiv, spielt nichts
const uint8_t DRV_MODUS_RTP = 0x05;      // Echtzeit-Modus: Stärke = Register RTP
const uint8_t DRV_MODUS_STANDBY = 0x40;  // Standby: Motor sicher aus

// ---------- Variablen ----------
int stufe = 1;                     // welche Stufe gerade läuft (1 bis 4)

float referenzOhm = -1.0f;         // -1 = noch keine Referenz
bool referenzLaeuft = false;       // werden gerade Messungen für die Referenz gesammelt?
int referenzZaehler = 0;
float referenzSumme = 0.0f;

GrenzwertZustand grenzwertZustand; // aus stufen_logik.h
bool zuWeit = false;               // Ergebnis der Grenzwert-Prüfung
bool sensorFehler = false;         // war der letzte ADC-Wert ungültig?

bool motorOk = false;              // hat der DRV2605L geantwortet? (nur Stufe 4)
bool motorAn = false;              // was wir dem Motor zuletzt befohlen haben

bool tasteStabil = false;          // entprellter Zustand der Taste (true = gedrückt)
bool tasteLetzterWert = false;     // roher Wert beim letzten Lesen
uint32_t tasteWechselMs = 0;       // wann hat sich der rohe Wert zuletzt geändert?

uint32_t letzteMessungMs = 0;
bool usbWarVerbunden = false;

// ---------- 2. setup() und loop() ----------

void setup() {
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  pinMode(PIN_TASTE, INPUT_PULLUP);

  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  Wire.begin();
  Wire.setTimeout(I2C_ZEITLIMIT_MS, true);  // true = Bus nach Zeitüberschreitung zurücksetzen
  // Sicherheit: Falls der Motor von vorher noch vibriert, still ausschalten (keine Meldung).
  drvRegisterSchreiben(DRV_REG_RTP, 0);
  drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_STANDBY);

  Serial.begin(SERIAL_BAUD);   // nicht auf den PC warten
  analogReadResolution(12);    // 0 bis 4095

  letzteMessungMs = millis();
}

void loop() {
  uint32_t jetzt = millis();

  serialLesen();
  if (stufe >= 3) {
    tasteLesen(jetzt);
  }

  if (jetzt - letzteMessungMs >= MESS_INTERVALL_MS) {
    if (jetzt - letzteMessungMs > 5 * MESS_INTERVALL_MS) {
      letzteMessungMs = jetzt;               // weit im Rückstand: nicht nachholen
    } else {
      letzteMessungMs += MESS_INTERVALL_MS;  // fester Takt: im Mittel genau 50 pro Sekunde
    }
    messZyklus(jetzt);
  }

  usbVerbindungPruefen();
}

// ---------- 3. STUFE wechseln ----------

// Name der Stufe für die Meldungen.
const char *stufenName(int s) {
  if (s == 1) return "Sensor";
  if (s == 2) return "Grenzwert";
  if (s == 3) return "Taste + LED";
  if (s == 4) return "Motor";
  return "?";
}

// Wechselt in eine neue Stufe. Alles, was die alte Stufe angeschaltet hat, wird ausgeschaltet.
void stufeWechseln(int neueStufe) {
  if (neueStufe < 1 || neueStufe > 4) {
    return;
  }

  // Aufräumen: Motor aus, LED aus, Alarm zurücksetzen
  if (motorOk) {
    drvRegisterSchreiben(DRV_REG_RTP, 0);
    drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_STANDBY);
  }
  motorOk = false;
  motorAn = false;
  digitalWrite(PIN_STATUS_LED, LOW);
  grenzwertZustand = GrenzwertZustand{};
  zuWeit = false;
  referenzLaeuft = false;   // eine halbe Referenz-Messung wird verworfen
  // Die Referenz selbst bleibt erhalten, damit man nicht bei jeder Stufe neu einmessen muss.

  stufe = neueStufe;
  stufeSenden();

  char text[64];
  snprintf(text, sizeof(text), "Stufe %d - %s", stufe, stufenName(stufe));
  ereignisSenden(text);

  // Erst in Stufe 4 wird der Motor-Treiber gesucht.
  if (stufe == 4) {
    motorOk = motorTreiberEinrichten();
    if (motorOk) {
      ereignisSenden("DRV2605L gefunden - Motor bereit");
    } else {
      ereignisSenden("DRV2605L nicht gefunden - SDA SCL VIN GND pruefen");
    }
  }
}

// ---------- 4. MESSEN und REFERENZ ----------

// Liest A0 mehrmals und gibt den Mittelwert zurück (0 bis 4095).
int flexAdcLesen() {
  long summe = 0;
  for (int i = 0; i < ADC_MITTEL_ANZAHL; i++) {
    summe += analogRead(PIN_FLEX);
  }
  long mittel = (summe + ADC_MITTEL_ANZAHL / 2) / ADC_MITTEL_ANZAHL;
  if (mittel < 0) mittel = 0;
  if (mittel > ADC_MAX) mittel = ADC_MAX;
  return (int)mittel;
}

// Ein Messzyklus (alle 20 ms). Was passiert, hängt von der Stufe ab.
void messZyklus(uint32_t jetzt) {
  // --- Stufe 1 (immer): Sensor lesen ---
  int adc = flexAdcLesen();
  bool gueltig = adcGueltig(adc);
  float widerstand = widerstandOhm(adc);  // -1 bei ungültigem Wert

  // Ungültigen Wert nur beim Wechsel melden, nicht dauernd.
  if (!gueltig && !sensorFehler) {
    ereignisSenden("Sensorwert ungueltig - Kabel zum Flex pruefen");
  } else if (gueltig && sensorFehler) {
    ereignisSenden("Sensorwert wieder ok");
  }
  sensorFehler = !gueltig;

  // --- ab Stufe 2: Referenz und Grenzwert ---
  float abweichung = 0.0f;
  zuWeit = false;
  if (stufe >= 2) {
    if (referenzLaeuft) {
      referenzWeiter(widerstand, gueltig);
    }
    bool referenzDa = referenzOhm > 0.0f;
    if (gueltig && referenzDa && !referenzLaeuft) {
      abweichung = abweichungProzent(widerstand, referenzOhm);
      zuWeit = grenzwertPruefen(grenzwertZustand, abweichung, jetzt,
                                GRENZWERT_PROZENT, HYSTERESE_PROZENT, ENTPRELL_MS);
    } else {
      grenzwertZustand = GrenzwertZustand{};  // ohne gültige Messung kein Alarm
    }
  }

  // --- ab Stufe 3: LED auf dem Board zeigt "zu weit" ---
  if (stufe >= 3) {
    digitalWrite(PIN_STATUS_LED, zuWeit ? HIGH : LOW);
  }

  // --- Stufe 4: Motor vibriert bei "zu weit" ---
  if (stufe >= 4) {
    motorSetzen(zuWeit);
  }

  datenzeileSenden(jetzt, adc, gueltig ? widerstand : 0.0f, abweichung);
}

// Startet die Referenz-Messung (ab Stufe 2): 1 Sekunde lang Werte sammeln.
void referenzStarten() {
  if (stufe < 2 || referenzLaeuft) {
    return;
  }
  referenzLaeuft = true;
  referenzZaehler = 0;
  referenzSumme = 0.0f;
  grenzwertZustand = GrenzwertZustand{};
  ereignisSenden("Referenz wird gemessen - Arm 1 s ruhig halten");
}

// Sammelt einen Wert für die Referenz. Nach 50 Werten ist die Referenz fertig.
void referenzWeiter(float widerstand, bool gueltig) {
  if (!gueltig) {
    referenzLaeuft = false;
    ereignisSenden("Referenz abgebrochen - Sensorwert ungueltig");
    return;
  }
  referenzSumme += widerstand;
  referenzZaehler++;
  if (referenzZaehler >= REFERENZ_ANZAHL) {
    referenzOhm = referenzSumme / REFERENZ_ANZAHL;
    referenzLaeuft = false;
    char text[64];
    snprintf(text, sizeof(text), "Referenz gesetzt - %ld Ohm", lroundf(referenzOhm));
    ereignisSenden(text);
  }
}

// Welcher Zustand gilt gerade? (Nummer für die D-Zeile)
uint8_t zustandBestimmen() {
  if (sensorFehler) return ZUSTAND_SENSOR_FEHLER;
  if (stufe >= 4 && !motorOk) return ZUSTAND_MOTOR_FEHLER;
  if (referenzLaeuft) return ZUSTAND_EINMESSEN;
  if (!(referenzOhm > 0.0f)) return ZUSTAND_KEINE_REFERENZ;
  return ZUSTAND_BEREIT;
}

// ---------- 5. TASTE und SERIAL ----------

// Liest die Taste (gedrückt = LOW wegen Pullup). Ein neuer Druck startet die Referenz.
void tasteLesen(uint32_t jetzt) {
  bool gedrueckt = digitalRead(PIN_TASTE) == LOW;
  if (gedrueckt != tasteLetzterWert) {
    tasteLetzterWert = gedrueckt;   // Wert hat sich geändert: Stoppuhr neu starten
    tasteWechselMs = jetzt;
    return;
  }
  // Wert ist gleich geblieben. Lange genug ruhig und anders als vorher? Dann zählt er.
  if (jetzt - tasteWechselMs >= TASTE_ENTPRELL_MS && gedrueckt != tasteStabil) {
    tasteStabil = gedrueckt;
    if (tasteStabil) {
      ereignisSenden("Taste gedrueckt");
      referenzStarten();
    }
  }
}

// Liest Zeichen vom PC: '1' bis '4' = Stufe wechseln, Enter = Referenz setzen.
void serialLesen() {
  int gelesen = 0;
  while (gelesen < SERIAL_MAX_PRO_LOOP && Serial.available() > 0) {
    int zeichen = Serial.read();
    gelesen++;
    if (zeichen >= '1' && zeichen <= '4') {
      stufeWechseln(zeichen - '0');   // '3' - '0' = 3
    } else if (zeichen == '\n') {
      referenzStarten();
    }
    // alle anderen Zeichen (auch '\r') werden ignoriert
  }
}

// ---------- 6. MOTOR (DRV2605L über I2C) ----------

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

// Sucht den DRV2605L und stellt ihn für einen ERM-Motor im Echtzeit-Modus ein (gleich wie
// in jointsense_flex). true = alles hat geklappt.
bool motorTreiberEinrichten() {
  bool ok = drvRegisterSchreiben(DRV_REG_RTP, 0);
  ok = drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_INTERN) && ok;
  ok = drvRegisterSchreiben(DRV_REG_GO, 0) && ok;

  uint8_t status = 0;
  if (!drvRegisterLesen(DRV_REG_STATUS, status)) {
    return false;  // Chip antwortet nicht
  }

  uint8_t feedback = 0;
  ok = drvRegisterLesen(DRV_REG_FEEDBACK, feedback) && ok;
  ok = drvRegisterSchreiben(DRV_REG_FEEDBACK, (uint8_t)(feedback & 0x7F)) && ok;  // ERM

  uint8_t control3 = 0;
  ok = drvRegisterLesen(DRV_REG_CONTROL3, control3) && ok;
  ok = drvRegisterSchreiben(DRV_REG_CONTROL3, (uint8_t)((control3 | 0x20) & ~0x08)) && ok;

  ok = drvRegisterSchreiben(DRV_REG_RTP, 0) && ok;
  ok = drvRegisterSchreiben(DRV_REG_MODUS, DRV_MODUS_RTP) && ok;
  return ok;
}

// Schaltet den Motor an oder aus. Schreibt nur, wenn sich etwas ändert (weniger I2C-Verkehr).
void motorSetzen(bool an) {
  if (!motorOk || an == motorAn) {
    return;
  }
  if (drvRegisterSchreiben(DRV_REG_RTP, an ? MOTOR_STAERKE : 0)) {
    motorAn = an;
  } else {
    // Einmal melden, dann Ruhe. Neu versuchen: im GUI nochmals "Stufe 4" drücken.
    motorOk = false;
    motorAn = false;
    ereignisSenden("DRV2605L antwortet nicht mehr - Stufe 4 neu waehlen");
  }
}

// ---------- 7. MELDUNGEN an den PC ----------

// Schickt eine Zeile nur, wenn im USB-Puffer Platz ist. So hängt das Programm nie,
// wenn der PC nicht mitliest.
void zeileSenden(const char *text) {
  size_t laenge = strlen(text);
  if (Serial.availableForWrite() < (int)laenge) {
    return;
  }
  Serial.write((const uint8_t *)text, laenge);
}

// Ereignis melden: "E,<text>". Text ohne Komma, höchstens 60 Zeichen.
void ereignisSenden(const char *text) {
  char zeile[72];
  snprintf(zeile, sizeof(zeile), "E,%.60s\n", text);
  zeileSenden(zeile);
}

// Begrenzt eine Zahl, bevor sie in eine ganze Zahl umgewandelt wird (kein Überlauf).
long sicherRunden(float wert) {
  if (isnan(wert)) return 0;
  if (wert > 999999999.0f) return 999999999L;
  if (wert < -999999999.0f) return -999999999L;
  return lroundf(wert);
}

// D-Zeile: eine pro Messzyklus. Das Feld "zu weit" zeigt den Alarm (in Stufe 4 = Motor an).
void datenzeileSenden(uint32_t jetzt, int adc, float widerstand, float abweichung) {
  char zeile[80];
  snprintf(zeile, sizeof(zeile), "D,%lu,%d,%ld,%ld,%d,%u\n",
           (unsigned long)jetzt, adc, sicherRunden(widerstand), sicherRunden(abweichung * 10.0f),
           zuWeit ? 1 : 0, (unsigned)zustandBestimmen());
  zeileSenden(zeile);
}

// S-Zeile: welche Stufe läuft. G-Zeile: Grenzwert, Hysterese, Motorstärke.
void stufeSenden() {
  char zeile[48];
  snprintf(zeile, sizeof(zeile), "S,%d\n", stufe);
  zeileSenden(zeile);
  snprintf(zeile, sizeof(zeile), "G,%ld,%ld,%u\n", sicherRunden(GRENZWERT_PROZENT * 10.0f),
           sicherRunden(HYSTERESE_PROZENT * 10.0f), (unsigned)MOTOR_STAERKE);
  zeileSenden(zeile);
}

// Öffnet der PC den Port (z. B. GUI verbindet), schicken wir Stufe und Einstellungen.
// Sonst würde das GUI nie erfahren, welche Stufe läuft.
void usbVerbindungPruefen() {
  bool verbunden = (bool)Serial;
  if (verbunden && !usbWarVerbunden) {
    stufeSenden();
    char text[64];
    snprintf(text, sizeof(text), "Stufen-Test bereit - Stufe %d - %s", stufe, stufenName(stufe));
    ereignisSenden(text);
  }
  usbWarVerbunden = verbunden;
}
