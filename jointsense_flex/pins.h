// pins.h - Prototyp 2 (Flex-Sensor, Adafruit Feather RP2040)
// EINZIGE Stelle im Code mit Pin-Nummern und I2C-Adressen.
// 1:1 aus README.md, Abschnitt "Verdrahtung Prototyp 2 (Flex)".
// Ändert sich ein Kabel: zuerst README, dann diese Datei (im selben Commit).
//
// Hinweis: Beim Feather RP2040 (Board-Paket arduino-pico) sind die Pin-Nummern im Code
// die GPIO-Nummern des Chips. Darum steht hier z. B. 7 für den Anschluss "D5".

#ifndef PINS_H
#define PINS_H

// Flex-Sensor im Spannungsteiler: A0 = GPIO 26 (3V3 - Flex - A0 - 22 kOhm - GND)
#define PIN_FLEX 26

// I2C zum Haptik-Treiber DRV2605L: SDA = GPIO 2, SCL = GPIO 3
#define PIN_SDA 2
#define PIN_SCL 3

// Taste zum Einmessen: D5 = GPIO 7, andere Seite an GND (interner Pullup)
#define PIN_TASTE 7

// Status-LED: rote LED auf dem Board, D13 = GPIO 13
#define PIN_STATUS_LED 13

// I2C-Adresse des DRV2605L
#define I2C_ADRESSE_DRV2605L 0x5A

#endif
