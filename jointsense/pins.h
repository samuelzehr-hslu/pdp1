// pins.h
// Hier stehen ALLE Pin-Nummern und I2C-Adressen des Projekts.
// Quelle: README.md, Abschnitt "Verdrahtung (Schaltplan Rev. B)".
// Regel: Wenn sich ein Kabel ändert, zuerst die README anpassen, danach diese Datei
// (beides im selben Commit). Im restlichen Code nur die Namen verwenden, nie Zahlen.

#ifndef PINS_H
#define PINS_H

// --- Motortreiber / Vibrationsmotor ---
const int PIN_MOTOR_PWM = 9;   // D9 (PWM) -> Treiber IN1
const int PIN_MOTOR_IN2 = 8;   // D8 -> Treiber IN2 (im Code dauerhaft LOW)

// --- IMUs (gemeinsamer I2C-Bus) ---
// SDA = A4 und SCL = A5 beim Uno/Nano. Die Pins nutzt die Bibliothek Wire automatisch,
// wir müssen sie nicht selbst setzen.
const int IMU1_ADRESSE = 0x68; // Oberarm, AD0 an GND
const int IMU2_ADRESSE = 0x69; // Unterarm, AD0 an 3V3

// --- Taste (Einmessen) ---
// VORLÄUFIG: D2 wurde für die Wokwi-Simulation gewählt. Team muss das noch bestätigen.
const int PIN_TASTER = 2;      // D2 -> Taste -> GND (interner Pull-up, gedrückt = LOW)

// --- Noch offen (steht in der README unter "Bedienung", aber nicht in der Verdrahtung) ---
// TODO: Pin für die Status-LED festlegen und in der README-Verdrahtung eintragen

// --- Offene Fragen an das Team ---
// TODO: IMU-Typ festlegen. Die Adressen 0x68/0x69 (über AD0) passen zu MPU6050/MPU6500,
//       nicht zum BNO085 (dieser hat die Adressen 0x4A/0x4B).
// TODO: Motortreiber-Typ festlegen.

#endif
