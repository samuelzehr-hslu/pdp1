# Testbericht REQ-005, Runde 1 (Wiederholung nach Test-Korrektur)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-005](../anforderungen/REQ-005.md) |
| **Runde** | 1 von 3 (Code unverändert, nur Tests korrigiert) |
| **Test-Korrekturen** | 1 von 3 verbraucht (im Auftrag des Engineers) |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet, `jointsense/` unverändert gegenüber dem ersten Bericht |
| **Gesamtergebnis** | ☒ nicht bestanden (1 Befund: AK-8 Beugung, Ursache vermutlich Code) |

## Was an den Tests geändert wurde (Test-Korrektur 1)

**Gruppe 1, Aufruf-Reihenfolge (nur Tests, keine Erwartung gelockert):** Überall wird jetzt wie im Sketch zuerst `einmessenAbschliessen`, danach `einmessenFortschritt` aufgerufen: Einmess-Ablauf-Helfer (`frei`), Fortschritts-Zahlen (jede Sekunde ausgewertet), «nie kleiner», Fuzz (zufällig und NaN in Schritt 2). «Erfolg nur, wenn alle vier Werte OK» wird jetzt so geprüft: Erfolg ⇒ `einmessenFehlenderGrund` ist KEINER; kein Erfolg ⇒ Fortschritt zeigt nie «alle vier OK». Neu: Vor der ersten Auswertung sind Beugung und Achse 0.

**Gruppe 2, alte REQ-002-Tests:** Als «ersetzt durch REQ-005 (zählt nicht)» markiert (werden angezeigt, zählen nicht als Fehler): REQ-002 AK-6 (5 Prüfungen mit festem Skript, gleichzeitig, 5,5 s), Hand genau 45° (1), Oberarm-Kreis genau 15° (2). REQ-002 AK-14 «Einmessen klappt trotz eingestreuter ungültiger Zyklen» **nicht gestrichen**, sondern mit Einmess-Vorspann nach AK-11 (2 Schritt-1-Sekunden, dann nacheinander beugen 50° ×2, Oberarm-Kreis 25°, Hand 60°, jede Sekunde auswerten, bis 30 s) gefahren: bestanden.

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1, 1b | Kompilieren, RAM | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Code unverändert: 0 Warnungen, Flash 99 % (32132 Byte), RAM 62 %. |
| 2 | cppcheck | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Keine Funde. |
| 3 | `alle_tests.py` (clang++) | **Fehler (2)** | 10 Testdateien, 1 mit Fehlern. 719 von 721 zählenden Einzelprüfungen bestanden. Dazu 8 Prüfungen «ersetzt durch REQ-005» (zählen nicht). |
| 3b | Fuzz | ok | Zufalls-Aufrufe und NaN/∞ mitten in Schritt 2: bestanden. |
| 4 | Wokwi | nicht geprüft | Team-Vorgabe (Kontingent). |
| 5, 6 | Bibliotheken, Sicherheitsregeln | nicht wiederholt (Ergebnis aus Runde 1 gilt) | `Wire.setWireTimeout` fehlt: steht in `docs/offene-hinweise.md`. |

## Test-Fehler und Code-Fehler

**Test-Fehler:** keine mehr (die 5 Prüfungen aus Gruppe 1 und die 9 aus Gruppe 2 sind erledigt).

**Code-Fehler (2 Prüfungen, ein Befund): AK-8, Beugung zeigt 0, obwohl gebeugt wurde.**
- Beobachtung: Nach der Umstellung auf «erst Auswerten, dann Fortschritt» liefert `beugungGrad` bei reinem Beugen (nichts anderes bewegt) immer noch 0:
  - 2 Mal 50° mit 1 Hz (2 s Bewegung + 0,5 s, Auswertung jede Sekunde) → Beugung 0 statt 50 (± 3°),
  - 4 Mal 20° mit 1 Hz (4 s + 0,5 s) → Beugung 0 statt 20 (± 3°).
- AK-8 erlaubt «vorläufige Achse, ± 3°», nicht «0 solange die Achse nicht steht».
- Hintergrund aus dem Code und aus anderen Tests: Im Code wird die Beugung nur nachgeführt, wenn die Achsen-Suche ein Modell findet (sonst werden neue Lagen verworfen). Beim reinen Beugen steht das Modell offenbar nicht. In den 30-s-Abbruchfällen («nur Beugen» ohne Oberarm und Hand) wird die Beugung aber irgendwann ≥ 30 erkannt (dort kommt der Grund «Oberarm» richtig heraus). Die Beugungs-Zahl erscheint also verspätet, nicht nie. **Wie spät, weiss ich nicht** (nicht gemessen).
- Folge für die Praxis: Wer zuerst nur beugt, sieht in der Fortschrittszeile erst einmal `Beugung 0/30`. Bei den vollständigen Läufen (AK-11: 18 von 18 klappen) stört das nicht, weil der Erfolg erst kommt, wenn alles passt.
- Ursache: **Code** (Beugung steht nicht rechtzeitig nach der ersten Auswertung). Möglich auch **Anforderung**, falls ein verspätetes Erscheinen gewollt ist: AK-8 nennt keine Zeitgrenze. Der Engineer entscheidet (Zeit nennen, z. B. «Beugung ≥ 30° steht spätestens N s nach dem Beugen in der Zeile», oder Code ändern).

## Akzeptanzkriterien

Nur Änderungen gegenüber Runde 1 ausführlich. «PC» = Logik-Tests, «Board» = nur am Board prüfbar.

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1, AK-2, AK-5, AK-7, AK-16 | offen (Board) | wie Runde 1 |
| AK-3 | erfüllt (PC) | wie Runde 1 |
| AK-4 | Zahl erfüllt (PC), Textzeile Board | wie Runde 1 |
| AK-6 | Warten **erfüllt (PC)**, Stoss/Texte/30 s Board | 29 s Warten mit freiem Bewegen: danach klappt Schritt 2, Winkel stimmt auf 2,5°. |
| AK-8 | **nicht erfüllt** | Hand, Oberarm, «nie kleiner», «vor der ersten Auswertung sind Beugung und Achse 0»: erfüllt. **Beugung (± 3°): nicht erfüllt** (siehe oben, 0 statt 50 und 0 statt 20). Achse: nicht 100 % bei Arm still, bei reinem Beugen und beim Schwenken (Achse < 100 ⇒ Grund Oberarm), 100 % nur zusammen mit dem Erfolg (kein Erfolg ohne vollständige Prüfung, in allen Läufen konsistent). |
| AK-9 | offen (Board) | wie Runde 1 |
| AK-10 | **erfüllt (PC)** | «Erfolg genau dann, wenn alle vier Werte OK» jetzt in allen 18 Läufen jede Sekunde bestanden (Test-Korrektur). Fehler nach dem Einmessen ≤ 2,5° (max. 2,27°). Vibration/Text Board. |
| AK-11 | erfüllt (PC) | wie Runde 1 (18 von 18, längster Schritt 2 18,0 s, grösster Winkelfehler 2,27°, **kleine Reserve 0,23°**). |
| AK-12 | erfüllt (PC) | wie Runde 1 (0 von 18 Erfolgen bei gleichzeitigen Bewegungen, nie falscher Winkel). |
| AK-13 | Grund-Logik erfüllt (PC); Zeit/Texte Board | wie Runde 1 |
| AK-14 | erfüllt (PC) | wie Runde 1; schiefes Einmessen 38 von 38. |
| AK-15 | Logik erfüllt (PC); Texte/Motor Board | wie Runde 1 |
| AK-17 | Speicher erfüllt, Zykluszeit Board | wie Runde 1 (Flash 99 %). |
| AK-18 | erfüllt (PC) | wie Runde 1 (Streuung 0,35°). |
| AK-19 | erfüllt | wie Runde 1 |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-6 | geschützt (am PC) | wie Runde 1; Fuzz mit korrigierter Reihenfolge ohne falschen Erfolg. Vorbehalt: kleine Reserve bei AK-11. |
| B-7 | Durchsicht ok, Board offen | wie Runde 1 |
| B-1 | geschützt | wie Runde 1 |

## Fehler in Worten für den Developer

- **AK-8 (Ursache: Code, möglicherweise Anforderung):** Wenn die Person nur den Ellbogen beugt (z. B. 2 Mal um 50° in etwa 2 Sekunden, oder 4 Mal um 20° in 4 Sekunden) und noch nichts anderes bewegt hat, zeigt die Fortschrittszeile nach den Auswertungen immer noch `Beugung 0`. Verlangt ist die echte Spannweite auf ± 3° genau, auch solange die Achse noch nicht eindeutig ist (vorläufige Achse). Bitte erst weitergeben, wenn der Engineer entschieden hat, ob dafür eine Zeitgrenze in die Anforderung kommt.

## Am echten Board prüfen

Wie Runde 1 (Checkliste `docs/tests/board-test-REQ-005.md`, angepasst). Zusätzlich: Beim reinen Beugen am Anfang schauen, wann die Beugungs-Zahl in der Zeile steigt (Verzögerung notieren).

## Hinweise für den Engineer

1. Die Entscheidungen aus Runde 1 sind umgesetzt. Test-Korrekturen: 1 von 3 verbraucht.
2. **Offener Befund:** Beugung steht bei reinem Beugen auf 0 (siehe oben). Ich weiss nicht, nach wie vielen Sekunden sie erscheint. Wenn du eine Messung willst («ab wann steht Beugung ≥ 30 bei nur Beugen»), brauche ich dafür einen ausdrücklichen Auftrag für eine Probe.
3. Reserve bei AK-11 klein (0,23°), Flash 99 %: wie Runde 1. `Wire.setWireTimeout` fehlt: in `docs/offene-hinweise.md`.
4. Die Zeile «Zusatz: mindestens die Hälfte der Läufe mit gleichzeitigen Bewegungen klappt» zeigt weiter HINWEIS (0 von 18). Nicht verlangt, zählt nicht.

## Fehlende Werkzeuge

Keine (clang++ statt g++, wie Runde 1). Wokwi absichtlich nicht benutzt.
