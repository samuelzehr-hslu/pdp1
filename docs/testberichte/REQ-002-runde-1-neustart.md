# Testbericht REQ-002, Runde 1 (Neustart)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-002](../anforderungen/REQ-002.md) |
| **Runde** | 1 von 3 (Neustart nach Team-Entscheidung «Achse messen statt annehmen») |
| **Datum** | 2026-11-06 |
| **Code-Stand** | nicht committet (Änderungen in `jointsense.ino`, `logik.h` nur Kommentar, `tech-stack.md`; `logik.cpp` unverändert) |
| **Gesamtergebnis** | **nicht geprüft** (kein Compiler vorhanden, kein einziger Test konnte laufen). Weder «bestanden» noch «nicht bestanden». |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`), alle Warnungen als Fehler | nicht geprüft | `arduino-cli` nicht gefunden, `CLAUDE.local.md` fehlt. Siehe `/einrichten`. |
| 1b | Speicher: RAM-Belegung | nicht geprüft | Kein Build. Flash war vorher bei 98 % (374 Byte frei): Phase 3 (zweiter Stoss, Abbruchgrund `Hand`) macht den Sketch grösser. **Hohes Risiko, dass er nicht mehr passt.** |
| 2 | Code-Scanner `cppcheck` | nicht geprüft | `cppcheck` nicht gefunden. Siehe `/einrichten`. |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`) | nicht geprüft | `python docs/tests/alle_tests.py` meldet: «Kein C++-Compiler gefunden (g++ oder clang++)». `C:\msys64` existiert nicht. Keine Ersatzsuche (Regel «keine Umwege»). Python-Szenarien starten erst zusammen mit den Tests, auch nicht gelaufen. |
| 3b | Fuzz-Tests | nicht geprüft | wie 3 |
| 4 | Wokwi-Simulation inkl. Störfälle | nicht geprüft | `wokwi-cli` nicht gefunden. Zudem Gratis-Kontingent laut Verlauf aufgebraucht, darum keine Wokwi-Aktion versucht. |
| 5 | Bibliotheken | ok (nur Lesen) | Nur `Wire.h`, `math.h` (Arduino-Core), keine Fremd-Bibliothek. Passt zu `docs/tech-stack.md`. IMU-Bibliothek dort weiter `TODO` (Teamentscheid, kein Code-Fehler). |
| 6 | Sicherheitsregeln im Code | teilweise | Kein `String`, kein `new`/`malloc`. Pins nur über `pins.h`-Namen. `NaN`/`inf`-Prüfungen in `logik.cpp` vorhanden. **`Wire.setWireTimeout` kommt im Code nicht vor.** Hängender I²C-Bus ist laut REQ-002 «Nicht dazugehörig», gehört zu B-3 (eigene Anforderung). |

## Akzeptanzkriterien

Kein Kriterium konnte geprüft werden. Nichts davon gilt als erfüllt.

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 bis AK-5 | nicht geprüft | kein Compiler. Code von `logik.cpp` ist unverändert zu Runde 3, dort aber ohne Phase 3 getestet. Alte Ergebnisse gelten **nicht**, die Anforderung ist neu (AK-6, AK-7e, AK-16, neue Toleranzen). |
| AK-6 (Ablauf, 7500 ms, 2 Stösse) | nicht geprüft | Nur Code gelesen: Phase 3 ab 6000 ms, Stoss 1 und Stoss 2 (200 ms Abstand), Ende bei 7500 ms. Sieht passend aus, ist aber nicht belegt. Hardware-Teil nur in Wokwi oder am Board prüfbar. |
| AK-7 (Abbruch, auch `Grund: Hand`) | nicht geprüft | Zeile `Hand` im Code vorhanden (gelesen), nicht gelaufen. |
| AK-8 bis AK-16 | nicht geprüft | kein Compiler / kein Wokwi |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-2 | nicht geprüft | Fuzz-Tests nicht gelaufen. Nur gesehen: `NaN`/`inf`-Prüfungen existieren. |
| B-3 | nicht geprüft | RAM/Flash/Zykluszeit nicht gemessen |
| B-6 | nicht geprüft | AK-7, AK-12, AK-13, AK-16 nicht gelaufen |

## Fehler in Worten für den Developer

Keine Fehler festgestellt, weil nichts lief. Es gibt keinen Beleg für oder gegen die Umsetzung.

## Am echten Board prüfen

- Zykluszeit (≤ 15 ms) und «Zyklus max»-Zeile, Gyro-Übersteuerung (AK-14), Ablauf mit Stössen (AK-6) und die drei Phasen von Hand.
- Ob der Sketch nach Phase 3 noch in den Flash passt (vorher 98 %).

## Hinweise für den Engineer

- Alles hängt an fehlenden Werkzeugen: `g++` oder `clang++`, `arduino-cli`, `cppcheck`, `wokwi-cli`. `CLAUDE.local.md` fehlt (Board, Pfad). Bitte `/einrichten` ausführen, dann Prüfung wiederholen. Die Runde sollte nicht als verbraucht zählen, weil nichts geprüft wurde (Entscheidung beim Engineer/Team).
- `Wire.setWireTimeout` fehlt im Code (Sicherheitsregel, B-3, ausserhalb von REQ-002). Als offener Hinweis eintragen.
- Flash-Risiko nach Phase 3 (vorher 374 Byte frei) und `tech-stack.md` hat Flash/RAM als `TODO`.
- Testdateien: unverändert. Es wurden keine Tests geändert und keine Korrekturen gemacht.

## Fehlende Werkzeuge

- `g++` / `clang++`: nicht gefunden (`Kein C++-Compiler gefunden`), `C:\msys64` fehlt → siehe `/einrichten`
- `arduino-cli`: nicht gefunden → siehe `/einrichten`
- `cppcheck`: nicht gefunden → siehe `/einrichten`
- `wokwi-cli`: nicht gefunden (und Kontingent laut Verlauf leer) → siehe `/einrichten`
- Python 3.11 vorhanden.
