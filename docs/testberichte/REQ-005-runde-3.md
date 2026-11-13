# Testbericht REQ-005, Runde 3 (letzte Runde)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-005](../anforderungen/REQ-005.md) |
| **Runde** | 3 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet (geändert seit Runde 2: `logik.cpp`, `jointsense.ino`, `docs/tech-stack.md`) |
| **Gesamtergebnis** | ☒ bestanden am PC (alle zählenden Prüfungen grün); Board-Prüfungen offen; Flash-Reserve 4 Byte |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, `--warnings all`, Uno) | ok | 0 Warnungen. Flash 32252 von 32256 Byte (**99 %, nur 4 Byte frei**), bestätigt. |
| 1b | Speicher: RAM | ok | 1322 Byte (64 %), 726 Byte für lokale Variablen frei. Grenze 70 % eingehalten. **Flash-Reserve 4 Byte = hohes Risiko** (siehe Hinweise). |
| 2 | cppcheck | ok | Keine Funde. |
| 3 | `alle_tests.py` komplett (REQ-001 bis REQ-005, clang++) | ok | 10 von 10 Testdateien bestanden, 723 von 723 zählenden Prüfungen. Dazu 8 Prüfungen «ersetzt durch REQ-005» (zählen nicht, wie bisher). |
| 3b | Fuzz | ok | Zufalls-Aufrufe und NaN/∞ mitten in Schritt 2 bestanden. |
| 4 | Wokwi | nicht geprüft | Team-Vorgabe (Kontingent). |
| 5 | Bibliotheken | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Keine neue Bibliothek. |
| 6 | Sicherheitsregeln | nicht wiederholt, Stichprobe ok | Kein `String`/`new`/`malloc`; `Wire.setWireTimeout` fehlt weiter (steht in `docs/offene-hinweise.md`, nicht Teil dieser Prüfung). |

## Durchsicht `jointsense.ino`: Obergrenze 1000 in `wertMelden`

Geprüft, ob die neue Obergrenze (1000 statt 100000) irgendeine Ausgabe falsch macht. **Ergebnis: keine Ausgabe der Zeilen aus REQ-001 bis REQ-005 wird falsch.**
- `wertMelden` wird **nur** für die drei Grad-Werte der Fortschrittszeile gebraucht (`Beugung`, `Oberarm`, `Hand`), nirgends sonst.
- `Zyklus max: <n> us` (z. B. 15000 oder 100000) wird mit `Serial.print(zyklusMaxUs)` direkt geschrieben und ist **nicht betroffen**. Ebenso `Achse <a>%` (0 bis 100, direkt), `Ruhig: x.x s`, `Neigung …`, `Abweichung …` (alle über `zahlSchreiben`, ohne Obergrenze) und die FEHLER-/Abbruch-Zeilen.
- Die drei Grad-Werte liegen im Betrieb unter 360°: Beugung und Oberarm sind Spannweiten von Winkeln, Hand ist der Scheitelwert der Handdrehung (hin und zurück hebt sich auf).
- Namen und Zeichen der Zeile (`Beugung `, `  Oberarm `, `  Hand `, `<wert>/<ziel>`, ` OK`) sind im Code zeichengleich zur Vorgabe aus AK-9.
- **Einzige Randerscheinung (kein Fehler):** Dreht die Hand in einer Richtung mehr als 1000° (fast 3 volle Umdrehungen, für einen Unterarm nicht möglich), schreibt die Zeile `Hand 0/45 OK` statt der grossen Zahl. Nur die Anzeige betrifft das; die Erfolgs-Entscheidung kommt aus der Logik und nicht aus dieser Funktion.
- Auch ein Wert über 1000 für Beugung/Oberarm tritt physikalisch nicht auf.

## Winkelfehler, Reserve

**Nicht schlechter geworden, alle Zahlen identisch mit Runde 1 und 2:**
- AK-11/AK-12: grösster Winkelfehler nach Erfolg **2,267°** (Reserve weiter nur **0,23°** zur Grenze 2,5°). 18 von 18 Läufen, längster Schritt 2 = 18,0 s.
- AK-18: Streuung der 10 Referenzen 0,349°.
- REQ-002: AK-4 Einbaudrehung 2,028°, AK-10/11 1,985°/1,960°, schiefes Einmessen 1,991°.

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1, AK-2, AK-5, AK-7, AK-16 | offen (Board) | Sketch gelesen, sieht richtig aus. Nur Board belegt es. |
| AK-3 | erfüllt (PC) | wie Runde 1 |
| AK-4 | Zahl erfüllt (PC), Textzeile Board | wie Runde 1 |
| AK-6 | Warten erfüllt (PC), Stoss/Texte/30 s Board | wie Runde 1 |
| AK-8 | **erfüllt (PC)** | Beugung steht spätestens 2000 ms nach der Bewegung auf 50 bzw. 20 (± 3°); vor der ersten Auswertung ist die Achse 0 % und die Beugung nie über der echten Spannweite; Oberarm, Hand und Beugung hängen nur von der eigenen Bewegung ab; «nie kleiner» erfüllt. Zeile (Format, ganze Zahlen) siehe AK-9. |
| AK-9 | offen (Board) | Durchsicht: Format, abgerundet, « OK» richtig (siehe Durchsicht oben). |
| AK-10 | **erfüllt (PC)** | Erfolg nur mit echter Spannweite (Befund aus Runde 2 behoben: 4 Mal 20° gleichzeitig mit Oberarm-Kreis und Hand bricht wieder ab). Winkelfehler ≤ 2,5° (max. 2,27°). Vibration/Text: Board. |
| AK-11 | erfüllt (PC) | 18 von 18, ≤ 18,0 s, Fehler max. 2,27° (Reserve **0,23°**, unverändert). |
| AK-12 | erfüllt (PC) | 0 von 18 Erfolge bei gleichzeitigen Bewegungen, nie falscher Winkel. |
| AK-13 | Grund-Logik erfüllt (PC), Zeit/Texte Board | wie Runde 1 |
| AK-14 | **erfüllt (PC)** | Nie Erfolg ohne Oberarm-/Handbewegung, nie Erfolg mit zu kleiner Beugung, schiefes Einmessen 38 von 38. |
| AK-15 | Logik erfüllt (PC), Texte/Motor Board | wie Runde 1 |
| AK-17 | Speicher erfüllt, Zykluszeit **offen (Board)** | Flash 32252 ≤ 32256 (4 Byte Reserve; Wunsch «höchstens 94 %» verfehlt), RAM 64 % ≤ 70 %, kein `String`/`new`. Zykluszeit (≤ 15 ms normal, ≤ 100 ms einmal pro Sekunde in Schritt 2) nur am Board messbar. |
| AK-18 | erfüllt (PC) | Streuung 0,349°. |
| AK-19 | erfüllt | wie Runde 1 |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-6 (falsche Einmessung) | geschützt (am PC) | Befund aus Runde 2 behoben; nie Erfolg ohne Oberarm/Hand/echte Beugung; nie falscher Winkel bei gleichzeitigen Läufen; Fuzz ohne falschen Erfolg. Vorbehalt: kleine Winkel-Reserve (0,23°). |
| B-7 | Durchsicht ok, Board offen | 30-s-Grenzen in allen Zuständen, Zeichen-Regeln unverändert. |
| B-1 | geschützt | Fuzz mit NaN/∞/riesigen Werten ohne Absturz. |

## Fehler in Worten für den Developer

Keine.

## Am echten Board prüfen

Checkliste `docs/tests/board-test-REQ-005.md` (alle Abschnitte A bis F). Besonders:
- Zykluszeit in Schritt 2 (≤ 100 ms, einmal pro Sekunde) und IMU-Ausfall-Erkennung währenddessen.
- Texte und Zeiten von AK-1, AK-2, AK-4 bis AK-7, AK-9, AK-13, AK-15, AK-16.
- Echte Bedienung am Arm: Dauer, Engpass, «schlampig» bewegen, gleichzeitiges Beugen und Oberarm-Kreisen, 29 s Warten mit Bewegung, Beugungs-Zahl steigt beim ersten Beugen (≤ 2 s).
- Kompilieren auf dem Rechner, der hochlädt: Flash hat nur 4 Byte Reserve.

## Hinweise für den Engineer

1. **Flash 4 Byte frei (99,99 %).** Formal erfüllt (≤ 32256), aber jede Änderung (ein Wort im Text, andere `arduino-cli`-/Core-Version, eine neue Funktion) kann den Uno sprengen. Vor der Abnahme: gleiche Werkzeug-Version wie beim Hochladen prüfen. Für weitere Arbeit am Code: zuerst Flash schaffen (z. B. Nano-Board mit mehr Flash, oder Funktionen kürzen); das Team entscheidet.
2. **Reserve AK-10/AK-11 weiter 0,23°** (2,27° von 2,5°). Mit anderen Zufallswerten oder echten Sensoren kann sie kippen. Am Board darauf achten (10 Einmessungen, Winkel lesen).
3. **Anzeige-Grenze 1000 ist unkritisch** (siehe Durchsicht); nur bei > 1000° Handdrehung zeigt die Zeile `Hand 0/45 OK`.
4. **Gleichzeitige Bewegungen führen nie zum Erfolg** (0 von 18). Erlaubt nach AK-12. In der Bedienanleitung «nacheinander bewegen» erwähnen.
5. Die 8 als «ersetzt durch REQ-005» markierten Prüfungen (alte REQ-002-Tests) zählen nicht. Die Wokwi-Szenarien W1 bis W6 sind weiter auf dem alten Einmessen aufgebaut (`docs/offene-hinweise.md`).
6. Nach der Abnahme am Board: `Wire.setWireTimeout` (offene-hinweise) bleibt zu klären.

## Fehlende Werkzeuge

Keine (clang++ statt g++ auf diesem PC). Wokwi absichtlich nicht benutzt.
