# Testbericht REQ-007, Runde 1

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-007](../anforderungen/REQ-007.md) |
| **Runde** | 1 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet (geändert gegenüber Commit `cb7d6a4`: `jointsense.ino`, `logik.cpp`, `logik.h` nur Kommentare, `tools/gui/jointsense_live.html`; `pins.h` und `diagram.json` unverändert) |
| **Gesamtergebnis** | ☒ bestanden am PC (alle Prüfungen grün, kein Fehler gefunden); AK-6 und AK-8 (echter Browser) nur am Board, noch offen |

Ausgangslage (vor dem Aufräumen) und alle Ergebnisse dieser Runde: `scratch/REQ-007-ausgangslage/` und `scratch/REQ-007-nachher/` (nicht im Repo).

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, `--warnings all`, Uno) beide Modi | ok | 0 Warnungen in beiden Modi (wie vorher). |
| 1b | Speicher | ok | GUI: Flash 31454 (vorher 31836, **-382**), RAM 1339 (65 %, gleich). Text: Flash 31892 (vorher 32252, **-360**), RAM 1322 (65 %, gleich). Die Flash-Reserve im Text-Modus ist jetzt 364 statt 4 Byte. |
| 2 | `cppcheck` 2.22.0 | ok | 0 Befunde (vorher 0). |
| 3 | `alle_tests.py` (clang++) | ok | 10 von 10 Testdateien, 723 von 723 Tests bestanden, 0 Fehler. Ausgabe Zeile für Zeile identisch mit der Ausgangslage (nach Sortieren, ohne Zeilenende-Unterschiede). |
| 3b | Fuzz-Tests | ok | Teil von Schritt 3 (gleiche Ergebnisse wie vorher). |
| 3c | Firmware-Ausgabe Zeile für Zeile (`ino_vergleich.py`) | ok | Die echte `.ino` mit Arduino-Attrappe auf dem PC, 3 feste Szenarien, beide Modi: alle 6 Ausgabedateien (Serielle Zeilen mit Zeit, Motor-PWM, Pin-Modi, Baudrate) **gleich**. Auch die I²C-Zugriffe (nur Hinweis) sind in allen 6 Läufen gleich. |
| 3d | GUI mit denselben Zeilen (`gui_vergleich.js`) | ok | Gleiche Zeilen in beide GUI-Versionen: Protokoll von Anzeige-Änderungen, Zeichenbefehle der Graphen (Prüfsumme je Bild), CSV-Aufnahme und Demo-Modus: **kein Unterschied** (22'896 Protokollzeilen, `diff -r` leer, 0 Laufzeitfehler). |
| 3e | GUI statisch (`gui_statisch.py`) | ok | Keine Internet-Adressen, keine externen Einbindungen, gleiche Element-Namen (33 und 31), gleiche 29 Farben, gleiche 5 Zeilenarten. Nur Hinweis: 5 neue Konstanten-Namen `FARBE_BLAU/GRAU/GRUEN/ORANGE/ROT` (die neuen Farbkonstanten, keine neue Farbe). |
| 4 | Wokwi | nicht geprüft | Team-Entscheid: AK-6 nur am Board, Wokwi-Kontingent aufgebraucht. Nichts gestartet. |
| 5 | Bibliotheken | ok | Nur `Wire` (Arduino-Core) und `math.h`. Nichts Neues, `docs/tech-stack.md` nicht betroffen. |
| 6 | Sicherheitsregeln | ok mit bekanntem Hinweis | Kein `String`, kein `new`/`malloc`, Pins nur über `pins.h`, `isnan`/`isinf` vor dem Warnen vorhanden. `Wire.setWireTimeout` fehlt weiterhin (war schon vorher so, steht in `docs/offene-hinweise.md`, nicht Teil von REQ-007). |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 | erfüllt | Alle 723 Tests bestanden wie vorher, Ausgabe identisch. |
| AK-2 | erfüllt | `logik.h` ohne Kommentare Zeichen für Zeichen gleich wie in `cb7d6a4` (Funktionen, `struct`s, Felder, `#define`s). Die Diff zeigt nur Kommentare (vier Stellen, siehe Durchsicht). In `logik.cpp` sind alle Nicht-`static`-Funktionen dieselben wie vorher, keine neue öffentliche Funktion. Neue innere Funktionen (z. B. `hoechstensEins`, `qVorneDrehen`, `scharnierDatenSammeln`) sind alle `static`. |
| AK-3 | erfüllt (am PC) | Gleiche Zeilen in beiden Modi in allen 3 Szenarien (Unsinn über Serial, Taste mit Prellen, Enter/Leertaste, Einmessen bis «Referenz gesetzt», alle Abbruchgründe ausser Oberarm und Hand, Überstreckung, Zittern, kurze Aussetzer, IMU-Ausfall mit Wiederkehr, Gyro am Anschlag). Gleiche Zeitpunkte, gleiche Zahlen, gleiche Motor-Änderungen, gleiche Baudrate (115200 bzw. 9600). Nicht belegt: echte Serielle Zeit am Board (siehe Board-Liste). |
| AK-4 | erfüllt | 0 Warnungen in beiden Modi, `cppcheck` 0 Befunde. |
| AK-5 | erfüllt | GUI 31454 / 1339 (Grenze 31836 / 1339), Text 31892 / 1322 (Grenze 32252 / 1322). |
| AK-6 | nicht geprüft (nur Board) | Rechenzeit lässt sich am PC nicht messen, Wokwi ist gesperrt. Siehe Board-Liste. Hinweis: Der Code wurde kleiner, die Zahl der I²C-Zugriffe pro Zyklus ist gleich geblieben (gleiche I²C-Aufrufe in den PC-Läufen), das spricht nicht für eine längere Zykluszeit. Das ist kein Beleg. |
| AK-7 | erfüllt | `pins.h` und `diagram.json` unverändert. Suchmuster und Durchsicht von `jointsense.ino` und `logik.cpp`: keine Pin-Nummer und keine I²C-Adresse als Zahl, Zugriffe über `PIN_...` und `IMU1_ADRESSE`/`IMU2_ADRESSE`. Die `0x1B`, `0x6B`, `0x3B` sind MPU6050-Register, keine Adressen. |
| AK-8 | am PC erfüllt, echter Browser offen | Mit denselben aufgezeichneten Zeilen (3 Szenarien plus Demo) zeigt die neue GUI-Datei exakt dieselben Werte, Texte, Farben, Klassen, Graph-Zeichenbefehle und CSV-Daten wie die alte. Keine neuen Bibliotheken oder Internet-Adressen. Aussehen (Pixel, Layout) und Web-Serial mit echtem Board: siehe Board-Liste. |
| AK-9 | erfüllt | Längste Funktion: `loop()` mit 60 Zeilen (vorher 212), danach `einmessenAbschliessen` mit 48 (vorher 82), `imuLesen` 41. Gezählt ohne Kommentare und Leerzeilen, beide `#if`-Zweige zusammen. Kommentare: siehe Durchsicht, alle deutsch und aktuell. |

## Durchsicht (AK-7, AK-9, Kommentare)

- **`logik.h`:** Änderungen nur in Kommentaren: Kommentar zu `EINMESSEN_GRUND_BESTAETIGUNG` (stimmt: `jointsense.ino` übergibt den Grund bei Zeitablauf, `einmessenGrund()` liefert ihn nie, siehe Code), `ruheZeit` und `lageLetzter` «zurzeit nicht benutzt, wird nur auf 0 gesetzt» (stimmt: in `logik.cpp` nur Zuweisungen von 0), Kommentar bei `einmessenGrund` («`jointsense.ino` braucht diese Funktion zurzeit nicht», stimmt: kein Aufruf in der `.ino`).
- **`jointsense.ino`:** Kopfkommentar beschreibt den aktuellen Ablauf (REQ-005) und ergänzt den Aufbau der Datei. Neue Funktionen heissen verständlich (z. B. `einmessZeitPruefen`, `einmessenWeiterfuehren`). Das Makro `MODUS_TEXT(guiText, monitorText)` wählt den Text beim Kompilieren, nicht zur Laufzeit. Der nicht gewählte Text kommt nicht ins Programm, das belegen die gleichen RAM-Werte und der kleinere Flash.
- **`logik.cpp`:** neue innere Funktionen mit deutschem Kommentar; keine englischen Kommentare gefunden (Suche nach englischen Wörtern in allen vier Dateien ohne Treffer). `NICHT_INLINE` (Attribut `noinline`) ist schon vorher benutzt worden und nicht neu.
- **GUI-Datei:** neue Farbkonstanten und Hilfsfunktionen, keine `http`/`src=`-Einbindung.
- Kein Hinweis auf veraltete Kommentare («Phase 1/2/3», «automatisch einmessen» o. ä.) gefunden.

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 Kaputte Eingaben über Serial | geschützt (wie vorher) | Szenario `normal` schickt 9 Zeichen Unsinn und 150 Zeichen am Stück (Empfangspuffer 64 Byte). Ausgabe Zeile für Zeile gleich wie vor dem Aufräumen. Dazu Fuzz-Tests unverändert bestanden. |
| B-2 Falsche Sensordaten | geschützt (wie vorher) | `isnan`/`isinf`-Prüfung noch im Code, Ausfall-Szenarien (IMU weg, Gyro am Anschlag) gleiche Ausgabe wie vorher, Fuzz bestanden. |
| B-3 Lahmlegen | wie vorher | RAM gleich, kein dynamischer Speicher. `Wire.setWireTimeout` fehlt weiterhin (nicht neu, nicht Teil dieser REQ). |
| B-4 Bibliotheken | geschützt (wie vorher) | Nichts Neues. |
| B-6 Falsche Einmessung | geschützt (wie vorher) | Alle Einmess-Tests und alle Abbruch-Szenarien gleiche Ergebnisse, Referenz bleibt bei Abbruch (gleiche Ausgabe). |
| B-7 Einmessen über USB | geschützt (wie vorher) | Szenario `normal` und `abbruch`: Enter/Leertaste in Schritt 1 und 2 werden ignoriert, Abbruch nach 30 s, gleiche Zeilen wie vorher. |

## Änderung an meinen eigenen Prüfwerkzeugen in dieser Runde

Zwei Fehler in meinen Prüfskripten, keine am Produkt-Code. Beide korrigiert, ohne zu ändern, was geprüft wird:
- `refactoring_pruefen.py` (AK-2, Zusatzprüfung): Innere Funktionen mit vorangestelltem Makro `NICHT_INLINE static ...` wurden fälschlich als «neu und öffentlich» gemeldet. Das Skript erkennt `static` jetzt im ganzen Funktionskopf. Ursache: Test.
- `ino_vergleich.py`: Funktionsköpfe über mehrere Zeilen (`einmessenWeiterfuehren`) wurden bei der automatischen Vorwärtsdeklaration nicht erkannt, der Übersetzer meldete «undeclared identifier». Ursache: Test (Arduino selbst macht das richtig). Die Ausgangslage wurde mit dem alten Skript erzeugt und ist weiter gültig: Ein Lauf mit dem neuen Skript auf dem unveränderten Code ergibt dieselben Dateien (deshalb ist der jetzige Vergleich gültig).

## Fehler in Worten für den Developer

Keine. Alle prüfbaren Kriterien sind erfüllt, am PC wurde kein Unterschied im Verhalten gefunden.

## Am echten Board prüfen

Für das Team, vor der Abnahme. Reihenfolge so, dass man mit einem Upload auskommt. Hochladen macht ein Mensch.

**A) AK-6 Rechenzeit (nur Board).** Nach Entscheid des Teams wird Wokwi nicht benutzt.
- [ ] A1: Firmware **vor** dem Aufräumen (Commit `cb7d6a4`, `GUI_MODUS 0`) hochladen, Seriellen Monitor auf 9600 Baud, 2 Minuten normal bewegen (beugen, Hand drehen, Oberarm bewegen, kurz überstrecken) und die Zeilen `Zyklus max: … us` notieren (grösster Wert). Währenddessen einmal Einmessen, denn die Auswertung in Schritt 2 ist der längste Zyklus.
- [ ] A2: Dasselbe mit der neuen Firmware (`GUI_MODUS 0`). Sollwert: unter 15000 us **und** höchstens 5 % über A1.
- [ ] A3: Falls der Sleeve schon mit `GUI_MODUS 1` getestet wird: dasselbe mit den `Z`-Zeilen (GUI zeigt «Zyklus max»). Auch hier unter 15 ms und höchstens 5 % mehr.
- Hinweis: Der Wert hängt von der Länge der Seriellen Zeilen ab (siehe offene Hinweise). Für den Vergleich beide Male dieselbe Bewegung und denselben Modus nehmen.

**B) AK-3 und AK-8 im echten Browser.**
- [ ] B1: `GUI_MODUS 1` hochladen, `tools/gui/jointsense_live.html` im Browser (Chrome/Edge) öffnen, Verbinden. Vergleichen mit der alten Version (aus Git): gleiche Anzeige für Statuskarte (6 Zustände: OK, ÜBERSTRECKT, EINMESSEN, SENSORFEHLER, NICHT EINGEMESSEN, KEINE DATEN), Chips, Graphen (Winkel, 4 IMU-Graphen), Einmess-Balken, Ereignisliste.
- [ ] B2: Farben und Texte gleich (Blau/Orange/Grün/Grau/Rot wie vorher), Farben der Graphen x blau, y orange, z lila.
- [ ] B3: Knöpfe `Verbinden`, `Einmessen`, `Aufnahme` (CSV wird heruntergeladen und lässt sich öffnen), `Pause`, Demo-Haken (Demo läuft ohne Arduino) funktionieren wie vorher.
- [ ] B4: Fenstergrösse ändern, Handy-Breite: nichts läuft über, Graphen werden neu gezeichnet.
- [ ] B5: IMU abstecken: orange Meldung und Lücke im Graphen, Anstecken: grün «wieder da».
- [ ] B6: Datenrate 50 ± 2 Hz (unverändert zu REQ-006).
- [ ] B7: `GUI_MODUS 0` hochladen: lesbare Texte, 9600 Baud, Taste und Enter wie vorher.

**C) Ganze Firmware nochmals am Arm (da REQ-002 bis REQ-005 noch nicht abgenommen).** Siehe `docs/tests/board-test-REQ-004.md`, `board-test-REQ-005.md`, `board-test-2026-11-13.md`: Einmessen, Warnung kommt bei Überstreckung, kein Fehlalarm bei Hand drehen, Fehlermuster bei abgestecktem Sensor.

## Hinweise für den Engineer

- Flash ist nach dem Aufräumen wesentlich kleiner (Text-Modus 364 Byte frei statt 4). Der Punkt «Flash 99 %» in `docs/offene-hinweise.md` ist damit entschärft (Rest-Reserve ca. 1 %). Weiter vorsichtig sein.
- `Wire.setWireTimeout` fehlt weiterhin (B-3): Flash reicht jetzt dafür. Eigene Anforderung oder `/setup-aenderung`, Team entscheidet.
- Neue Prüfwerkzeuge für künftige Refactorings (nicht im Repo-Ablauf `alle_tests.py`): `docs/tests/ino_host/` (Firmware-Ausgabe mit Arduino-Attrappe), `docs/tests/gui_host/` (GUI ohne Browser), `docs/tests/refactoring_pruefen.py`. Anleitung: `docs/tests/ino_host/README.md`. Die Ausgangslage liegt nur in `scratch/` (gitignored). Wird sie gelöscht, kann man sie mit einer Kopie von Commit `cb7d6a4` neu erzeugen.
- Der Vergleich der Seriellen Ausgabe ist ein PC-Lauf mit simulierter Zeit: Er belegt gleiche Zeilen, Zahlen, Reihenfolge, Zeitpunkte und Motor-Befehle, nicht die reale Geschwindigkeit des Boards.
- Nicht abgedeckt im PC-Vergleich: Abbruchgründe «Oberarm» und «Hand» beim Einmessen (sie lassen sich auch in `alle_tests.py` nicht gezielt am PC auslösen, siehe `docs/offene-hinweise.md`).

## Fehlende Werkzeuge

Keine. `g++` fehlt, `alle_tests.py` und die neuen Skripte nehmen `clang++` (normal auf Windows ARM). `wokwi-cli` ist vorhanden, wurde aber auf Team-Vorgabe nicht benutzt.
