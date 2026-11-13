# Testbericht REQ-005, Runde 2

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-005](../anforderungen/REQ-005.md) |
| **Runde** | 2 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet (geändert seit Runde 1: `logik.h`, `logik.cpp`, `docs/tech-stack.md`; `jointsense.ino` unverändert) |
| **Gesamtergebnis** | ☒ nicht bestanden (2 Befunde: ein Sicherheitsbefund B-6, ein Widerspruch REQ/Code) |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, `--warnings all`, Uno) | ok | 0 Warnungen. Flash 32220 von 32256 Byte (**99 %, nur 36 Byte frei**), bestätigt. |
| 1b | Speicher: RAM | ok | 1322 Byte (64 %), 726 Byte für lokale Variablen frei (Runde 1: 62 %). Grenze 70 % eingehalten. **Flash-Reserve 36 Byte = Risiko.** |
| 2 | cppcheck | ok | Keine Funde. |
| 3 | `alle_tests.py` komplett (REQ-001 bis REQ-005, clang++) | **Fehler** | 10 Testdateien, 2 mit Fehlern (`test_REQ-002`, `test_REQ-005`). 8 Prüfungen «ersetzt durch REQ-005» (zählen nicht, wie in der Wiederholung von Runde 1). Zählende Prüfungen: 2 fehlgeschlagen. REQ-001, REQ-003, REQ-004 und die drei weiteren REQ-002-Dateien (drift, hand, oberarm, schief): alle bestanden. |
| 3b | Fuzz | ok | Zufalls-Aufrufe und NaN/∞ mitten in Schritt 2 bestanden. |
| 4 | Wokwi | nicht geprüft | Team-Vorgabe (Kontingent). |
| 5 | Bibliotheken | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Keine neue Bibliothek. |
| 6 | Sicherheitsregeln | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Kein `String`/`new`/`malloc` in den geänderten Dateien; `Wire.setWireTimeout` fehlt weiter: `docs/offene-hinweise.md`. |

## Winkelfehler, Reserve (Frage des Engineers)

**Nicht schlechter geworden.** Alle Reserve-Zahlen sind mit Runde 1 identisch (auf drei Stellen):
- AK-11/AK-12 (REQ-005): grösster Winkelfehler nach Erfolg **2,267°** (Reserve weiter nur **0,23°** zur Grenze 2,5°); 18 von 18 Läufen klappen; längster Schritt 2 = 18,0 s.
- AK-18: Streuung der 10 Referenzen 0,349°.
- REQ-002: AK-4 Einbaudrehung 2,028°, AK-10/11 1,985°/1,960°, Hand 45/60/90°: 1,743/1,890/1,876°, schiefes Einmessen 1,991°.

## Fehlgeschlagene Prüfungen (2)

### Befund 1 (Sicherheit, B-6): Erfolg mit zu kleiner Beugung — Ursache: Code

Prüfung (REQ-002 AK-7a, Altfall mit festem Skript): Der Arm beugt 4 Mal um nur 20° hin und zurück (Spannweite 20°, Weg 160°), bewegt dabei den Oberarm im Kreis (20°) und dreht danach die Hand um 60°. **Das Einmessen meldet jetzt Erfolg** (Referenz gesetzt). Es muss abbrechen, weil die Beugung unter 30° bleibt. In Runde 1 brach dieser Fall richtig ab (er bestand, allerdings nur, weil damals dieses Skript nie zum Erfolg kam).
- Verletzt: AK-8 (Beugung = Spannweite, ± 3°), AK-10 («Erfolg nur, wenn Beugung OK»), AK-14/B-6 («richtig oder Abbruch, nie raten»).
- Dieselbe Beugung allein (4 Mal 20°, nichts anderes bewegt) und «Beugung 10°» zusammen mit Oberarm und Hand brechen weiter richtig mit Grund Beugung ab (REQ-005-Tests bestanden). Auffällig ist also nur das **gleichzeitige** Bewegen von Beugen und Oberarm-Kreis.
- Vermutung (nicht belegt): Die neue «vorläufige Beugung» aus dem Gyro des Unterarms rechnet bei gleichzeitigem Oberarm-Kreis zu viel (Beugung ≥ 30 gemeldet, obwohl die echte Spannweite 20° ist). Da sich der Beugungs-Wert nie verkleinert, bleibt der Überschuss stehen.
- Auswirkung: Ein Einmessen mit zu wenig Beugung könnte als gültig durchgehen. Dann wäre die Achse für kleine Beugungen weniger gut bestimmt.

### Befund 2: «Vor der ersten Auswertung sind Beugung und Achse 0» — Ursache: Anforderung (Widerspruch zum neuen Code), Entscheidung Engineer

Prüfung nach dem Satz in der Schnittstelle («Vor der ersten Auswertung in Schritt 2 sind diese beiden Werte 0»): Nach 2,4 s Beugen ohne Aufruf von `einmessenAbschliessen` zeigt der Fortschritt jetzt nicht mehr 0 für Beugung (und/oder Achse). Das ist die Folge der neuen vorläufigen Beugung (aktuell schon im Zyklus). Es ist besser fürs Team (die Zahl steigt sofort), aber der Satz in der REQ stimmt nicht mehr. Der Test ist nicht geändert (keine Test-Korrektur ohne Auftrag). Vorschlag: den Satz in der REQ streichen oder umformulieren («Beugung darf schon vor der ersten Auswertung steigen») und den Test dann anpassen.

## Akzeptanzkriterien

Bei «wie Runde 1»: Ergebnis unverändert (gleiche Zahlen).

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1, AK-2, AK-5, AK-7, AK-16 | offen (Board) | wie Runde 1 |
| AK-3 | erfüllt (PC) | wie Runde 1 |
| AK-4 | Zahl erfüllt (PC), Textzeile Board | wie Runde 1 |
| AK-6 | Warten erfüllt (PC), Rest Board | wie Runde 1 |
| AK-8 | **teilweise** | **Neu erfüllt:** Beugung steht spätestens 2000 ms nach der Bewegung auf 50 bzw. 20 (± 3°) (2 Mal 50°, 4 Mal 20°), und «nur Oberarm bewegt: Beugung und Hand bleiben 0». Hand, Oberarm, «nie kleiner»: wie Runde 1 erfüllt. **Nicht erfüllt:** Beugung nicht zu gross bei gleichzeitigem Oberarm-Kreis (Befund 1); Satz «vor der ersten Auswertung 0» (Befund 2). |
| AK-9 | offen (Board) | wie Runde 1 |
| AK-10 | **nicht erfüllt** (wegen Befund 1) | Erfolg und Winkelfehler ≤ 2,5° erfüllt (2,27°, unverändert); «Erfolg genau dann, wenn alle vier Werte OK» in allen 18 Läufen jede Sekunde bestanden. Aber: Erfolg trotz echter Beugung 20° (Befund 1). |
| AK-11 | erfüllt (PC) | 18 von 18, längster Schritt 2 18,0 s, Fehler 2,27° (Reserve weiter 0,23°). |
| AK-12 | erfüllt (PC) | wie Runde 1 (0 von 18 Erfolge bei gleichzeitigen Bewegungen, nie falscher Winkel). Hinweis: Befund 1 ist ein gleichzeitiger Fall mit Erfolg und zu kleiner Beugung. |
| AK-13 | Grund-Logik erfüllt (PC), Zeit/Texte Board | wie Runde 1 |
| AK-14 | **nicht erfüllt** (wegen Befund 1) | Nie Erfolg ohne Oberarm-/Handbewegung: erfüllt. Schiefes Einmessen 38 von 38 erfüllt. Aber «nie raten»: Erfolg mit zu kleiner Beugung. |
| AK-15 | Logik erfüllt (PC), Texte/Motor Board | wie Runde 1 |
| AK-17 | Speicher erfüllt, Zykluszeit Board | Flash 32220 ≤ 32256 (99 %, **36 Byte Reserve**), RAM 64 % ≤ 70 %. Zykluszeit am Board messen (die Änderung betrifft die Rechenarbeit pro Zyklus). |
| AK-18 | erfüllt (PC) | Streuung 0,349° (unverändert). |
| AK-19 | erfüllt | wie Runde 1 |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-6 (falsche Einmessung) | **nicht geschützt in einem Fall** | Befund 1: Erfolg trotz Beugung nur 20° bei gleichzeitiger Oberarm-Bewegung. Sonst wie Runde 1 (nie Erfolg ohne Oberarm/Hand, nie falscher Winkel bei den 18 gleichzeitigen Läufen, Fuzz ohne falschen Erfolg). |
| B-7 | Durchsicht ok, Board offen | wie Runde 1 (`jointsense.ino` unverändert) |
| B-1 | geschützt | wie Runde 1 |

## Fehler in Worten für den Developer

- **Beugung zu gross gemeldet (AK-8, AK-10, AK-14, B-6; Ursache: Code, Vermutung vorläufige Beugung):** Beugt die Person 4 Mal nur um 20° hin und zurück (Spannweite 20°, Gesamtweg 160°), bewegt dabei den Oberarm im Kreis und dreht danach die Hand um 60°, dann meldet das Gerät «Referenz gesetzt». Die Beugung hätte unter 30° bleiben müssen, das Einmessen hätte mit Grund Beugung abbrechen müssen. Wird dieselbe Beugung allein ausgeführt, klappt der Abbruch richtig. Bitte dafür sorgen, dass die Beugung auch bei gleichzeitigem Oberarm-Kreis nicht über der echten Spannweite (+ 3°) liegt.
- Befund 2 erst weitergeben, wenn der Engineer entschieden hat (Anforderung klarstellen).

## Am echten Board prüfen

Wie Runde 1 (`docs/tests/board-test-REQ-005.md`, inklusive neuer Zeile «Beugungs-Zahl steigt beim ersten Beugen sofort (≤ 2 s)»). Zusätzlich: Zykluszeit in Schritt 2 mit der neuen Beugung messen (≤ 100 ms, einmal pro Sekunde), und prüfen, ob die Beugungs-Zahl beim gleichzeitigen Beugen und Oberarm-Kreisen zu hoch läuft.

## Hinweise für den Engineer

1. **Befund 1 hat Vorrang (Sicherheit B-6).** In Runde 1 war er unsichtbar, weil die Beugung bei diesem Skript nie auftauchte. Test-Ursache ist unwahrscheinlich: Die gleiche Beugung allein bricht richtig ab, und der Fall ist in der REQ erlaubt-bewegt (Beugen/Oberarm/Hand).
2. **Befund 2:** Satz in der REQ-Schnittstelle («Vor der ersten Auswertung sind diese beiden Werte 0») streichen oder umformulieren, dann passe ich den Test an (Test-Korrektur 2 von 3 würde verbraucht, wenn du es als solche zählst).
3. **Flash: 36 Byte frei.** Jede weitere Korrektur am Code kann den Uno sprengen; der Developer sollte beim Beheben von Befund 1 auf Flash achten (Runde 3 wäre die letzte).
4. Winkelfehler/Reserve nicht schlechter geworden (siehe oben). Reserve bei AK-11 weiter nur 0,23°.
5. Gleichzeitige Bewegungen führen weiter nie zum Erfolg (0 von 18), erlaubt nach AK-12. Befund 1 betrifft den alten, festen Skript-Fall (Beugen + Oberarm-Kreis gleichzeitig, danach Hand).

## Fehlende Werkzeuge

Keine (clang++ statt g++ auf diesem PC). Wokwi absichtlich nicht benutzt.
