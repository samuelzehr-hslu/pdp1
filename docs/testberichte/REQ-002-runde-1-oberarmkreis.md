# Testbericht REQ-002, Runde 1 (Zählung mit einem Oberarm-Kreis)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-002](../anforderungen/REQ-002.md) |
| **Runde** | 1 von 3 (neue Zählung, Bedienung: ein Oberarm-Kreis) |
| **Datum** | 2026-11-06 |
| **Code-Stand** | nicht committet. Seit der letzten Prüfung nur ein Kommentar in `jointsense.ino` geändert (laut Git: «ein kleiner Kreis von mindestens 15 Grad»), `logik.cpp` und `logik.h` wie Runde 2 der Prüfung davor |
| **Gesamtergebnis** | **bestanden für alles, was auf dem PC prüfbar ist.** Wokwi nicht geprüft (Team-Entscheidung), Board-Teile offen. |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno, `--warnings all`) | ok | 0 Warnungen. Flash 28822 von 32256 Byte (89 %). |
| 1b | Speicher: RAM (Uno: 2048 Bytes) | ok | Belegt 1155 Bytes (56 %), frei 893. Grenze AK-15 (70 %, 1434 Byte) eingehalten. |
| 2 | Code-Scanner `cppcheck` | ok | Kein «error», kein «warning». Ein Stil-Hinweis: `logik.cpp` Zeile 1081, Feld `pf` könnte `const` sein (nur Schönheit). |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`) | ok | 6 von 6 Testdateien bestanden, 0 fehlgeschlagen. 530 Einzelprüfungen «ok», keine «FEHLER». Compiler: g++. |
| 3b | Fuzz-Tests (NaN, unendlich, extreme Werte, Startwert 42) | ok | In `test_REQ-001` (Teil F) und `test_REQ-002` (Fuzz, AK-14) bestanden. |
| 4 | Wokwi-Simulation inkl. Störfälle | nicht geprüft (Team-Entscheidung) | Wurde nicht ausgeführt. |
| 5 | Bibliotheken | ok | Nur `Wire` (im Core) und `math.h`, wie in `docs/tech-stack.md`. Keine neue Bibliothek. |
| 6 | Sicherheitsregeln im Code | ok mit Hinweis | Kein `String`, kein `new`/`malloc`, Pins über `pins.h`, `NaN`/`inf`-Prüfungen in `logik.cpp` vorhanden. `Wire.setWireTimeout` fehlt, gehört aber laut REQ-002 «Nicht dazugehörig» (B-3, eigene Anforderung). |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 | erfüllt (PC) | Alle Armlagen und Ellbogenwinkel. Grösste Winkeländerung 0,42° (Grenze 2°). |
| AK-2 | erfüllt (PC) | wie AK-1 |
| AK-3 | erfüllt (PC) | wie AK-1 |
| AK-4 | erfüllt (PC) | Lagen: grösster Fehler 0,29° (Grenze 2°). Einbaudrehung: grösster Fehler 2,06° (Grenze 2,5°). |
| AK-5 | erfüllt (PC) | Messbereich, Auflösung, Erkennung ± 2° bestanden. |
| AK-6 | erfüllt (PC) | Mit **einem** Kreis klappt das Einmessen bei 15° (Minimum) und bei 20°, jeweils mit und ohne alte Referenz, bei allen 5 Einbauten. Danach stimmt der Winkel auf 2,5° (Fehler bei: leer). Hand 45°, 60°, 90°: Einmessen klappt, Fehler nach dem Einmessen höchstens 2,13°. Zeitablauf (7500 ms, Stösse, 150-ms-Bestätigung) steckt in `jointsense.ino` und ist nicht auf dem PC prüfbar. |
| AK-7 | erfüllt (PC), a bis e | (a) Beugung, (b) ruhig, (e) Hand: bestanden. **(d) Oberarm:** Ein still hängender Oberarm, ein Oberarm mit nur 3° Bewegung und ein Oberarm nur um die Senkrechte (Schwenken und Schulterdrehung je 60°) brechen alle ab (Rückgabe `false`, nie `true` beim zweiten Abschliessen). Alte Referenz und Achse bleiben, die Lage bleibt bekannt, Fehler danach höchstens 2,5° auch bei gleichzeitigen Bewegungen. Nach einem Abbruch klappt ein zweiter Versuch mit Oberarm-Bewegung. Hinweis: Die Zeile `Einmessen abgebrochen - Grund: Oberarm` schreibt `jointsense.ino`, das ist nur in Wokwi oder am Board prüfbar. |
| AK-8 | erfüllt (PC) | Zehn Einmessungen in der Streuung. |
| AK-9 | erfüllt (PC) | Keine Referenz, keine Warnung. Die Zeile `Keine Referenz` im 1-s-Takt ist Hardware-Teil. |
| AK-10 | erfüllt (PC) | Drift mit Ruhe: grösster Fehler 1,99° (Grenze 2,5°). |
| AK-11 | erfüllt (PC) | Drift ohne Ruhe: grösster Fehler 1,96° (Grenze 2,5°). |
| AK-12 | erfüllt (PC) | Referenz bleibt sicher, Warnung bei +8° bleibt. |
| AK-13 | erfüllt (PC) | Nach Ausfall keine Warnung, bis die Lage bekannt ist. |
| AK-14 | erfüllt (PC) | Ungültige Werte werden abgelehnt. Die **Übersteuerung** (Rohwert am Anschlag) steckt in `jointsense.ino`: nicht geprüft (Wokwi nicht ausgeführt). |
| AK-15 | teilweise | RAM 56 % (Grenze 70 %): erfüllt. Zykluszeit ≤ 15 ms und die Zeile `Zyklus max` nur am echten Board prüfbar. |
| AK-16 | erfüllt (PC) | Schieflage 0 bis 10° in 8 Richtungen, Referenz bei 0° und −20°, 4 weitere Einbaulagen: nie ein falscher Winkel (über 2,5°) ohne Abbruch, und das Einmessen klappt bis 10° in allen Richtungen. Auch bei 12°, 15°, 20°, 30° keine falschen Winkel. Grösster Fehler bei geklappten Einmessungen 2,15° (Grenze 2,5°, Reserve klein). |

Der Winkel nach dem Einmessen stimmt auf 2,5°: grösster gemessener Fehler im ganzen Testlauf 2,15° (AK-16) bzw. 2,13° (Hand-Drehung), 2,06° (Einbaudrehung).

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-2 (falsche Sensordaten) | geschützt (Logik) | Fuzz-Tests und AK-14. Übersteuerungs-Überspringen im `.ino` nicht geprüft. |
| B-3 (Lahmlegen) | teilweise | RAM ok. Zykluszeit nur am Board prüfbar. Hängender I²C-Bus gehört nicht zu REQ-002. |
| B-6 (Einmessen verfälscht die Messung) | geschützt (Logik) | AK-7, AK-12, AK-13, AK-16: richtig oder Abbruch, nie falscher Winkel ohne Abbruch. |

## Fehler in Worten für den Developer

Keine Fehler gefunden. Alle auf dem PC prüfbaren Kriterien sind erfüllt.

## Am echten Board prüfen

- Zeitablauf des Einmessens: Gesamtdauer 7500 ms ± 100 ms, 100-ms-Stoss, zwei Stösse, 150-ms-Bestätigung (AK-6).
- Serielle Zeilen `Einmessen abgebrochen - Grund: …` (alle fünf Gründe, vor allem `Oberarm` und `Hand`), `Keine Referenz` alle 1000 ms, `Zyklus max: <n> us` (AK-7, AK-9, AK-15).
- Zykluszeit ≤ 15 ms und Rechenzeit von `einmessenAbschliessen` (blockiert die Schleife, REQ-001 muss weiter funktionieren).
- Gyro-Übersteuerung (Rohwert +32767 / −32768 überspringt den Zyklus, kein IMU-Ausfall), AK-14.
- Mit echtem Sensor-Rauschen: Reserve beim Einmess-Fehler ist klein (2,15° bei Grenze 2,5°). Eine Messung mit dem echten Arm zeigt, ob die Simulation stimmt.
- Flash ist mit 89 % belegt.

## Hinweise für den Engineer

- Wokwi wurde auf Team-Entscheidung nicht ausgeführt: Hardware-Teile des Ablaufs (Zeiten, Meldungen, Übersteuerung) sind nur am Board belegt.
- `cppcheck`-Stilhinweis `pf` (`logik.cpp` Zeile 1081): kein Fehler, kann der Developer bei Gelegenheit als `const` markieren.
- `Wire.setWireTimeout` fehlt im Code. Gehört laut REQ-002 zu B-3 und einer eigenen Anforderung, sollte dort nicht vergessen werden.
- Die Reserve bei AK-4 (Einbaudrehung 2,06° von 2,5°) und AK-16 (2,15° von 2,5°) ist knapp, in der Simulation aber bestanden.

## Fehlende Werkzeuge

Keine. `wokwi-cli` wurde nicht verwendet (Team-Entscheidung, nicht wegen fehlendem Werkzeug).
