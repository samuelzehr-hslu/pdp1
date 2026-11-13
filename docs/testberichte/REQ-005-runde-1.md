# Testbericht REQ-005, Runde 1

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-005](../anforderungen/REQ-005.md) |
| **Runde** | 1 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet |
| **Gesamtergebnis** | ☒ nicht bestanden (14 von 728 Einzelprüfungen schlagen fehl: 5 wegen unklarer Schnittstelle, 9 wegen zu strenger alter Tests; kein eindeutiger Code-Fehler, aber AK-8 ist nicht belegt) |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, `--warnings all`, Uno) | ok | 0 Warnungen. Flash 32132 von 32256 Byte (**99 %**, 124 Byte frei). |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ok | 1278 Bytes (62 %), 770 Byte für lokale Variablen frei. Grenze AK-17 (70 %) eingehalten. **Flash 99 % = Risiko** (Wunsch «höchstens 94 %» verfehlt, harte Grenze 32 256 gerade eingehalten). |
| 2 | Code-Scanner `cppcheck` | ok | Keine Funde. |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, Compiler `clang++`) | **Fehler** | 10 Testdateien, 5 mit Fehlern. 714 von 728 Einzelprüfungen bestanden, 14 fehlgeschlagen (Aufstellung unten). |
| 3b | Fuzz-Tests (zufällige und extreme Werte) | teilweise | Zufalls-Aufrufe (30 000, NaN/∞/riesig): bestanden, kein Absturz, nie falscher Erfolg. Eine Fuzz-Prüfung («NaN mitten in Schritt 2») schlägt fehl, Ursache wie Gruppe 1 unten. |
| 4 | Wokwi-Simulation inkl. Störfälle | nicht geprüft | Vom Team ausgeschlossen (Kontingent). |
| 5 | Bibliotheken (`docs/tech-stack.md`) | ok | Nur `Wire` (Core) und `math.h`, keine neue Bibliothek. Die Änderung in `tech-stack.md` betrifft nur Speicherzahlen und Rechenverfahren. |
| 6 | Sicherheitsregeln im Code | **teilweise** | Kein `String`, kein `new`/`malloc`; Pins nur über `pins.h`; NaN-/Unendlich-Prüfungen in `logik.cpp` vorhanden. **`Wire.setWireTimeout` kommt im Code gar nicht vor** (auch nicht in der Version vor REQ-005, also nicht neu; siehe Hinweise). |

## Fehlgeschlagene Prüfungen: Test-Fehler und Code-Fehler getrennt

Tippfehler in Tests wurden **nicht** gefunden, es wurde nichts an Testdateien geändert (Test-Korrekturen: 0 von 3 verbraucht).

### Gruppe 1: Fortschritt liest Werte, die erst `einmessenAbschliessen` nachführt (5 Prüfungen) — Ursache: unklar (Schnittstelle: Code oder Anforderung; der Test nimmt eine Reihenfolge an, die in der REQ nicht steht)

`einmessenFortschritt` gibt für **Beugung** und **Achse** nur die Werte weiter, die `einmessenAbschliessen` beim letzten Aufruf berechnet hat (Hand und Oberarm sind dagegen aktuell). Im Sketch ist das unproblematisch: dort ruft `loop()` einmal pro Sekunde erst `einmessenAbschliessen` und dann `einmessenFortschritt` auf, die Zeile ist also frisch. Meine Tests rufen aber `einmessenFortschritt` allein oder **vor** `einmessenAbschliessen` auf. Folgen:

| Prüfung | Beobachtung |
|---|---|
| AK-8: Beugung 50° hin und zurück (2 Mal), nur Fortschritt abgefragt | Beugung 0 statt ca. 50 (Toleranz ± 3°) |
| AK-8: 4 Mal 20° beugen | Beugung 0 statt ca. 20 |
| AK-10: «Erfolg genau dann, wenn alle vier Werte OK» (test_REQ-005, testErfolg) | Fortschritt vor dem Abschliessen zeigt noch «nicht alles OK», der Abschluss meldet danach Erfolg |
| AK-10: dieselbe Prüfung in allen 18 freien Läufen, jede Sekunde (test_REQ-005_frei) | gleiches Muster |
| Fuzz: NaN/∞ mitten in Schritt 2 «nie falscher Erfolg» | gleiches Muster (Fortschritt vor Abschluss abgefragt, dann Erfolg) |

Die REQ sagt bei `einmessenFortschritt` nur «Füllt die Werte aus AK-8» und nennt keine Aufruf-Reihenfolge. Entscheidung für den Engineer:
(a) Anforderung klarstellen: «Fortschritt zeigt den Stand der letzten Auswertung; erst `einmessenAbschliessen`, dann `einmessenFortschritt`». Dann werden meine 5 Prüfungen auf diese Reihenfolge umgestellt (2 Test-Korrekturen wären verbraucht, wenn zusammen 1 Korrektur pro Datei zählt; Engineer entscheidet). Oder
(b) Code ändern: `einmessenFortschritt` liefert auch allein aktuelle Beugung und Achse (dagegen spricht «leicht genug», und Flash ist bei 99 %).
**Wichtig:** Bis (a) oder (b) entschieden und neu geprüft ist, ist AK-8 («Beugung ± 3° von der echten Spannweite vor dem Erfolg») **nicht belegt**: Ich habe die Beugungs-Zahl nach einer Auswertung noch nicht gegen die echte Spannweite geprüft.

### Gruppe 2: Alte Einmess-Tests von REQ-002 auf dem neuen Ablauf (9 Prüfungen) — Ursache: Test (verlangen mehr als REQ-005)

Betroffen: `test_REQ-002` (AK-6 fünf Prüfungen, AK-14 «Einmessen klappt trotz eingestreuter ungültiger Zyklen»), `test_REQ-002_hand` (Hand genau 45°), `test_REQ-002_oberarm` (Oberarm-Kreis genau 15°, 2 Prüfungen).
Diese Tests (von mir in Auftrag 1 auf den neuen Ablauf umgestellt) spielen das **alte feste Bewegungs-Skript**: Beugen und Oberarm-Kreis **gleichzeitig** 4 s lang, dann 1,5 s Hand drehen, und verlangen Erfolg innerhalb von **5,5 s** Schritt 2, teilweise mit **genau** den Minimalwerten (Hand 45°, Oberarm 15°).
REQ-005 verlangt etwas anderes: AK-11 (Reihenfolge egal, Beugen ≥ 40°, Oberarm-Kreis ≥ 20°, Hand ≥ 60°, bis 30 s), und AK-12 sagt ausdrücklich, dass ein Erfolg nur mit gleichzeitigen Bewegungen **nicht verlangt** ist. Die Tests sind also strenger als die Anforderung (Zeit 5,5 s statt 30 s, gleichzeitig, Minimalwerte). Das Ergebnis ist trotzdem ein Hinweis: Der echte Code schafft die alte Choreografie mit gleichzeitigem Beugen und Oberarm-Kreis nicht mehr (siehe auch «Hinweise»). Abgesichert: ohne Erfolg gibt es in diesen Fällen keinen falschen Winkel (Abbruch statt Raten). Empfehlung: Engineer entscheidet, ob ich diese Tests auf die Bewegungspläne von AK-11 (nacheinander, 30 s, ≥ 20° / ≥ 60°) umstelle (das wäre eine Test-Korrektur im Auftrag des Engineers). Sicher bin ich nicht, ob der Code mit der längeren Zeit klappen würde: Das ist nicht ausprobiert.

## Akzeptanzkriterien

Kurz: «PC» = mit Logik-Tests belegt. «Board» = nur am Board prüfbar (Checkliste `docs/tests/board-test-REQ-005.md`). Bei «Board» habe ich nur den Sketch gelesen (Durchsicht), das ist **kein** Bestanden.

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 Neigungs-Anzeige | offen (Board) | Durchsicht: Zeile wird mit `neigungAusBeschleunigung` gefüllt, nur ohne Referenz, ohne Einmessen, ohne Ausfall, Messung höchstens 100 ms alt, alle 1000 ms. Sieht richtig aus. Am Board prüfen. |
| AK-2 Start, Text Schritt 1 | offen (Board) | Durchsicht ok (Text stimmt). |
| AK-3 Ruhe 2000 ms, beide IMUs ≤ 10 °/s, Wackeln = neu zählen, Referenz aus Ruhe | **erfüllt (PC)** | Schritt 1 nach 100 Zyklen = 2000 ms. Länge des Gyro-Vektors zählt (Grenzfälle 9,5 / 10,4 ok), ein Wackler setzt zurück, kein Abbruch durch die Logik, 30 s Wackeln nie fertig. Referenz stimmt auch nach 3 s Wackeln davor. |
| AK-4 «Ruhig: x.x s» | Zahl erfüllt (PC), Textzeile offen (Board) | `ruhigSekunden` nach 1000 ms ≈ 1,0. |
| AK-5 30 s Abbruch «ruhig» | offen (Board) | Durchsicht ok. |
| AK-6 Warte-Zustand, 29 s Warten verschlechtert nichts | Warten **erfüllt (PC)**, Stoss/Texte/30 s offen (Board) | 29 s Warten mit freiem Bewegen, danach Einmessen: klappt, Winkel stimmt auf 2,5° (zählt jetzt als normale Prüfung, nicht mehr als «Zusatz»). |
| AK-7 Schritt 2 startet, Text, Ausgangslage | offen (Board), Logik gelesen | Durchsicht ok: kein Stoss, Text stimmt. |
| AK-8 Fortschritt (4 Werte) | **nicht belegt** | Hand (≈ 60 bei 60°, ≈ 30 bei 30°), Oberarm (kleinere Richtung: 0 bei einer Richtung, 38,7 beim 40°-Kreis) und «nie kleiner» sind **erfüllt**. Beugung (± 3°) und Achse sind wegen Gruppe 1 **nicht geprüft**; Ursache unklar (Schnittstelle). |
| AK-9 Fortschritts-Zeile | offen (Board) | Durchsicht ok: ganze Zahlen abgerundet, « OK» dahinter. |
| AK-10 Erfolg, Referenz, Winkel danach ≤ 2,5° | **erfüllt (PC)**; «genau dann»-Prüfung offen | Normalfall: Schritt 1 = 2,00 s, Schritt 2 bis Erfolg = 6,0 s, Fehler danach max. 1,68°. Die Prüfung «Erfolg genau dann, wenn alle vier Werte OK» scheitert nur wegen Gruppe 1. Vibration/Text: Board. |
| AK-11 Reihenfolge egal | **erfüllt (PC)** | 18 von 18 Läufen (3 Einbaudrehungen, Reihenfolge, Pausen, Zusatzbewegungen, 0,3–1,5 Hz, Sensorfehler): längster Schritt 2 = 18,0 s, grösster Winkelfehler danach 2,27°. **Reserve zur Grenze 2,5° ist klein (0,23°).** |
| AK-12 Gleichzeitige Bewegungen | **erfüllt (PC)** | Nie ein Erfolg mit falschem Winkel (0 von 18 Läufen mit Erfolg, das ist erlaubt). Siehe Hinweis zur Praxis. |
| AK-13 Abbruch nach 30 s, Grund = erster fehlender Wert | Grund-Logik **erfüllt (PC)**; Zeit und Texte offen (Board) | Alle Fälle richtig: Beugung zu klein, keine Bewegung, nur 20°, Oberarm still / 3° / nur eine Richtung / nur geschwenkt, Hand nicht / 2° / 30°. Achse < 100 % ergibt Oberarm. |
| AK-14 Nie raten | **erfüllt (PC)** | 30 s ohne Bewegung: nie Erfolg; Oberarm ≤ 3° und Hand ≤ 3° nie Erfolg; falsche Aufruf-Reihenfolgen geben nie eine Referenz. Schiefes Einmessen bis 10° (`test_REQ-002_schief`): 38 von 38 bestanden. `Grund: schief` nicht umgesetzt (erlaubt). |
| AK-15 Abbruch: alte Referenz/Achse bleiben | Logik **erfüllt (PC)**; Texte/Motor offen (Board) | Abbruch in Schritt 1, Warte-Zustand, Schritt 2, mit und ohne alte Referenz: Winkel bleibt auf 2,5° richtig, ohne alte Referenz bleibt «keine Abweichung», sofort wieder startbar. |
| AK-16 Keine Überstreckungs-Warnung | offen (Board) | Durchsicht: Warnung wird beim Start abgeschaltet, Motor nur beim Warte-Stoss und bei der Bestätigung. |
| AK-17 Tempo und Speicher | Speicher **erfüllt**, Zykluszeit offen (Board) | Flash 32132 ≤ 32256, RAM 62 % ≤ 70 %, kein `String`/`new`. **Flash 99 %** (Wunsch 94 % verfehlt, nur 124 Byte Reserve). Zykluszeit ≤ 15 ms normal und ≤ 100 ms (einmal pro Sekunde, nur Schritt 2) nur am Board messbar, Checkliste angepasst. |
| AK-18 Wiederholgenauigkeit | **erfüllt (PC)** | 10 Einmessungen: Streuung 0,35° (Grenze 3°). |
| AK-19 QA-02 in der README | erfüllt | README Zeile QA-02 hat den neuen Text. |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-6 (falsche Einmessung) | geschützt (am PC) | Nie Erfolg ohne Oberarm-/Handbewegung, nie Erfolg mit falschem Winkel bei gleichzeitigen oder geschwenkten Bewegungen, nie Referenz bei falscher Aufruf-Reihenfolge, Fuzz ohne falschen Erfolg. Vorbehalt: Reserve bei AK-11 nur 0,23°. |
| B-7 (Einmessen über USB blockieren) | teilweise (Durchsicht), Board offen | Durchsicht: Zeitgrenzen 30 s in allen Zuständen, Zeichen-Regeln unverändert, Bestätigung nur im Warte-Zustand wirksam. Test am Board nötig. |
| B-1 (kaputte Eingaben) | geschützt (wie bisher) | Fuzz mit NaN/∞/riesigen Werten ohne Absturz. |

## Fehler in Worten für den Developer

(Noch nichts an den Developer weitergeben, bevor der Engineer die Fragen unter «Hinweise» entschieden hat.)

- **Mögliche Code-Frage (AK-8, Ursache unklar):** Die Fortschrittswerte für Beugung und Achse werden nur bei der Auswertung nachgeführt. Fragt man den Fortschritt ohne vorherige Auswertung ab, zeigt die Beugung 0 und die Achse 0 %, auch wenn der Arm längst gebeugt wurde. Im Sketch (erst auswerten, dann anzeigen) fällt das nicht auf. Falls verlangt wird, dass der Fortschritt auch allein aktuell ist, müsste er Beugung und Achse selbst nachführen.
- **Hinweis zur Praxis (AK-12 / REQ-002-Altfälle):** Wenn Beugen und Oberarm-Kreis gleichzeitig passieren (wie die alte Einmess-Choreografie), kommt das Gerät in den 5,5-s-Fällen und in allen 18 «gleichzeitig»-Läufen nie zum Erfolg. Das ist nach AK-12 erlaubt (kein falscher Winkel), aber am echten Arm bewegen Menschen oft gleichzeitig.

## Am echten Board prüfen

Alles in `docs/tests/board-test-REQ-005.md` (angepasst: AK-17 jetzt 100 ms einmal pro Sekunde; neu: 29 s Warten mit freiem Bewegen). Besonders:

- Texte und Zeiten von AK-1, AK-2, AK-4 bis AK-7, AK-9, AK-13, AK-15, AK-16.
- **Zykluszeit in Schritt 2** (`Zyklus max`, ≤ 100 ms) und dass IMU-Ausfall währenddessen erkannt wird.
- Echte Bedienung am Arm: Wie lange dauert es, welche Zahl ist der Engpass, klappt es «schlampig», und was passiert bei gleichzeitigem Beugen und Oberarm-Kreisen?
- Flash-Reserve: nur 124 Byte.

## Hinweise für den Engineer

1. **Entscheidung nötig (Gruppe 1):** Soll `einmessenFortschritt` auch allein aktuelle Beugung/Achse liefern, oder gilt «erst `einmessenAbschliessen`, dann `einmessenFortschritt`» (dann REQ klarstellen und meine 5 Prüfungen umstellen)? Ich empfehle (a) Anforderung klarstellen, weil das dem Sketch entspricht und Flash spart. Danach wäre AK-8 (Beugung ± 3°) noch zu prüfen: bisher nicht belegt.
2. **Entscheidung nötig (Gruppe 2):** 9 Prüfungen alter REQ-002-Tests (fester Ablauf, gleichzeitig, 5,5 s, Minimalwerte) sind strenger als REQ-005. Vorschlag: auf die Pläne von AK-11 umstellen (Test-Korrektur im Auftrag, zählt zu den 3) oder als «ersetzt durch REQ-005» streichen. Ich habe nichts geändert.
3. **Reserve AK-11 klein:** grösster Winkelfehler 2,27° bei Grenze 2,5°. Bei anderen Zufallswerten oder echten Sensoren kann es kippen.
4. **Flash 99 %** (124 Byte frei): jede weitere Funktion sprengt den Uno. Team entscheidet bei der Abnahme (Wunsch war ≤ 94 %).
5. **`Wire.setWireTimeout` fehlt im Code** (schon vor REQ-005, kein neuer Fehler). Die Sicherheitsregel aus der Prüfkette ist damit nicht erfüllt. Entweder war das bewusst (REQ-001 erkennt Ausfall anders; dann Regel anpassen), oder es fehlt. Bitte klären (`docs/offene-hinweise.md`).
6. **Gleichzeitige Bewegungen führen nie zum Erfolg** (0 von 18). Für die Praxis wichtig: In der Anleitung sagen «Bewegungen nacheinander machen». Die Fortschrittszeile im Monitor zeigt dann, was fehlt.
7. Wokwi-Szenarien (W1 bis W6) sind weiter auf dem alten Einmessen aufgebaut (`docs/offene-hinweise.md`), nicht geprüft.

## Fehlende Werkzeuge

Keine. `arduino-cli`, `clang++` (kein `g++` auf diesem PC, ARM-Windows), `cppcheck` und `python` waren vorhanden. `wokwi-cli` wurde absichtlich nicht benutzt (Kontingent).
