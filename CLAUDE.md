# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekt

JointSense: tragbares Assistenzsystem, das die Überstreckung des Ellbogens erkennt und per Vibration meldet (Arduino Uno/Nano, 2 IMUs, Vibrationsmotor). Studentisches Team-Projekt (HSLU, Modul PDP). Details zu Hardware, Software-Konzept und Anforderungen stehen in `README.md`.

## Arbeitsweise mit dem Team

- Das Team besteht aus Programmier-Anfängern: Antworten und Code-Kommentare **auf Deutsch**, **extrem simpel** halten und gut erklären. Einfache Namen, keine cleveren Tricks.
- **Nicht committen oder pushen**, ausser das Team fordert es ausdrücklich auf.
- **Nie aufs Board hochladen und den seriellen Port nicht belegen, ohne vorher zu fragen.** Das Board ist ein physisches Gerät, andere Teammitglieder testen damit.
- Messdaten, Logs und Experimente gehören in `scratch/` (gitignored), nicht ins Repo.
- Persönliche Einstellungen (COM-Port, Pfad zu `arduino-cli`, Board Uno oder Nano) stehen in `CLAUDE.local.md` (nicht eingecheckt).

## Arbeitsweise mit Agenten (Pflicht)

Jede Änderung an der Firmware (`jointsense/`) läuft über drei Agenten. Das gilt für alle, die das Repo klonen.

| Agent | Datei | Aufgabe |
|---|---|---|
| **Engineer** | `.claude/agents/engineer.md` | Startet automatisch als Haupt-Chat (`"agent": "engineer"` in `.claude/settings.json`). Höchste Stelle unter den Agenten, über ihm steht nur das Team. Klärt Anforderungen mit dem Team, macht den Hardware-Check, legt die Schnittstelle fest und steuert Developer und Tester. Schreibt selbst keinen Code. |
| **Developer** | `.claude/agents/developer.md` | Programmiert **nur aus der Anforderung** und macht sonst nichts: keine Tests, keine Simulatoren. Sieht die Testfälle nicht (ein Hook sperrt `docs/tests/`, `docs/wokwi/` und `docs/testberichte/`). Verantwortlich für `docs/tech-stack.md`, arbeitet gegen `docs/bedrohungen.md`. |
| **Tester** | `.claude/agents/tester.md` | Schreibt die Testfälle **vor** dem Programmieren, nur aus der Anforderung. Testet danach: Kompilieren, `cppcheck`, Logik- und Fuzz-Tests (`g++`), Python-Szenarien, Wokwi. Ändert keinen Produkt-Code. |

### Ablauf (Pipeline)

1. **Anforderung klären** (Team + Engineer, Skill `/anforderung`) → `docs/anforderungen/REQ-xxx.md` aus `_vorlage.md`.
2. **Hardware-Check und Schnittstelle** (Engineer) → Status `fixiert`.
3. **Testfälle schreiben** (Tester) → Status `tests-geschrieben`. Unklarheiten meldet der Tester dem Engineer, der klärt sie (falls nötig mit dem Team).
4. **Programmieren** (Developer, ohne Testkenntnis) → Status `umgesetzt`.
5. **Testen** (Tester) → Testbericht in `docs/testberichte/` → Status `getestet`.
6. **Auswerten** (Engineer): Alles bestanden → Team nimmt ab → Status `abgenommen`. Fehler → Engineer beschreibt sie dem Developer **in Worten** (ohne Testcode) → zurück zu 4.
   Höchstens **3 Runden**, danach Status `pausiert` und das Team entscheidet (weiter mit `/weiter REQ-xxx`). Eine Runde ist ein Programmier-Durchgang. Test-Korrekturen sind ebenfalls auf **3** begrenzt, danach bespricht der Engineer es mit dem Team.

Schritte 3 bis 6 laufen automatisch. Das Team ist nur bei 1, bei Rückfragen und bei der Abnahme gefragt.
Das ist nur die Übersicht. Die genauen Regeln stehen in `.claude/agents/engineer.md` (Ablauf) und `.claude/agents/tester.md` (Prüfkette).

### Skills

| Befehl | Wozu |
|---|---|
| `/anforderung` | Startknopf: neue Anforderung klären, danach läuft die Pipeline bis zur Abnahme |
| `/weiter REQ-xxx` | Pausierte Anforderung fortsetzen |
| `/setup-aenderung` | Kleine Änderung ohne ganze Pipeline (Pin, Einstellwert, Werkzeug, Agenten-Regel): der Engineer passt alle Stellen im Repo an |
| `/testen` | Aktuellen Code ausserhalb der Pipeline testen (z. B. nach Änderungen von Hand) |
| `/status` | Übersicht: welche Anforderung steht wo |
| `/einrichten` | Erkennt System und Prozessor, prüft `arduino-cli`, C++-Compiler, `cppcheck`, Python, `wokwi-cli` und die Sicherheits-Sperren |

### Regeln

- Kein Code in `jointsense/` ohne Anforderung mit Status `fixiert` oder später. Ausgenommen sind kleine Tippfehler in Kommentaren und Änderungen über `/setup-aenderung`.
- Der Developer ändert **nie** Tests. Hält er einen Test oder eine Anforderung für falsch, meldet er das dem Engineer.
- Status-Werte: `Entwurf → fixiert → tests-geschrieben → umgesetzt → getestet → abgenommen`, dazu `pausiert`.
- Hochladen aufs Board und der Serielle Monitor sind in `.claude/settings.json` **gesperrt**. Hochladen macht ein Mensch selbst (Arduino IDE oder `arduino-cli upload`).

### Plattformen (Windows, Mac, Linux, Intel und ARM)

Das Framework läuft auf **Windows (x64 und ARM64), Mac (Intel und Apple-Chip) und Linux**.

- **Python** heisst je nach PC `python3` (Mac/Linux), `python` oder `py` (Windows). In der Doku steht meist `python`: den Namen nehmen, den `/einrichten` anzeigt.
- **Sicherheits-Sperren (Hooks)** starten über `.claude/hooks/python_starten.sh`. Das Skript sucht selbst das richtige Python. Es läuft mit `sh` (auf Windows kommt `sh` mit Git Bash). Fehlt Python, werden Developer und Tester gesperrt, damit die Sperren nicht still ausfallen.
- **C++-Compiler** für die Logik-Tests: `g++` oder `clang++` (Windows ARM hat meist nur `clang++`).
- **Mac mit Apple-Chip** braucht Rosetta 2 für das Board-Paket `arduino:avr`.
- `.sh`-Dateien haben immer LF-Zeilenenden (siehe `.gitattributes`), sonst laufen sie nicht.
- Wer neu klont: zuerst **`/einrichten`** ausführen.

## Source of Truth für die Verdrahtung

- `README.md`, Abschnitt «Verdrahtung (Schaltplan Rev. B)», ist die **einzige Wahrheit** für die Verkabelung.
- `jointsense/pins.h` ist die **einzige Stelle im Code**, an der Pin-Nummern und I²C-Adressen stehen. Sie ist eine 1:1-Übersetzung der README-Tabelle.
- Im restlichen Code nur die Namen verwenden (z. B. `PIN_MOTOR_PWM`), nie Zahlen wie `9`.
- Ändert sich ein Kabel: **zuerst README, dann `pins.h`, im selben Commit.** Nicht still eine von beiden ändern; Widersprüche dem Team melden.

## Code-Struktur

Arduino-Sketch in `jointsense/`. Der Ordnername muss gleich heissen wie die `.ino`-Datei (Arduino-Regel).

- `jointsense/jointsense.ino`: Ablauf (`setup()`, `loop()`) und alles, was Hardware braucht (Sensoren lesen, Motor, Taste, Serieller Monitor).
- `jointsense/logik.h` + `jointsense/logik.cpp`: reine Logik (Grenzwert, Entprellung, Hysterese, Einmessen) **ohne Arduino-Befehle**. In `.h` steht die Liste der Funktionen, in `.cpp` ihr Inhalt. So kann man die Logik mit `g++` auf dem PC testen. Zeit (`millis()`) wird als Wert übergeben, nicht in der Logik abgefragt.
- `jointsense/pins.h`: Pins und I²C-Adressen. SDA/SCL (A4/A5) werden von `Wire` automatisch genutzt.
- `jointsense/diagram.json`: Schaltung für die Wokwi-Simulation.

Das Signalverarbeitungs-Konzept (Abtastrate, Sensorfusion, Entprellung, Einmessen, Drift) steht in der README unter «Software».

Dokumentation und Tests liegen in `docs/`:

| Ordner / Datei | Inhalt |
|---|---|
| `docs/anforderungen/` | Anforderungen `REQ-xxx.md` und `_vorlage.md` |
| `docs/tests/logik/` | Logik- und Fuzz-Tests (`g++`), Start mit `python docs/tests/alle_tests.py` (Mac/Linux: `python3`) |
| `docs/tests/szenarien/` | Python erzeugt Winkelverläufe (CSV), baut die Logik **nicht** nach |
| `docs/testberichte/` | Testberichte pro Anforderung und Runde |
| `docs/wokwi/` | `wokwi.toml` und Anleitung für `wokwi-cli` |
| `docs/tech-stack.md` | Werkzeuge und Bibliotheken mit fester Version |
| `docs/bedrohungen.md` | Bedrohungsliste (Cybersecurity und Ausfälle) |
| `docs/offene-hinweise.md` | Hinweise aus Testberichten, die noch niemand erledigt hat |

## Kompilieren und Hochladen

Gebaut wird mit der Arduino IDE oder `arduino-cli`. Tests laufen auf dem PC (siehe oben), einen Build-Server gibt es nicht.

- Kompilieren (z. B. Uno): `arduino-cli compile --fqbn arduino:avr:uno --warnings all --output-dir scratch/build jointsense` (Nano: `arduino:avr:nano`). Das lädt nichts aufs Board, den Pfad zu `arduino-cli` und das Board vorher aus `CLAUDE.local.md` bzw. beim Team erfragen.
- Hochladen (`arduino-cli upload`) ist für Claude gesperrt, siehe oben. Ein Mensch lädt selbst hoch, nach Absprache im Team.

## Offene Punkte (nicht selbst entscheiden)

Diese Punkte sind im Team noch zu klären und in `pins.h` als `TODO` markiert:

1. Pins für **Taste** und **Status-LED**: in der README unter «Bedienung» erwähnt, aber nicht in der Verdrahtungstabelle.
2. **IMU-Typ** ist offen. Die README nennt den BNO085, die Adressen `0x68`/`0x69` über AD0 passen aber zu MPU6050/MPU6500 (BNO085: `0x4A`/`0x4B`).
3. **Motortreiber-Typ** ist offen.
4. **IMU-Bibliothek** ist offen (README «Setup», Punkt 3).
