# Testbericht REQ-004, Runde 2

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-004](../anforderungen/REQ-004.md) |
| **Runde** | 2 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet |
| **Gesamtergebnis** | ☑ nicht bestanden (AK-13: nur noch der Fall «Arm hängt die ganze Wartezeit still» liegt über 2,5°; Wokwi nicht geprüft) |

Geändert seit Runde 1: `logik.cpp` (nur `warteLageStuetzen`, Gyro-Offset wird in den Warte-Zuständen nachgelernt), `docs/tech-stack.md`. `logik.h` und `jointsense.ino` unverändert. Meine Tests unverändert (keine neue Test-Korrektur, Stand 1 von 3).

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno), alle Warnungen als Fehler | ☑ ok | 0 Warnungen |
| 1b | Speicher | ☑ ok, Flash knapp | RAM 1180 Bytes (57 %), unverändert. **Flash 30410 von 32256 Bytes (94 %)**, noch 1846 Bytes Reserve (Runde 1: 2174). Knapp |
| 2 | `cppcheck` | ☑ ok | Kein error/warning. Derselbe Stil-Hinweis wie in Runde 1 (`pf`, jetzt Zeile 1221) |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, `clang++`) | ☑ Fehler | 9 Testdateien, 8 bestanden, 1 fehlgeschlagen. REQ-001, -002 (alle Teile), -003 und `test_REQ-004` (124/124) grün, wie Runde 1. `test_REQ-004_warten`: 34 von 35 |
| 3b | Fuzz-Tests | ☑ ok | bestanden |
| 4 | Wokwi | ☐ nicht geprüft | Kontingent aufgebraucht |
| 5 | Bibliotheken | ☑ nicht wiederholt (Runde 1 gilt) | `.ino` und Bibliotheken unverändert |
| 6 | Sicherheitsregeln | ☑ nicht wiederholt (Runde 1 gilt) | Nur `warteLageStuetzen` geändert, nur Rechnung. `Wire.setWireTimeout` fehlt weiter (B-3, nicht Teil von REQ-004) |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1, 5, 6, 7, 9, 11, 12 | wie Runde 1 | Logik-Teil erfüllt, unverändert |
| AK-2, 3, 4, 8, 10, 14, 15 | ☐ nicht geprüft | wie Runde 1 (nur `.ino`, Wokwi nicht verfügbar, Board) |
| AK-13 | ☒ **nicht erfüllt** (1 von 7 Fällen) | Siehe Tabelle unten |

### AK-13 pro Fall (grösster Winkelfehler, Grenze 2,5°)

| Fall | Runde 1 | Runde 2 | Ergebnis |
|---|---|---|---|
| 59 s, Arm frei, hängend/hängend, Drift + | 2,634° | 2,311° | bestanden |
| 59 s, Arm frei, vorn/hängend, Drift - | 2,150° | 1,779° | bestanden |
| 59 s, Arm frei, oben/seitlich, Drift + | 2,396° | 2,261° | bestanden |
| 59 s, Arm frei, seitlich/vorn, Drift - | 1,868° | 1,797° | bestanden |
| 59 s, Arm frei, schräg/schräg, Drift + | 2,597° | 2,359° | bestanden |
| **59 s, Arm hängt die ganze Wartezeit still, Drift +** | 2,577° | **2,698°** | **nicht bestanden** |
| kurze Wartezeit 0,5 s, ohne Drift | 2,114° | 2,119° | bestanden |

In allen sieben Fällen liefern Still-, Bewegungs- und Abschluss-Prüfung `true`, die Lage ist immer bekannt.

**Ausdrücklich: Der Fall «Arm hängt die ganze Wartezeit still» ist als einziger noch über 2,5°** (2,70°). Er hat sich gegenüber Runde 1 sogar leicht verschlechtert (+0,12°), während sich alle Fälle mit freier Armbewegung um 0,1 bis 0,4° verbessert haben.

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 | ☑ wie Runde 1 | Logik und Fuzz bestanden |
| B-6 | ☑ teilweise geschützt | wie Runde 1. Nur noch der Fall «Arm still» liegt knapp über der Genauigkeits-Grenze |
| B-7 | ☐ nicht geprüft | wie Runde 1 (`.ino`, Wokwi/Board) |

## Fehler in Worten für den Developer

- **AK-13, nur noch ein Fall: Arm hängt während beider Wartezeiten (je 59 s) ganz still, der Gyro-Fehler wandert dabei um bis zu 0,05 °/s.** Der Winkel ist in der Alltagsbewegung danach um bis zu 2,70° falsch statt höchstens 2,5°. Das Nachlernen des Gyro-Fehlers in den Warte-Zuständen hat die Fälle mit freier Bewegung deutlich verbessert, diesen Fall aber nicht, er wurde etwas schlechter. Das passt zum Grenzfall «Drift um die Senkrechte» der Anforderung: Wenn der Arm still hängt, sieht die Schwerkraft keine Drehung um die Senkrechte, und das Nachlernen hat dort nichts, woran es sich orientieren kann. Der Fehler wird erst durch die Bewegung nach dem Einmessen korrigiert. Der Peak liegt bei etwa 8 s Alltagsbewegung, also noch bevor die Korrektur voll greift.
  - **Vermutete Ursache: unklar (Code oder Anforderung).** Die REQ nennt für diesen Grenzfall selbst einen Rest von bis zu ca. 4,6°, der bei Bewegung danach korrigiert wird, und verlangt trotzdem AK-13 mit Alltagsbewegung. Ob 2,5° beim Still-hängenden Arm mit Schlimmstfall-Drift (beide IMUs, Fehler heben sich nicht auf) mit nur Schwerkraft und Gyro erreichbar ist, ist offen. Der Engineer soll mit dem Team entscheiden: Code weiter verbessern (z. B. Nachlernen im Stillhalten anpassen, schnellere Korrektur nach dem Einmessen) oder die Anforderung für diesen Grenzfall lockern (z. B. 3° nach dem Einmessen mit Warten bei still hängendem Arm).

## Am echten Board prüfen

Unverändert wie Runde 1 (siehe `docs/tests/board-test-REQ-004.md`): Zeichen/Taste/Zeilenende, Zeiten und 300-ms-Stoss, Ignorieren während der Phase, 60-s-Abbruch, keine Warnung während dem Einmessen, IMU abziehen (`Grund: IMU`), 50 Hz. Zusätzlich sinnvoll: Einmessen mit etwa 1 Minute Wartezeit bei still gehaltenem Arm und danach den Winkel am bekannten Winkel (z. B. 90°) gegenprüfen.

## Hinweise für den Engineer

- **Flash 94 %** (30410 von 32256 Bytes, 1846 Bytes Reserve) und wächst. Für `docs/offene-hinweise.md`. Jede weitere Korrektur für AK-13 muss in diese Reserve passen.
- **Wokwi weiterhin nicht geprüft** (Kontingent). Alle `.ino`-Kriterien bleiben ohne automatischen Beleg.
- **Hand bei der 2. Bestätigung** ist laut REQ jetzt beliebig: entspricht dem Test, nichts angepasst.
- `Wire.setWireTimeout` fehlt weiter (B-3).
- Runde 2 von 3 verbraucht. Test-Korrekturen: 1 von 3.

## Fehlende Werkzeuge

Keine (`clang++` statt `g++`).
