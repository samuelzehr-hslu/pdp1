// JointSense - Überstreckung des Ellbogens erkennen und per Vibration melden.
// Alle Pin-Nummern und I2C-Adressen stehen in pins.h (Quelle: README, Abschnitt "Verdrahtung").
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

// ---------- Einstellungen (hier dürft ihr Werte ändern) ----------
const float GRENZWERT_GRAD   = 10.0; // ab so vielen Grad Überstreckung wird gewarnt (QA-03: 0 bis 10)
const float HYSTERESE_GRAD   = 2.0;  // Motor geht erst wieder aus, wenn 2 Grad unter dem Grenzwert
const unsigned long ENTPRELL_MS = 80;  // so lange muss die Überstreckung anliegen (README: 50-100 ms)
const unsigned long MESS_PAUSE_MS = 20; // alle 20 ms messen = 50 Hz (README: 50-100 Hz)
// Motorstärke als PWM-Wert: 0 = aus, 255 = immer an (volle 3 V vom Spannungsregler).
// Der Motor braucht mindestens 2,5 V. PWM verteilt die 3 V nur über die Zeit:
// Durchschnitt = 3 V * MOTOR_STAERKE / 255. Unter ca. 215 sind das weniger als 2,5 V,
// dann läuft der Motor evtl. nicht mehr sicher an. Deshalb nicht unter 215 gehen.
const int MOTOR_STAERKE      = 255;
const int EINMESS_ANZAHL     = 25;   // so viele Messungen werden beim Einmessen gemittelt

// ---------- Variablen, die sich während dem Betrieb ändern ----------
float referenzWinkel = 0.0;          // Winkel bei gestrecktem Arm (wird beim Einmessen gesetzt)
bool motorLaeuft = false;            // läuft der Motor gerade?
unsigned long zuWeitSeit = 0;        // seit wann ist der Arm zu weit? (0 = gerade nicht zu weit)
unsigned long letzteMessung = 0;     // wann haben wir zuletzt gemessen?
unsigned long letzteAusgabe = 0;     // wann haben wir zuletzt etwas auf den Monitor geschrieben?
int alterTasterZustand = HIGH;       // Zustand der Taste beim letzten Durchlauf
unsigned long letzteTasterAenderung = 0;


// Läuft einmal beim Start.
void setup() {
  Serial.begin(9600);
  Wire.begin();                       // I2C starten (SDA = A4, SCL = A5)

  pinMode(PIN_MOTOR_PWM, OUTPUT);
  analogWrite(PIN_MOTOR_PWM, 0);      // Motor am Anfang aus (MOSFET sperrt)

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
  bool zuWeit = grenzwertPruefen(abweichung);
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
  float x = ax;
  float y = ay;
  float z = az;
  float neigung = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;
  return neigung;
}

// Berechnet den Ellbogenwinkel aus den Daten beider IMUs.
float winkelLesen() {
  float oberarm  = neigungLesen(IMU1_ADRESSE);
  float unterarm = neigungLesen(IMU2_ADRESSE);
  return unterarm - oberarm;
}


// =====================================================================
// LOGIK: Grenzwert prüfen (hängt nicht von der Hardware ab)
// =====================================================================

// Prüft, ob der Winkel über dem Grenzwert liegt (mit Entprellung und Hysterese).
// abweichung = wie viele Grad der Arm über der eingemessenen Referenz ist.
bool grenzwertPruefen(float abweichung) {
  // Hysterese: Läuft der Motor schon, gilt eine etwas tiefere Grenze.
  // So flackert der Motor nicht, wenn der Arm genau an der Grenze ist.
  float grenze = GRENZWERT_GRAD;
  if (motorLaeuft) {
    grenze = GRENZWERT_GRAD - HYSTERESE_GRAD;
  }

  if (abweichung <= grenze) {
    zuWeitSeit = 0;                   // Arm ist im erlaubten Bereich
    return false;
  }

  // Arm ist zu weit. Seit wann?
  if (zuWeitSeit == 0) {
    zuWeitSeit = millis();
  }

  // Entprellung: Erst warnen, wenn es lange genug zu weit ist
  if (millis() - zuWeitSeit >= ENTPRELL_MS) {
    return true;
  }
  return motorLaeuft;                 // noch nicht lange genug: Zustand beibehalten
}

// Misst die Referenz: Mittelwert über mehrere Messungen bei gestrecktem Arm.
void einmessen() {
  Serial.println("Einmessen... Arm ruhig gestreckt halten.");
  float summe = 0.0;
  for (int i = 0; i < EINMESS_ANZAHL; i++) {
    summe = summe + winkelLesen();
    delay(MESS_PAUSE_MS);
  }
  referenzWinkel = summe / EINMESS_ANZAHL;

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
// Der Arduino schaltet nur den MOSFET. Den Strom für den Motor liefert der 3-V-Regler.
void feedbackAusgeben(bool zuWeit) {
  motorLaeuft = zuWeit;
  if (zuWeit) {
    analogWrite(PIN_MOTOR_PWM, MOTOR_STAERKE);
  } else {
    analogWrite(PIN_MOTOR_PWM, 0);
  }
}

// Gibt true zurück, wenn die Taste gerade eben gedrückt wurde (nur einmal pro Druck).
// Wartet 50 ms nach jeder Änderung, weil eine Taste beim Drücken kurz "prellt".
bool tasteGedrueckt() {
  int zustand = digitalRead(PIN_TASTER);
  if (zustand == alterTasterZustand) {
    return false;                     // nichts hat sich geändert
  }
  if (millis() - letzteTasterAenderung < 50) {
    return false;                     // noch am Prellen, ignorieren
  }
  letzteTasterAenderung = millis();
  alterTasterZustand = zustand;
  return zustand == LOW;              // LOW = gedrückt
}
