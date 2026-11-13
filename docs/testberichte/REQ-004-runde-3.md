# Testbericht REQ-004, Runde 3

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-004](../anforderungen/REQ-004.md) |
| **Runde** | 3 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet |
| **Gesamtergebnis** | ☑ Logik bestanden, alle PC-Prüfungen grün. Die Kriterien, die nur das `.ino` betreffen, sind **nicht geprüft** (Wokwi nicht verfügbar) und gehören an das Board. |

Geändert seit Runde 2: `jointsense.ino` (`EINMESS_WARTEN_MAX_MS = 30000` und zwei Kommentare), `logik.h`/`logik.cpp` nur Kommentare, `docs/tech-stack.md`. Die Anforderung wurde auf höchste Wartezeit 30 s (AK-9) und je 29 s Wartezeit in AK-13 geändert, meine Tests dazu angepasst (keine Test-Korrektur, Stand 1 von 3).

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno), alle Warnungen als Fehler | ☑ ok | 0 Warnungen |
| 1b | Speicher | ☑ ok, Flash knapp | RAM 1180 Bytes (57 %), 868 Bytes frei. **Flash 30410 von 32256 Bytes (94 %)**, 1846 Bytes Reserve, gleich wie Runde 2 |
| 2 | `cppcheck` | ☑ ok | Kein error/warning. Derselbe Stil-Hinweis wie vorher (`pf`, `logik.cpp` Zeile 1221) |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, `clang++`) | ☑ ok | 9 von 9 Testdateien bestanden: REQ-001 (147), REQ-002 (253, 28, 35, 29, 38), REQ-003 (11), REQ-004 (124) und `test_REQ-004_warten` (35 von 35) |
| 3b | Fuzz-Tests | ☑ ok | bestanden |
| 4 | Wokwi | ☐ nicht geprüft | Kontingent aufgebraucht |
| 5 | Bibliotheken | ☑ nicht wiederholt (Runde 1 gilt) | Keine neue Bibliothek |
| 6 | Sicherheitsregeln | ☑ nicht wiederholt (Runde 1 gilt) | Seit Runde 2 nur Konstante und Kommentare geändert. `Wire.setWireTimeout` fehlt weiter (B-3, nicht Teil von REQ-004) |

**Code-Durchsicht AK-9:** In `jointsense.ino` ist `EINMESS_WARTEN_MAX_MS = 30000` (Zeile 57). Der Aufruf `wartezeitAbgelaufen(schrittStartMs, jetzt, EINMESS_WARTEN_MAX_MS)` (Zeile 169) gilt nur in den Warte-Zuständen 1 und 2, bei Ablauf folgt `einmessenAbbrechenMelden(EINMESSEN_GRUND_BESTAETIGUNG)`. `schrittStartMs` wird beim Eintritt in den Warte-Zustand auf `jetzt` gesetzt (Zeile 450). Stimmt mit der Anforderung überein. Das Verhalten im echten Ablauf bleibt am Board zu prüfen (B-8).

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1, 5, 6, 7, 11, 12 | wie Runde 1 | Logik-Teil erfüllt |
| AK-9 | ☑ Logik erfüllt, Ablauf nicht geprüft | `wartezeitAbgelaufen` mit Grenze 30 000 ms: 29 999 nicht, 30 000 und 30 001 abgelaufen, auch beim Überlauf von `millis()`. Der Wert im `.ino` stimmt (Code-Durchsicht). 30-s-Abbruch im Ablauf: Board B-8 |
| AK-2, 3, 4, 8, 10, 14, 15 | ☐ nicht geprüft | wie Runde 1 (nur `.ino`, Wokwi nicht verfügbar, Board) |
| AK-13 | ☑ erfüllt (PC-Simulation) | Alle 7 Fälle ≤ 2,5°, siehe Tabelle |

### AK-13 pro Fall (grösster Winkelfehler, Grenze 2,5°)

Runde 2 lief mit 59 s Wartezeit (damals Höchstwartezeit 60 s), Runde 3 läuft mit 29 s. Der Vergleich zeigt vor allem, wie viel die kürzere Wartezeit bringt. Der Code der Logik ist gleich wie in Runde 2.

| Fall | Runde 2 (59 s) | Runde 3 (29 s) | Ergebnis |
|---|---|---|---|
| frei, hängend/hängend, Drift + | 2,311° | 2,232° | bestanden |
| frei, vorn/hängend, Drift - | 1,779° | 1,798° | bestanden |
| frei, oben/seitlich, Drift + | 2,261° | 2,134° | bestanden |
| frei, seitlich/vorn, Drift - | 1,797° | 1,772° | bestanden |
| frei, schräg/schräg, Drift + | 2,359° | 2,117° | bestanden |
| **Arm hängt die ganze Wartezeit still, Drift +** | 2,698° (nicht bestanden) | **2,452°** | **bestanden, aber nur knapp (0,05° Reserve)** |
| kurze Wartezeit 0,5 s, ohne Drift | 2,119° | 2,119° | bestanden |

In allen sieben Fällen liefern Still-, Bewegungs- und Abschluss-Prüfung `true`, die Lage ist immer bekannt.

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 | ☑ geschützt (Logik, Fuzz), `.ino` nur Durchsicht | wie Runde 1 |
| B-6 | ☑ geschützt (Logik) | Jede Phase wird geprüft, alte Referenz bleibt bei Abbruch, Wartezeit begrenzt (30 s), Genauigkeit nach dem Warten in allen Fällen ≤ 2,5°. Verfrühte Bestätigung im Ablauf: nicht geprüft |
| B-7 | ☐ nicht geprüft | Zeichenfilter in der Logik ok. Rest steckt im `.ino`: Board |

## Fehler in Worten für den Developer

Keine Fehler im geprüften Umfang.

## Am echten Board prüfen

Siehe `docs/tests/board-test-REQ-004.md` (jetzt mit 30 s). Wichtig, weil Wokwi nicht verfügbar ist und das `.ino` sonst ohne Beleg bleibt:

- Enter, Leerzeichen und Taste im echten Seriellen Monitor (alle Zeilenende-Einstellungen), `" \r\n"` zählt nur einmal (AK-1, AK-2).
- Zeiten der drei Phasen, 300-ms-Warte-Stoss unterscheidbar von den anderen Stössen, Textzeilen genau einmal (AK-5 bis AK-8, AK-14).
- Bestätigung während einer Phase wird ignoriert (AK-3). 30-s-Abbruch (B-8, AK-9). Keine Überstreckungs-Warnung während dem Einmessen (AK-10).
- IMU abziehen in einer Phase und in einem Warte-Zustand: `Grund: IMU` schon nach einer fehlgeschlagenen Messung (AK-12).
- Zykluszeit 50 Hz bleibt (AK-15).
- Genauigkeit: Einmessen mit etwa 25 bis 29 s Wartezeit bei still gehaltenem Arm, danach den Winkel an einem bekannten Winkel gegenprüfen (B-12).

## Hinweise für den Engineer

- **AK-13 «Arm hängt still» besteht nur knapp** (2,452° bei Grenze 2,5°). Die Annahmen (Schlimmstfall-Drift von 0,05 °/s) stammen aus der REQ. Eine kleine Änderung (andere Lernrate oder Einbau) kann das kippen. Ein Board-Test bei still gehaltenem Arm ist darum sinnvoll.
- **Flash 94 %** (1846 Bytes Reserve). Neue Anforderungen (IMU-Bibliothek, Status-LED) könnten nicht mehr passen. Für `docs/offene-hinweise.md`.
- **`Wire.setWireTimeout` fehlt** weiter (B-3, bekannt seit REQ-001).
- **Wokwi nicht geprüft** (Kontingent). Alle `.ino`-Kriterien haben nur den Board-Test als Beleg. Die alten Wokwi-Szenarien sind veraltet (siehe `docs/wokwi/REQ-004-stoerfaelle.md`).
- Runde 3 von 3 verbraucht, aber ohne offene Fehler.

## Fehlende Werkzeuge

Keine (`clang++` statt `g++`).
