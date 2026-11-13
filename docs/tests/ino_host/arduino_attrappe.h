// arduino_attrappe.h
// Nur für die Tests (REQ-007). Kein Produkt-Code.
// Eine "Attrappe" für die Arduino-Befehle (Serial, Wire, millis, Pins ...), damit man jointsense.ino
// auf dem PC übersetzen und laufen lassen kann. Alles, was die Firmware ausgibt, wird mit Zeit
// in eine Datei geschrieben. So kann man die Ausgabe VOR und NACH dem Aufräumen Zeile für Zeile vergleichen.
#ifndef ARDUINO_ATTRAPPE_H
#define ARDUINO_ATTRAPPE_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class __FlashStringHelper;
#define F(text) (reinterpret_cast<const __FlashStringHelper *>(text))
#define PROGMEM
#define PSTR(text) (text)
#define pgm_read_byte(adresse) (*(const uint8_t *)(adresse))
#define pgm_read_word(adresse) (*(const uint16_t *)(adresse))
#define pgm_read_float(adresse) (*(const float *)(adresse))

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define DEC 10
#define HEX 16
typedef bool boolean;
typedef uint8_t byte;

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(uint8_t pin, uint8_t modus);
void digitalWrite(uint8_t pin, uint8_t wert);
int digitalRead(uint8_t pin);
void analogWrite(uint8_t pin, int wert);

class SerialAttrappe {
 public:
  void begin(unsigned long baud);
  size_t print(const __FlashStringHelper *text);
  size_t print(const char *text);
  size_t print(char zeichen);
  size_t print(unsigned char zahl, int basis = 10);
  size_t print(int zahl, int basis = 10);
  size_t print(unsigned int zahl, int basis = 10);
  size_t print(long zahl, int basis = 10);
  size_t print(unsigned long zahl, int basis = 10);
  size_t print(double zahl, int stellen = 2);
  size_t println();
  size_t println(const __FlashStringHelper *text);
  size_t println(const char *text);
  size_t println(char zeichen);
  size_t println(unsigned char zahl, int basis = 10);
  size_t println(int zahl, int basis = 10);
  size_t println(unsigned int zahl, int basis = 10);
  size_t println(long zahl, int basis = 10);
  size_t println(unsigned long zahl, int basis = 10);
  size_t println(double zahl, int stellen = 2);
  size_t write(uint8_t zeichen);
  int available();
  int read();
  int peek();
  void flush();
};
extern SerialAttrappe Serial;

class WireAttrappe {
 public:
  void begin();
  void end();
  void setClock(uint32_t takt);
  void setWireTimeout(uint32_t zeit = 25000, bool zuruecksetzen = false);
  bool getWireTimeoutFlag();
  void clearWireTimeoutFlag();
  void beginTransmission(uint8_t adresse);
  void beginTransmission(int adresse);
  size_t write(uint8_t wert);
  uint8_t endTransmission(uint8_t stop);
  uint8_t endTransmission();
  uint8_t requestFrom(uint8_t adresse, uint8_t anzahl);
  uint8_t requestFrom(uint8_t adresse, uint8_t anzahl, uint8_t stop);
  uint8_t requestFrom(int adresse, int anzahl);
  int available();
  int read();
};
extern WireAttrappe Wire;

// ---- Steuerung durch das Testprogramm (nicht Teil von Arduino) ----
extern unsigned long h_ms;        // "jetzt" in Millisekunden
extern unsigned long h_wireUs;    // Zeit, die I2C-Zugriffe im laufenden Millisekunden-Schritt verbraucht haben
extern bool h_kabel[2];           // true = IMU 1 / IMU 2 antwortet
extern bool h_gyroAnschlag;       // true = IMU 2 meldet Gyro gx am Anschlag (32767)
extern bool h_taste;              // true = Taste gedrückt
void h_start(FILE *ausgabe, FILE *wireLog);
void h_ende();
void h_rxSchicken(const char *text);   // Zeichen, wie wenn jemand im Seriellen Monitor tippt
#endif
