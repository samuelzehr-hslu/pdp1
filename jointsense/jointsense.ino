// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Alle Pin-Nummern und I2C-Adressen stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").
// Die reine Logik (Winkel, Grenzwert, Taste entprellen) steht in logik.h / logik.cpp.
// Diese Datei hier macht nur den Ablauf und redet mit der Hardware.
//
// So funktioniert es:
//   1. Zwei IMUs (Oberarm und Unterarm) messen je ihre Neigung.
//   2. Ellbogenwinkel = Neigung Unterarm - Neigung Oberarm.
//   3. Einmessen: Arm strecken, Taste kurz drücken -> dieser Winkel ist die Referenz (0 Grad).
//   4. Ist der Arm mehr als GRENZWERT_GRAD über der Referenz -> Motor vibriert.
//
// Testen ohne Hardware: Wokwi-Simulation, siehe diagram.json im selben Ordner.

#include <Wire.h>
#include <math.h>
#include "pins.h"
#include "logik.h"

// ---------- Einstellungen (hier dürft ihr Werte ändern) ----------
const float GRENZWERT_GRAD   = 5.0; // ab so vielen Grad Überstreckung wird gewarnt (QA-03: 0 bis 10)
const float HYSTERESE_GRAD   = 2.0;  // Motor geht erst wieder aus, wenn 2 Grad unter dem Grenzwert
const unsigned long ENTPRELL_MS = 80;  // so lange muss die Überstreckung anliegen (README: 50-100 ms)
const unsigned long MESS_PAUSE_MS = 20; // alle 20 ms messen = 50 Hz (README: 50-100 Hz)
const int MOTOR_STAERKE      = 200;  // PWM-Wert 0 (aus) bis 255 (volle Kraft)
const int EINMESS_ANZAHL     = 25;   // so viele Messungen werden beim Einmessen gemittelt
const unsigned long TASTER_PRELL_MS = 50; // so lange "prellt" die Taste nach einer Änderung

// ---------- Variablen, die sich während dem Betrieb ändern ----------
float referenzWinkel = 0.0;          // Winkel bei gestrecktem Arm (wird beim Einmessen gesetzt)
GrenzwertZustand grenzZustand = {false, 0}; // Motor aus, Arm nicht zu weit (siehe logik.h)
TasteZustand tasteZustand = {false, 0};     // Taste nicht gedrückt (siehe logik.h)
unsigned long letzteMessung = 0;     // wann haben wir zuletzt gemessen?
unsigned long letzteAusgabe = 0;     // wann haben wir zuletzt etwas auf den Monitor geschrieben?


// Läuft einmal beim Start.
void setup() {
  Serial.begin(9600);
  Wire.begin();                       // I2C starten (SDA = A4, SCL = A5)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  digitalWrite(PIN_MOTOR_IN2, LOW);   // IN2 bleibt immer LOW
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus

  pinMode(PIN_TASTER, INPUT_PULLUP);  // Taste gegen GND, nicht gedrückt = HIGH

  // Beide IMUs aufwecken und melden, ob sie antworten
  if (imuStarten(IMU1_ADRESSE)) {
    Serial.println("IMU 1 (Oberarm): OK");
  } else {
    Serial.println("IMU 1 (Oberarm): FEHLER - Verkabelung pruefen");
  }
  if (imuStarten(IMU2_ADRESSE)) {
    Serial.println("IMU 2 (Unterarm): OK");
  } else {
    Serial.println("IMU 2 (Unterarm): FEHLER - Verkabelung pruefen");
  }

  einmessen();
  Serial.println("Zum neu Einmessen: Arm strecken und Taste kurz druecken.");
}


// Läuft immer wieder, solange der Arduino Strom hat.
void loop() {
  if (tasteGedrueckt()) {
    einmessen();
  }

  // Nur alle MESS_PAUSE_MS messen, sonst gleich wieder von vorne
  if (millis() - letzteMessung < MESS_PAUSE_MS) {
    return;
  }
  letzteMessung = millis();

  float winkel = winkelLesen();
  float abweichung = winkel - referenzWinkel;
  bool zuWeit = grenzwertPruefen(grenzZustand, abweichung, millis(),
                                 GRENZWERT_GRAD, HYSTERESE_GRAD, ENTPRELL_MS);
  feedbackAusgeben(zuWeit);

  // 4x pro Sekunde den aktuellen Wert auf den Seriellen Monitor schreiben
  if (millis() - letzteAusgabe >= 250) {
    letzteAusgabe = millis();
    Serial.print("Abweichung: ");
    Serial.print(abweichung, 1);
    Serial.print(" Grad");
    if (zuWeit) {
      Serial.print("  -> VIBRATION");
    }
    Serial.println();
  }
}


// =====================================================================
// SENSOR: IMUs auslesen (bei anderem IMU-Typ nur diesen Teil ändern)
// =====================================================================

// Weckt eine IMU (MPU6050) auf. Gibt true zurück, wenn sie antwortet.
bool imuStarten(int adresse) {
  Wire.beginTransmission(adresse);
  Wire.write(0x6B);                   // Register "PWR_MGMT_1"
  Wire.write(0x00);                   // 0 = Schlafmodus aus
  int fehler = Wire.endTransmission();
  return fehler == 0;
}

// Liest die Neigung einer IMU in Grad (nur aus der Erdbeschleunigung).
// Achtung: Stimmt nur, wenn der Arm sich nicht schnell bewegt.
// Später: Gyroskop dazunehmen (Komplementärfilter, siehe README).
float neigungLesen(int adresse) {
  Wire.beginTransmission(adresse);
  Wire.write(0x3B);                   // Register "ACCEL_XOUT_H" = erster Beschleunigungswert
  Wire.endTransmission(false);
  Wire.requestFrom(adresse, 6);       // 6 Bytes: X, Y, Z (je 2 Bytes)

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  // Welche Achse die richtige ist, hängt davon ab, wie die IMU am Arm sitzt!
  // Die Rechnung selbst steht in logik.cpp.
  return neigungAusBeschleunigung(ax, ay, az);
}

// Berechnet den Ellbogenwinkel aus den Daten beider IMUs.
float winkelLesen() {
  float oberarm  = neigungLesen(IMU1_ADRESSE);
  float unterarm = neigungLesen(IMU2_ADRESSE);
  return ellbogenWinkel(oberarm, unterarm);
}


// =====================================================================
// EINMESSEN
// =====================================================================

// Misst die Referenz: Mittelwert über mehrere Messungen bei gestrecktem Arm.
void einmessen() {
  Serial.println("Einmessen... Arm ruhig gestreckt halten.");
  float summe = 0.0;
  for (int i = 0; i < EINMESS_ANZAHL; i++) {
    summe = summe + winkelLesen();
    delay(MESS_PAUSE_MS);
  }
  referenzWinkel = referenzBerechnen(summe, EINMESS_ANZAHL);

  Serial.print("Referenz gesetzt: ");
  Serial.print(referenzWinkel, 1);
  Serial.println(" Grad");

  // Kurze Bestätigungsvibration (README: "Bedienung", Punkt 3)
  analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  delay(150);
  analogWrite(PIN_MOTOR_PWM, 0);
}


// =====================================================================
// AKTOR und BEDIENUNG
// =====================================================================

// Schaltet den Vibrationsmotor ein oder aus.
void feedbackAusgeben(bool zuWeit) {
  if (zuWeit) {
    analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  } else {
    analogWrite(PIN_MOTOR_PWM, 0);
  }
}

// Gibt true zurück, wenn die Taste gerade eben gedrückt wurde (nur einmal pro Druck).
// Die Entprell-Logik steht in logik.cpp (tasteNeuGedrueckt).
bool tasteGedrueckt() {
  bool istGedrueckt = (digitalRead(PIN_TASTER) == LOW);   // LOW = gedrückt
  return tasteNeuGedrueckt(tasteZustand, istGedrueckt, millis(), TASTER_PRELL_MS);
}
