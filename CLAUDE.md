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

## Source of Truth für die Verdrahtung

- `README.md`, Abschnitt «Verdrahtung (Schaltplan Rev. B)», ist die **einzige Wahrheit** für die Verkabelung.
- `jointsense/pins.h` ist die **einzige Stelle im Code**, an der Pin-Nummern und I²C-Adressen stehen. Sie ist eine 1:1-Übersetzung der README-Tabelle.
- Im restlichen Code nur die Namen verwenden (z. B. `PIN_MOTOR_PWM`), nie Zahlen wie `9`.
- Ändert sich ein Kabel: **zuerst README, dann `pins.h`, im selben Commit.** Nicht still eine von beiden ändern; Widersprüche dem Team melden.

## Code-Struktur

Arduino-Sketch in `jointsense/`. Der Ordnername muss gleich heissen wie die `.ino`-Datei (Arduino-Regel).

- `jointsense/jointsense.ino`: Ablauf (`setup()`, `loop()`, `winkelLesen()`, `grenzwertPruefen()`, `feedbackAusgeben()`). Erste Version der Logik ist drin (MPU6050 auslesen, Einmessen, Grenzwert, Motor).
- `jointsense/diagram.json`: Wokwi-Simulation. Wokwi simuliert keine MOSFET-Stufe, der Motor ist dort eine LED.
- `jointsense/pins.h`: Pins und I²C-Adressen. SDA/SCL (A4/A5) werden von `Wire` automatisch genutzt.

Wird der Sketch zu lang, Logik (Winkel, Grenzwert, Motor) in weitere `.h`/`.cpp`-Dateien im selben Ordner auslagern. Das Signalverarbeitungs-Konzept (Abtastrate, Sensorfusion, Entprellung, Einmessen, Drift) steht in der README unter «Software».

## Kompilieren und Hochladen

Es gibt keinen Build-Server und keine Tests. Gebaut wird mit der Arduino IDE oder `arduino-cli`.

- Kompilieren (z. B. Uno): `arduino-cli compile --fqbn arduino:avr:uno jointsense` (Nano: `arduino:avr:nano`). Das lädt nichts aufs Board, den Pfad zu `arduino-cli` und das Board vorher aus `CLAUDE.local.md` bzw. beim Team erfragen.
- Hochladen (`arduino-cli upload`) nur nach ausdrücklicher Rückfrage, siehe oben.

## Offene Punkte (nicht selbst entscheiden)

Diese Punkte sind im Team noch zu klären und in `pins.h` als `TODO` markiert:

1. Pins für **Taste** und **Status-LED**: in der README unter «Bedienung» erwähnt, aber nicht in der Verdrahtungstabelle.
2. **IMU-Typ** ist offen. Die README nennt den BNO085, die Adressen `0x68`/`0x69` über AD0 passen aber zu MPU6050/MPU6500 (BNO085: `0x4A`/`0x4B`).
3. **MOSFET-, Dioden- und Spannungsregler-Typ** sind offen (MOSFET muss Logic-Level sein).
4. **IMU-Bibliothek** ist offen (README «Setup», Punkt 3).
