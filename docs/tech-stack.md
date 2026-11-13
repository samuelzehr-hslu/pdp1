# Tech-Stack

Hier steht, **welche Werkzeuge und Bibliotheken** das Projekt nutzt, und zwar in welcher Version.
Verantwortlich ist der **Developer**. Neue Bibliotheken kommen nur dazu, wenn sie hier eingetragen sind.

Regeln:
- Bibliotheken nur aus dem **offiziellen Arduino Library Manager**.
- Immer eine **feste Version** eintragen (nicht «neueste»).
- `TODO` heisst: noch nicht festgelegt, Team muss entscheiden.

## Hardware

| Teil | Typ | Bemerkung |
|---|---|---|
| Board | Arduino Uno oder Nano (ATmega328P) | 2 KB RAM, 32 KB Flash |
| IMUs | `TODO` (Team) | README nennt BNO085, Adressen `0x68`/`0x69` passen aber zu MPU6050/MPU6500 |
| Motortreiber | `TODO` (Team) | |

## Bauen (Firmware)

| Werkzeug | Version | Wozu |
|---|---|---|
| `arduino-cli` | `1.5.1` | Kompilieren (nicht hochladen!) |
| Core `arduino:avr` | `1.8.8` | Unterstützung für Uno/Nano |
| Arduino IDE (optional) | `TODO` | Alternative zum Bauen mit Maus |

Kompilier-Befehl (Uno; beim Nano `arduino:avr:nano`):

```bash
arduino-cli compile --fqbn arduino:avr:uno --warnings all --output-dir scratch/build jointsense
```

## Bibliotheken (Firmware)

| Bibliothek | Version | Quelle | Wozu |
|---|---|---|---|
| `Wire` | im Core enthalten | Arduino Core | I²C-Bus zu den IMUs |
| IMU-Bibliothek | `TODO` (Team) | Library Manager | Sensoren auslesen |

## Einstellungen im Code (keine Bibliothek, nur Register der IMU und des Busses)

Stand REQ-002. Der Code spricht die IMUs wie einen MPU6050 an (der IMU-Typ ist weiterhin ein offener Team-Punkt). Nur die Sensor-Funktionen in `jointsense.ino` kennen den Chip.

| Einstellung | Wert | Grund |
|---|---|---|
| Gyro-Messbereich (Register `0x1B`) | `0x08` = ±500 °/s (65,5 pro °/s) | Schnelles Beugen und Drehen übersteigt den Standard ±250 °/s (Hardware-Check REQ-002). Der Wert wird alle 1000 ms neu geschrieben, falls eine IMU kurz neu startet. |
| Beschleunigung (Register `0x1C`) | Standard ±2 g (16384 pro g) | Reicht für die Schwerkraft, kein Eingriff nötig. |
| Lesen | 14 Bytes ab Register `0x3B` | Beschleunigung, Temperatur, Gyro in einer Anfrage (Zykluszeit, AK-15). |
| I²C-Takt (`Wire.setClock`) | 400 kHz | Ein Messzyklus mit zwei IMUs bleibt unter 15 ms (AK-15). |
| Serieller Monitor | 9600 Baud | wie bisher |

## Rechenverfahren der Winkelmessung (REQ-002, Runde 2)

Keine neue Bibliothek. Es wird nur `math.h` aus dem Arduino-Core (avr-libc) benutzt (`sinf`, `cosf`, `atan2f`, `sqrtf`, `floorf`).

| Teil | Wert | Grund |
|---|---|---|
| Lage der IMUs zueinander | Quaternion (4 `float`), von beiden Gyros weitergedreht, von der Schwerkraft korrigiert | Drehungen um Knochenachsen ändern den Ellbogenwinkel nicht, auch nicht bei mehreren Bewegungen gleichzeitig |
| Flash (Uno) | 32252 von 32256 Byte (99 %), gemessen mit `arduino-cli 1.5.1` (REQ-005 Runde 3; vor REQ-005 30410 Byte = 94 %) | Grenze 32256 Byte. Nur noch 4 Byte frei: neue Funktionen nur, wenn anderswo gespart wird |
| RAM (globale Variablen) | 1322 von 2048 Byte (64 %), 726 Byte für lokale Variablen frei (REQ-005 Runde 3) | Grenze 70 % = 1434 Byte. Neu im Zustand: Eigenvektoren der letzten Sekunde (144 Byte) und 16 gemerkte Lagen (256 Byte). Die Achsen-Suche hat noch ein lokales 6x6-Feld (144 Byte) auf dem Stapel |
| Lage beim Einmessen (REQ-005) | Lage jeder IMU von Schritt 1 bis zum Abschluss mit ihrem Gyro weiterdrehen (auch im Warte-Zustand und in Schritt 2), Schwerkraft zieht jede IMU einzeln zurecht (0,8 1/s) und führt den Gyro-Fehler nach (0,05 1/s²) | Bis 60 s ohne Referenz-Lage dazwischen: ohne Stütze würde der Gyro-Fehler mehrere Grad aufsummieren. Der Arm muss bei der Bestätigung nicht in der Referenz-Lage sein. Keine neue Bibliothek. |
| Einordnen in Schritt 2 (REQ-005) | Handdrehung = Unterarm-IMU dreht vor allem um die Schwerkraft-Richtung aus Schritt 1 (Cosinus ≥ 0,8) und der Oberarm dreht höchstens halb so schnell. Messungen mit deutlicher Drehung um diese Richtung (> 60 % und > 3 °/s) kommen nicht in die Achsen-Suche | Die Achsen-Suche erlaubt nur eine Drehachse. Annahme nur zum Einordnen: Unterarm hing in Schritt 1 höchstens 30° schief (REQ-005, Physik-Annahmen) |
| Serieller Monitor, Eingabe (REQ-004) | Pro `loop()` höchstens 64 Zeichen lesen (Empfangspuffer), nur `' '`, `'\n'`, `'\r'` zählen | B-1, B-7: kein `String`, kein eigener Puffer, nie warten |
| Achsen-Suche (REQ-002 Runde 2, REQ-005) | Jacobi-Verfahren (cyclisch, 6x6, `logik.cpp`), seit REQ-005 einmal pro Sekunde und ab den Eigenvektoren der letzten Sekunde weitergerechnet (höchstens 2 Durchgänge pro Sekunde) | Ohne Matrix-Inversion (die war bei fast flachen Daten ungenau). Weiterrechnen spart Rechenzeit: Auswertung in Schritt 2 höchstens 100 ms (AK-17, am Board zu messen) |
| `NICHT_INLINE` in `logik.cpp` | `__attribute__((noinline))` | Spart Flash, weil der Compiler die Hilfsfunktionen nicht überall hineinkopiert |

## Test-Werkzeuge

| Werkzeug | Version | Wozu |
|---|---|---|
| `g++` oder `clang++` (C++-Compiler für den PC) | `TODO` | Logik-Tests und Fuzz-Tests (`docs/tests/logik/`) |
| `cppcheck` | `TODO` | Code-Scanner für typische Sicherheitsfehler |
| Python | 3.x | Szenarien erzeugen (`docs/tests/szenarien/`), Tests starten |
| `wokwi-cli` | `TODO` | Wokwi-Simulation von der Kommandozeile (`docs/wokwi/`), braucht Token |

Ob alles installiert ist, prüft der Skill `/einrichten`.
