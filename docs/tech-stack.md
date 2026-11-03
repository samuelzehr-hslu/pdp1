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
| `arduino-cli` | `TODO` | Kompilieren (nicht hochladen!) |
| Core `arduino:avr` | `TODO` | Unterstützung für Uno/Nano |
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

## Test-Werkzeuge

| Werkzeug | Version | Wozu |
|---|---|---|
| `g++` oder `clang++` (C++-Compiler für den PC) | `TODO` | Logik-Tests und Fuzz-Tests (`docs/tests/logik/`) |
| `cppcheck` | `TODO` | Code-Scanner für typische Sicherheitsfehler |
| Python | 3.x | Szenarien erzeugen (`docs/tests/szenarien/`), Tests starten |
| `wokwi-cli` | `TODO` | Wokwi-Simulation von der Kommandozeile (`docs/wokwi/`), braucht Token |

Ob alles installiert ist, prüft der Skill `/einrichten`.
