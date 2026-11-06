# Testbericht REQ-002, Runde 2 (Prüfung)

<!-- Dateiname mit «-pruefung», weil REQ-002-runde-2.md schon in Git liegt und nicht überschrieben wird. -->

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-002](../anforderungen/REQ-002.md) |
| **Runde** | 2 von 3 |
| **Datum** | 2026-11-06 |
| **Code-Stand** | nicht committet (nur `logik.cpp` seit Runde 1 geändert: Achsen-Suche per Jacobi-Verfahren; `jointsense.ino` und `logik.h` unverändert, Zeitstempel 22:18 vor der letzten Prüfung) |
| **Gesamtergebnis** | nicht bestanden |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno, `--warnings all`) | ok | 0 Warnungen |
| 1b | Speicher | ok / Flash knapp | RAM 1155 von 2048 Bytes (56 %), frei 893. **Flash 28822 von 32256 Bytes (89 %)**: Risiko, wenig Platz für spätere REQs |
| 2 | `cppcheck` | ok | Nur 1 Stil-Hinweis (`pf` in `logik.cpp` Zeile 1081 könnte `const` sein), kein warning/error |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, g++ MSYS2) | **Fehler** | 6 Testdateien, 1 fehlgeschlagen. REQ-001 147/147, REQ-002 253/253, Drift 28/28, Hand 35/35, **Oberarm 29/31**, schiefes Einmessen 38/38. Genau gleich wie Runde 1 |
| 3b | Fuzz-Tests | ok | Alle Fuzz-Prüfungen in den Dateien bestanden |
| 4 | Wokwi | nicht geprüft (Team-Entscheidung) | |
| 5 | Bibliotheken | ok | Nur `Wire` (Core) und `math.h`, wie `docs/tech-stack.md` |
| 6 | Sicherheitsregeln | teilweise | Kein `String`, kein `new`/`malloc`, Pins über `pins.h`, NaN-/Unendlich-Prüfung vorhanden. `Wire.setWireTimeout` fehlt weiter (B-3, wie Runde 1, gehört laut REQ-002 nicht dazu) |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 bis AK-5, AK-8 bis AK-16 (PC-Teil) | erfüllt | wie Runde 1 (Tests unverändert, gleiche Ergebnisse) |
| AK-6 (Einmessen mit Oberarm-Kreisen) | **nicht erfüllt** | Siehe unten. Das Jacobi-Verfahren hat den Fehler **nicht** behoben |
| AK-7 a, b, c, e | erfüllt | wie Runde 1 |
| AK-7d (Oberarm still / 3° / nur Senkrechte) | erfüllt | Alle 12 Prüfungen ok: Abbruch, nie `true` nach Abbruch, alte Referenz bleibt. Auch die Wiederholung nach Abbruch klappt. Der Testabbruchgrund «Oberarm» selbst wird vom Test nicht einzeln geprüft (wie Runde 1) |
| Zeiten, Stösse, Meldungszeilen, Laufzeit auf AVR | nicht geprüft | nur am Board |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| NaN/Unendlich/Extremwerte | geschützt | Fuzz-Tests ok |
| B-3 (Lahmlegen) | teilweise | RAM ok, Zykluszeit und Laufzeit von `einmessenAbschliessen` nicht gemessen; `Wire.setWireTimeout` fehlt (anderes REQ) |

## Fehler in Worten für den Developer

- **AK-6:** Beugt der Unterarm genau 2 Mal um mindestens 30° und macht der Oberarm dabei **2 volle Umläufe** kleiner Kreise (Spannweite 15°), bricht das Einmessen bei **allen 5 Einbaudrehungen** ab, mit und ohne alte Referenz. Die Anforderung verlangt, dass es klappt (und der Winkel danach auf 2,5° stimmt). Das Verhalten ist unverändert zu Runde 1. Mit 1 Umlauf (15°) und mit 20°-Kreisen klappt es weiterhin. Bei 15°-Kreisen mit 2 Umläufen stimmt etwas in der Achsen-Bestimmung oder einer Plausibilitätsprüfung davor/danach nicht. Auffällig: Mehr Umläufe machen die Daten eigentlich besser, nicht schlechter. Vermutete Ursache: Code, aber **unklar**, welche Prüfung abbricht (Mindest-Belegung der schwächsten Achsrichtung 3000, Kipp-Prüfung des Oberarms oder eine Weg-/Spannweiten-Prüfung). Der Abbruchgrund ist aus den Tests nicht ablesbar.
- Zum Schwellwert `ACHSE_LAMBDA_MIN = 3000`: Bei den klappenden echten Kreisen (1 Umlauf 15°, 2 Umläufe 15° mit 20°-Kreisen, 20°) wird er erreicht, und er trennt zuverlässig vom stillen Oberarm (AK-7d bestanden, 0°, 3° und nur Senkrechte brechen ab). Ob er bei den 2×15°-Kreisen der Grund für den Abbruch ist, kann ich ohne Debug-Ausgabe nicht sagen. Bestätigung durchs Team bleibt offen.

## Am echten Board prüfen

- Laufzeit von `einmessenAbschliessen` auf dem AVR (Jacobi, 6x6, Gleitkomma): über Tests nicht messbar (g++ auf PC). Darf den Messzyklus nicht aufhalten (Einmessen blockiert sonst kurz die Messung). Mit Zeitstempel (`micros()`) am Board messen.
- Zeiten, Stösse, Phase 3, Meldungszeilen, echte Bedienung mit Oberarm-Kreisen.
- Flash 89 %: bei weiteren Änderungen im Auge behalten.

## Hinweise für den Engineer

- `docs/testberichte/REQ-002-runde-2.md` und `-runde-3.md` existierten schon (in Git, aus früherem Stand); darum diese Datei mit anderem Namen.
- `Wire.setWireTimeout` fehlt (B-3), bekannt.
- Wokwi nicht ausgeführt (Team-Entscheidung).
- Keine Test-Korrekturen gemacht, keine Tests geändert.

## Fehlende Werkzeuge

Keine. `clang++` fehlt, `g++` (MSYS2) genügt.
