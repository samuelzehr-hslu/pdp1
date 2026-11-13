// ino_lauf.cpp: Testprogramm für REQ-007. Nur für die Tests.
// Bindet die (vom Skript ino_vergleich.py vorbereitete) Firmware jointsense.ino ein und spielt ein festes Szenario ab:
// Zeit, Tastendrücke, Zeichen vom Seriellen Monitor, Kabelbruch. Alles Ausgegebene geht mit Zeit in eine Datei.
// Aufruf: ino_lauf <szenario> <ausgabe.txt> <wire.txt>
#include "arduino_attrappe.h"
#include "ino_zusammen.inc"   // die Firmware (vom Skript erzeugt)
#include "sensor_tabelle.h"

// Jede Millisekunde werden hier die "Ereignisse von aussen" gesetzt.
static void hEreignisse(const char *name, unsigned long ms) {
  // Tasten-Zustand und Kabel werden bei jedem Aufruf neu gesetzt (nur wo das Szenario etwas ändert)
  if (strcmp(name, "normal") == 0) {
    if (ms == 500) h_rxSchicken("abc123xyz");                       // Unsinn (keine Leerzeichen/Enter): wird ignoriert
    if (ms == 900) {                                                // 150 Zeichen am Stück (Puffer ist nur 64 gross)
      char lang[160];
      for (int i = 0; i < 150; i++) lang[i] = (char)('a' + (i % 26));
      lang[150] = 0;
      h_rxSchicken(lang);
    }
    if (ms == 3000) h_rxSchicken("\n");                             // Enter: Einmessen Schritt 1 beginnt
    if (ms == 4000) h_rxSchicken(" ");                              // Leertaste in Schritt 1: wird ignoriert
    h_taste = (ms >= 7000 && ms < 7015) || (ms >= 7030 && ms < 7200);  // Taste mit Prellen: Schritt 2 beginnt
    if (ms == 9000) h_rxSchicken("\r");                             // Enter in Schritt 2: wird ignoriert
    if (ms == 12000) h_rxSchicken("\r\n");                          // noch eine Bestätigung in Schritt 2
    if (ms == 20000) h_kabel[0] = false;                            // kurzer Aussetzer von IMU 1 (30 ms): kein Ausfall
    if (ms == 20030) h_kabel[0] = true;
    if (ms == 40000) h_kabel[1] = false;                            // IMU 2 fällt aus (7 s), Arm wird überstreckt
    if (ms == 47000) h_kabel[1] = true;
    if (ms == 60000) { h_kabel[0] = false; }                        // kurzer Aussetzer von IMU 1 (30 ms)
    if (ms == 60030) { h_kabel[0] = true; }
    if (ms == 62000) h_gyroAnschlag = true;                         // IMU 2: Gyro am Anschlag (100 ms)
    if (ms == 62100) h_gyroAnschlag = false;
    if (ms == 64000) h_kabel[0] = false;                            // IMU 1 ein halbe Sekunde weg
    if (ms == 64500) h_kabel[0] = true;
    if (ms == 70000) h_rxSchicken("\n");                            // zweites Einmessen (Arm still)
    if (ms == 76000) h_rxSchicken(" ");                             // weiter mit Schritt 2 (Arm bewegt sich nicht)
    return;
  }
  if (strcmp(name, "abbruch") == 0) {
    if (ms == 3000) h_rxSchicken("\n");                             // Schritt 1, aber der Arm schwenkt dauernd: Abbruch "ruhig"
    if (ms == 41000) h_rxSchicken("\n");                            // Schritt 1 klappt, dann keine Bestätigung: Abbruch
    if (ms == 75000) h_rxSchicken("\n");                            // Schritt 1 klappt
    h_taste = (ms >= 79000 && ms < 79200);                          // Taste: Schritt 2, aber keine Bewegung: Abbruch
    if (ms == 111000) h_rxSchicken("\n");                           // Schritt 1 klappt
    if (ms == 115000) h_rxSchicken(" ");                            // Schritt 2 beginnt
    if (ms == 118000) h_kabel[0] = false;                           // IMU 1 fällt aus: Abbruch "IMU"
    if (ms == 119000) h_kabel[0] = true;
    return;
  }
  if (strcmp(name, "ohne_imu2") == 0) {
    if (ms == 0) h_kabel[1] = false;
    if (ms == 5000) h_rxSchicken("\n");                             // Einmessen mit fehlender IMU: Abbruch
    if (ms == 10000) h_kabel[1] = true;                             // IMU 2 kommt
    if (ms == 12000) h_rxSchicken("\n");                            // jetzt klappt Schritt 1
    if (ms == 17000) h_rxSchicken("\n");                            // in Warte-Zustand: weiter mit Schritt 2
    return;
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "Aufruf: ino_lauf <szenario> <ausgabe.txt> <wire.txt>\n");
    return 2;
  }
  unsigned long dauer = sensorSzenarioLaden(argv[1]);
  if (dauer == 0) {
    fprintf(stderr, "Unbekanntes Szenario: %s\n", argv[1]);
    return 2;
  }
  FILE *aus = fopen(argv[2], "w");
  FILE *wire = fopen(argv[3], "w");
  if (aus == NULL || wire == NULL) {
    fprintf(stderr, "Ausgabedatei lässt sich nicht öffnen\n");
    return 2;
  }
  h_start(aus, wire);
  h_ms = 0;
  h_wireUs = 0;
  hEreignisse(argv[1], 0);
  setup();
  for (unsigned long ms = 0; ms <= dauer; ms++) {
    h_ms = ms;
    h_wireUs = 0;
    hEreignisse(argv[1], ms);
    loop();
  }
  h_ende();
  fclose(aus);
  fclose(wire);
  return 0;
}
