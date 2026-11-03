---
name: einrichten
description: Prüft, ob alle Werkzeuge für JointSense installiert sind (arduino-cli mit avr-Core, C++-Compiler, cppcheck, Python, wokwi-cli mit Token, Sicherheits-Sperren), erkennt Windows/Mac/Linux und Intel/ARM und erklärt Schritt für Schritt, was fehlt. Installiert nichts ohne Rückfrage.
---

# /einrichten: Werkzeuge prüfen

Deutsch, extrem simpel, für Programmier-Neulinge.
**Installiere nichts selbst, ohne vorher zu fragen.** Nie hochladen, nie den seriellen Port belegen.

## Schritt 1: Prüfen

Führe das Prüfskript aus. Python heisst je nach PC anders, darum der Reihe nach probieren:

`python3 "${CLAUDE_SKILL_DIR}/pruefen.py" || python "${CLAUDE_SKILL_DIR}/pruefen.py" || py "${CLAUDE_SKILL_DIR}/pruefen.py"`

Findet der Computer nicht einmal Python: das zuerst erklären (siehe Tabelle unten).

## Schritt 2: Ergebnis erklären

Das Skript zeigt oben **System und Prozessor** (z. B. «Windows, ARM64» oder «Mac, ARM64 (Apple-Chip)»).
Nimm aus der Tabelle unten **nur die Spalte, die dazu passt**. Zeig kurz, was `[OK]` und was `[FEHLT]` ist.
Erkläre für **jedes fehlende Werkzeug**:
1. Wozu wir es brauchen (ein Satz)
2. Wie man es installiert, Schritt für Schritt

| Werkzeug | Wozu | Windows x64 (Intel/AMD) | Windows ARM64 | Mac (Intel und Apple-Chip) | Linux (x64 und ARM64) |
|---|---|---|---|---|---|
| Python 3 | Prüfskript, Sicherheits-Sperren, Szenarien | `winget install Python.Python.3.12` oder python.org («Add to PATH» ankreuzen) | gleich, winget wählt die ARM-Version selbst | `brew install python` | `sudo apt install python3` |
| sh (Shell) | startet die Sicherheits-Sperren | kommt mit Git for Windows (`winget install Git.Git`), das Claude Code ohnehin braucht | gleich | schon vorhanden | schon vorhanden |
| arduino-cli | Code für Uno/Nano kompilieren | `winget install ArduinoSA.CLI` | gleich (die Intel-Version läuft über die eingebaute Emulation) | `brew install arduino-cli` | Installationsskript von arduino.github.io/arduino-cli (es gibt ARM64-Versionen) |
| arduino:avr | Board-Paket für Uno/Nano | `arduino-cli core update-index`, dann `arduino-cli core install arduino:avr` | gleich | gleich | gleich |
| Rosetta 2 | nur Mac mit Apple-Chip: das Board-Paket enthält Intel-Programme | – | – | `softwareupdate --install-rosetta` | – |
| C++-Compiler (`g++` oder `clang++`) | Logik-Tests auf dem PC | MSYS2 von msys2.org, im MSYS2-Fenster `pacman -S mingw-w64-ucrt-x86_64-gcc`, Ordner `C:\msys64\ucrt64\bin` zum PATH | MSYS2 (ARM-Installer), im Fenster «MSYS2 CLANGARM64» `pacman -S mingw-w64-clang-aarch64-clang`, Ordner `C:\msys64\clangarm64\bin` zum PATH (gibt `clang++`) | `xcode-select --install` | `sudo apt install g++` |
| cppcheck | sucht Sicherheitsfehler im Code | `winget install Cppcheck.Cppcheck`, Ordner `C:\Program Files\Cppcheck` zum PATH | gleich (läuft über Emulation) oder in MSYS2: `pacman -S mingw-w64-clang-aarch64-cppcheck` | `brew install cppcheck` | `sudo apt install cppcheck` |
| wokwi-cli | Arduino-Simulation | siehe github.com/wokwi/wokwi-cli | gleich (Intel-Version läuft über Emulation) | gleich (es gibt eine Apple-Chip-Version) | gleich (es gibt eine ARM64-Version) |
| WOKWI_CLI_TOKEN | Zugang für wokwi-cli | gratis auf wokwi.com/dashboard/ci erstellen, als Umgebungsvariable setzen («Umgebungsvariablen bearbeiten») | gleich | in `~/.zshrc`: `export WOKWI_CLI_TOKEN=...` | in `~/.bashrc`: `export WOKWI_CLI_TOKEN=...` |

Steht bei «Sicherheits-Sperren (Hooks)» `[FEHLT]`: Das ist **wichtig**. Ohne Sperren ist der Developer nicht blind und der Tester nicht vom Code getrennt. Meist fehlt Python oder `sh`. Die Sperren halten Developer und Tester dann vorsichtshalber ganz an, bis es behoben ist.

Hinweise:
- Nach einer PATH-Änderung das Terminal bzw. Claude Code **neu starten**.
- Der **Wokwi-Token ist geheim**: nie im Chat zeigen, nie in eine Datei im Repo schreiben, nie committen. Du (Claude) liest ihn nie aus und gibst ihn nie aus.
- Hat der User einen anderen Weg, der funktioniert (z. B. Arduino IDE schon installiert), ist das auch gut.

## Schritt 3: Persönliche Einstellungen

Erinnere daran: **COM-Port, Pfad zu `arduino-cli` und Board (Uno oder Nano)** gehören in `CLAUDE.local.md`. Diese Datei ist nicht eingecheckt.
Gibt es sie noch nicht, biete an, sie mit diesen drei Zeilen anzulegen (Werte fragt man den User).

## Schritt 4: Nochmals prüfen

Hat der User etwas installiert: Prüfskript nochmals laufen lassen, bis alles `[OK]` ist.
Ein fehlendes Werkzeug ist kein Weltuntergang: Der Tester überspringt dann nur diesen Prüfschritt und meldet das.

## Schritt 5: Python-Name merken

Das Skript zeigt «Python startet hier mit: …». Diesen Namen (`python3`, `python` oder `py`) für alle Python-Befehle in der Doku verwenden. In der Doku steht meist `python`, auf Mac und Linux heisst es aber meist `python3`.
