// sensor_tabelle.h: liefert Rohwerte der zwei IMUs (aus dem Arm-Simulator der Tests) für das Testprogramm.
#ifndef SENSOR_TABELLE_H
#define SENSOR_TABELLE_H
#include <stdint.h>
// Szenario laden. Name: "normal", "abbruch" oder "ohne_imu2". Gibt die Dauer in ms zurück (0 = unbekannt).
unsigned long sensorSzenarioLaden(const char *name);
// Rohwerte (ax, ay, az, gx, gy, gz) der IMU 'imu' (0 oder 1) zur Zeit 'ms'.
void sensorRoh(int imu, unsigned long ms, int16_t out[6]);
#endif
