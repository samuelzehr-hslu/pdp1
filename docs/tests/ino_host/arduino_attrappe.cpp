// arduino_attrappe.cpp: Inhalt der Attrappen (siehe arduino_attrappe.h). Nur für die Tests.
#include "arduino_attrappe.h"
#include "pins.h"
#include "sensor_tabelle.h"

SerialAttrappe Serial;
WireAttrappe Wire;

unsigned long h_ms = 0;
unsigned long h_wireUs = 0;
bool h_kabel[2] = {true, true};
bool h_gyroAnschlag = false;
bool h_taste = false;

static FILE *s_aus = NULL;
static FILE *s_wire = NULL;
static char s_zeile[600];
static int s_zeilenLaenge = 0;
static char s_rx[64];
static int s_rxAnzahl = 0;
static int s_rxLesen = 0;
static int s_pinWert[40];
static int s_pinModus[40];

void h_start(FILE *ausgabe, FILE *wireLog) {
  s_aus = ausgabe;
  s_wire = wireLog;
  for (int i = 0; i < 40; i++) {
    s_pinWert[i] = -1;
    s_pinModus[i] = -1;
  }
}

static void zeileFertig() {
  s_zeile[s_zeilenLaenge] = 0;
  fprintf(s_aus, "S,%lu,%s\n", h_ms, s_zeile);
  s_zeilenLaenge = 0;
}

static void zeichenAus(char c) {
  if (c == '\r') return;
  if (c == '\n') {
    zeileFertig();
    return;
  }
  if (s_zeilenLaenge < (int)sizeof(s_zeile) - 1) s_zeile[s_zeilenLaenge++] = c;
}

static size_t textAus(const char *t) {
  size_t n = 0;
  while (t[n] != 0) {
    zeichenAus(t[n]);
    n++;
  }
  return n;
}

void h_ende() {
  if (s_zeilenLaenge > 0) {
    s_zeile[s_zeilenLaenge] = 0;
    fprintf(s_aus, "S,%lu,%s<<KEIN-ZEILENENDE>>\n", h_ms, s_zeile);
    s_zeilenLaenge = 0;
  }
}

void h_rxSchicken(const char *text) {
  // Der Empfangspuffer des Arduino ist 64 Byte gross. Was nicht passt, geht verloren.
  for (int i = 0; text[i] != 0; i++) {
    if (s_rxAnzahl - s_rxLesen < 64) {
      s_rx[s_rxAnzahl % 64] = text[i];
      s_rxAnzahl++;
    }
  }
}

unsigned long millis() { return h_ms; }
unsigned long micros() { return h_ms * 1000UL + h_wireUs; }
void delay(unsigned long ms) { h_ms += ms; }
void delayMicroseconds(unsigned int us) { h_wireUs += us; }

void pinMode(uint8_t pin, uint8_t modus) {
  if (pin < 40 && s_pinModus[pin] != modus) {
    s_pinModus[pin] = modus;
    fprintf(s_aus, "PM,%lu,%d,%d\n", h_ms, pin, modus);
  }
}
void digitalWrite(uint8_t pin, uint8_t wert) {
  if (pin < 40 && s_pinWert[pin] != wert) {
    s_pinWert[pin] = wert;
    fprintf(s_aus, "DW,%lu,%d,%d\n", h_ms, pin, wert);
  }
}
int digitalRead(uint8_t pin) {
  if (pin == PIN_TASTER) return h_taste ? LOW : HIGH;
  return HIGH;
}
void analogWrite(uint8_t pin, int wert) {
  if (pin < 40 && s_pinWert[pin] != wert + 1000) {
    s_pinWert[pin] = wert + 1000;
    fprintf(s_aus, "AW,%lu,%d,%d\n", h_ms, pin, wert);
  }
}

// ---- Serial ----
void SerialAttrappe::begin(unsigned long baud) { fprintf(s_aus, "SB,%lu,%lu\n", h_ms, baud); }
size_t SerialAttrappe::print(const __FlashStringHelper *text) { return textAus(reinterpret_cast<const char *>(text)); }
size_t SerialAttrappe::print(const char *text) { return textAus(text); }
size_t SerialAttrappe::print(char zeichen) { zeichenAus(zeichen); return 1; }
static size_t zahlAus(long long z, bool ohneVorzeichen, int basis) {
  char puffer[40];
  if (basis == 16) snprintf(puffer, sizeof(puffer), "%llX", (unsigned long long)z);
  else if (ohneVorzeichen) snprintf(puffer, sizeof(puffer), "%llu", (unsigned long long)z);
  else snprintf(puffer, sizeof(puffer), "%lld", z);
  return textAus(puffer);
}
size_t SerialAttrappe::print(unsigned char z, int b) { return zahlAus(z, true, b); }
size_t SerialAttrappe::print(int z, int b) { return zahlAus(z, false, b); }
size_t SerialAttrappe::print(unsigned int z, int b) { return zahlAus(z, true, b); }
size_t SerialAttrappe::print(long z, int b) { return zahlAus(z, false, b); }
size_t SerialAttrappe::print(unsigned long z, int b) { return zahlAus(z, true, b); }
size_t SerialAttrappe::print(double z, int stellen) {
  char puffer[60];
  snprintf(puffer, sizeof(puffer), "%.*f", stellen, z);
  return textAus(puffer);
}
size_t SerialAttrappe::println() { zeichenAus('\n'); return 2; }
size_t SerialAttrappe::println(const __FlashStringHelper *t) { size_t n = print(t); println(); return n + 2; }
size_t SerialAttrappe::println(const char *t) { size_t n = print(t); println(); return n + 2; }
size_t SerialAttrappe::println(char c) { size_t n = print(c); println(); return n + 2; }
size_t SerialAttrappe::println(unsigned char z, int b) { size_t n = print(z, b); println(); return n + 2; }
size_t SerialAttrappe::println(int z, int b) { size_t n = print(z, b); println(); return n + 2; }
size_t SerialAttrappe::println(unsigned int z, int b) { size_t n = print(z, b); println(); return n + 2; }
size_t SerialAttrappe::println(long z, int b) { size_t n = print(z, b); println(); return n + 2; }
size_t SerialAttrappe::println(unsigned long z, int b) { size_t n = print(z, b); println(); return n + 2; }
size_t SerialAttrappe::println(double z, int s) { size_t n = print(z, s); println(); return n + 2; }
size_t SerialAttrappe::write(uint8_t zeichen) { zeichenAus((char)zeichen); return 1; }
int SerialAttrappe::available() { return s_rxAnzahl - s_rxLesen; }
int SerialAttrappe::read() {
  if (s_rxLesen >= s_rxAnzahl) return -1;
  int c = (unsigned char)s_rx[s_rxLesen % 64];
  s_rxLesen++;
  return c;
}
int SerialAttrappe::peek() { return (s_rxLesen >= s_rxAnzahl) ? -1 : (unsigned char)s_rx[s_rxLesen % 64]; }
void SerialAttrappe::flush() {}

// ---- Wire: zwei MPU6050 ----
static int w_adresse = 0;
static uint8_t w_bytes[40];
static int w_anzahl = 0;
static uint8_t w_rx[16];
static int w_rxAnzahl = 0;
static int w_rxLesen = 0;
static int w_register = 0;

static int imuNummer(int adresse) {
  if (adresse == IMU1_ADRESSE) return 0;
  if (adresse == IMU2_ADRESSE) return 1;
  return -1;
}

void WireAttrappe::begin() { fprintf(s_wire, "WB,%lu\n", h_ms); }
void WireAttrappe::end() {}
void WireAttrappe::setClock(uint32_t takt) { fprintf(s_wire, "WC,%lu,%lu\n", h_ms, (unsigned long)takt); }
void WireAttrappe::setWireTimeout(uint32_t zeit, bool z) { fprintf(s_wire, "WT,%lu,%lu,%d\n", h_ms, (unsigned long)zeit, (int)z); }
bool WireAttrappe::getWireTimeoutFlag() { return false; }
void WireAttrappe::clearWireTimeoutFlag() {}
void WireAttrappe::beginTransmission(uint8_t adresse) { w_adresse = adresse; w_anzahl = 0; }
void WireAttrappe::beginTransmission(int adresse) { beginTransmission((uint8_t)adresse); }
size_t WireAttrappe::write(uint8_t wert) {
  if (w_anzahl < 40) w_bytes[w_anzahl++] = wert;
  return 1;
}
uint8_t WireAttrappe::endTransmission(uint8_t stop) {
  int n = imuNummer(w_adresse);
  uint8_t rc = (n >= 0 && h_kabel[n]) ? 0 : 2;   // 2 = keine Antwort (Adresse nicht bestätigt)
  if (rc == 0 && w_anzahl > 0) w_register = w_bytes[0];
  h_wireUs += 150;
  fprintf(s_wire, "T,%lu,%d,", h_ms, w_adresse);
  for (int i = 0; i < w_anzahl; i++) fprintf(s_wire, "%02X", w_bytes[i]);
  fprintf(s_wire, ",stop=%d,rc=%d\n", (int)stop, rc);
  return rc;
}
uint8_t WireAttrappe::endTransmission() { return endTransmission(1); }
uint8_t WireAttrappe::requestFrom(uint8_t adresse, uint8_t anzahl) {
  int n = imuNummer(adresse);
  w_rxAnzahl = 0;
  w_rxLesen = 0;
  h_wireUs += 350;
  if (n >= 0 && h_kabel[n] && w_register == 0x3B && anzahl <= 14) {
    int16_t roh[6];
    sensorRoh(n, h_ms, roh);
    if (n == 1 && h_gyroAnschlag) roh[3] = 32767;
    int16_t feld[7] = {roh[0], roh[1], roh[2], 0, roh[3], roh[4], roh[5]};   // wie im MPU6050: Acc, Temp, Gyro
    for (int i = 0; i < 7; i++) {
      w_rx[2 * i] = (uint8_t)(((uint16_t)feld[i]) >> 8);
      w_rx[2 * i + 1] = (uint8_t)(((uint16_t)feld[i]) & 0xFF);
    }
    w_rxAnzahl = anzahl;
  } else if (n >= 0 && h_kabel[n]) {
    w_rxAnzahl = (anzahl <= 16) ? anzahl : 16;   // andere Register: Nullen
    memset(w_rx, 0, sizeof(w_rx));
  }
  fprintf(s_wire, "R,%lu,%d,%d,got=%d\n", h_ms, adresse, anzahl, w_rxAnzahl);
  return (uint8_t)w_rxAnzahl;
}
uint8_t WireAttrappe::requestFrom(uint8_t a, uint8_t n, uint8_t) { return requestFrom(a, n); }
uint8_t WireAttrappe::requestFrom(int a, int n) { return requestFrom((uint8_t)a, (uint8_t)n); }
int WireAttrappe::available() { return w_rxAnzahl - w_rxLesen; }
int WireAttrappe::read() { return (w_rxLesen < w_rxAnzahl) ? w_rx[w_rxLesen++] : -1; }
