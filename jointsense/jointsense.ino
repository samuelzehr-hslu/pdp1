// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Das ist nur das Grundgerüst mit Platzhaltern. Die Logik kommt später.
// Alle Pin-Nummern stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").

#include <Wire.h>
#include "pins.h"

// Läuft einmal beim Start.
void setup() {
  Serial.begin(9600);
  Wire.begin();                       // I2C starten (SDA = A4, SCL = A5)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);   // IN2 bleibt immer LOW
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus

  // TODO: beide IMUs starten (IMU1_ADRESSE, IMU2_ADRESSE)
  // TODO: Einmessen der Referenz (README: "Bedienung", Punkt 3)
}

// Läuft immer wieder, solange der Arduino Strom hat.
void loop() {
  float winkel = winkelLesen();
  bool zuWeit = grenzwertPruefen(winkel);
  feedbackAusgeben(zuWeit);
}

// Berechnet den Ellbogenwinkel aus den Daten beider IMUs.
float winkelLesen() {
  // TODO: IMUs auslesen, Sensorfusion (README: "Signalverarbeitung")
  return 0.0;
}

// Prüft, ob der Winkel über dem Grenzwert liegt (mit Entprellung und Hysterese).
bool grenzwertPruefen(float winkel) {
  // TODO: Grenzwert vergleichen (README: "Signalverarbeitung")
  return false;
}

// Schaltet den Vibrationsmotor ein oder aus.
void feedbackAusgeben(bool zuWeit) {
  // TODO: Motor per PWM ansteuern (PIN_MOTOR_PWM)
}
