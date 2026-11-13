# Testbericht REQ-004, Runde 1

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-004](../anforderungen/REQ-004.md) |
| **Runde** | 1 von 3 |
| **Datum** | 2026-11-13 |
| **Code-Stand** | nicht committet |
| **Gesamtergebnis** | ☑ nicht bestanden (AK-13: 3 von 7 Fällen knapp über 2,5°; Wokwi nicht geprüft) |

## Anpassungen an meinen Tests (auf Wunsch des Engineers, Test-Korrektur 1 von 3)

1. `Grund: ruhig` klein: Die Logik-Tests und die Wokwi-Pläne prüfen den Grund nur über die Konstante, dort musste nichts geändert werden. Im Board-Test (`docs/tests/board-test-REQ-004.md`, B-10) stand «Grund: Ruhig» und ist jetzt «Grund: ruhig».
2. AK-12 (eine fehlgeschlagene Messung reicht): In der Logik gibt es dazu nur den Abbruch von aussen (`einmessenAbbrechen`) im Warte-Zustand, das passt weiter. Das «eine Messung genügt» steckt im `.ino` und ist nur über Wokwi (R4-W4) oder am Board prüfbar. Beides heute **nicht geprüft**.
3. Handdrehung bei der Bestätigung: **Mein Simulator hielt sie nicht gleich wie in Phase 1.** In zwei Fällen (Lage seitlich, Lage schräg) war die Hand bei der ersten Bestätigung um -20° bzw. 30° gedreht. Das ist jetzt auf 0° (wie Phase 1) korrigiert (`test_REQ-004_warten.cpp`). Die Hand bei der zweiten Bestätigung (Start Phase 3) ist weiter beliebig, denn Phase 3 misst die Drehung selbst. Ergebnis vor und nach der Korrektur: dieselben drei Fälle schlagen fehl (siehe AK-13), der Fall «schräg» wurde dabei sogar etwas schlechter (2,538° → 2,597°). Die Korrektur ist also nicht die Ursache des Fehlers.
   Die «Alltagsbewegung nach dem Einmessen» (Grenzfall Drift um die Senkrechte) war schon im Test: 3 s halten, dann 30 s Alltag (beugen, Hand und Oberarm drehen, schwenken).

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno), alle Warnungen als Fehler | ☑ ok | Anzahl Warnungen: 0 |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ☑ ok | RAM 1180 Bytes (57 %), 868 Bytes frei. **Flash: 30082 von 32256 Bytes (93 %): knapp**, nur noch 2174 Bytes Reserve (Risiko für spätere Anforderungen) |
| 2 | Code-Scanner `cppcheck` | ☑ ok | Kein «error»/«warning». Ein Stil-Hinweis (`constVariable`: `pf` in `logik.cpp` Zeile 1211 könnte `const` sein), kein Fehler |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, Compiler `clang++`) | ☑ Fehler | 9 Testdateien, 8 bestanden, 1 fehlgeschlagen (`test_REQ-004_warten`: 32 von 35). REQ-001, -002 (alle Teile) und -003 laufen unverändert grün, auch mit dem umgebauten `einmessenAbschliessen`. `test_REQ-004`: 124 von 124 |
| 3b | Fuzz-Tests (NaN, unendlich, Extremwerte, wilde Aufrufreihenfolge) | ☑ ok | In `test_REQ-004` und den älteren Tests enthalten, alle bestanden |
| 4 | Wokwi-Simulation inkl. Störfälle | ☐ nicht geprüft | Wokwi-Kontingent aufgebraucht (Vorgabe des Engineers, Team-Entscheid). Nichts ausgeführt. Siehe `docs/wokwi/REQ-004-stoerfaelle.md` |
| 5 | Bibliotheken | ☑ ok | Nur `Wire` (Core) und `math.h`, keine neue Bibliothek. `docs/tech-stack.md` aktuell (Flash, RAM, Warte-Zustände, Serielle Eingabe eingetragen). IMU-Bibliothek bleibt `TODO` (offener Team-Punkt) |
| 6 | Sicherheitsregeln im Code | ☑ ok, eine bekannte Lücke | Kein `String`, kein `new`/`malloc`. Zeichen werden einzeln gelesen (höchstens 64 pro `loop()`), nie gewartet, kein eigener Puffer. Pins nur über `pins.h`. NaN-/Inf-Prüfungen vorhanden. **`Wire.setWireTimeout` fehlt weiterhin** (bekannt seit REQ-001, gehört zu B-3, nicht Teil von REQ-004) |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 | ☑ Logik erfüllt, Teil `.ino` nicht geprüft | `istBestaetigungsZeichen`: nur Leerzeichen, `\n`, `\r` sind `true`, alle anderen der 256 Zeichen `false`. Der Taster-Teil und das Lesen im `.ino` sind nicht automatisch prüfbar (Wokwi nicht verfügbar): Board B-1 |
| AK-2 | ☐ nicht geprüft | Steckt im `.ino`. Code-Durchsicht: `bestaetigungGekommen` liest alle Zeichen im Puffer und gibt höchstens eine Bestätigung zurück. Beleg fehlt: Board B-1b/B-2 |
| AK-3 | ☐ nicht geprüft | Nur `.ino`/Wokwi (R4-W2)/Board |
| AK-4 | ☐ nicht geprüft | Nur `.ino`/Wokwi/Board. Logik-Teil «nach Abbruch sofort wieder startbereit» ist bestanden (siehe AK-11) |
| AK-5 | ☑ Logik erfüllt, Zeiten nicht geprüft | `einmessenStillAuswerten`: ruhig → `true`; nicht ruhig und ohne Daten → `false` mit Grund ruhig. Die 2000 ms ± 100 ms stehen im `.ino`: Wokwi/Board |
| AK-6 | ☑ Logik erfüllt, Zeiten/Stoss nicht geprüft | `einmessenBewegungAuswerten`: Beugung und Oberarm richtig (Beugung zuerst). Start ≤ 100 ms, 1 Stoss: Wokwi/Board |
| AK-7 | ☑ Logik erfüllt, Zeiten/Stösse nicht geprüft | Hand-Prüfung und Abschluss nach den neuen Auswerte-Funktionen richtig. Ohne sie verhält sich `einmessenAbschliessen` wie in REQ-002. Zwei Stösse: Wokwi/Board |
| AK-8 | ☐ nicht geprüft | Nur `.ino`/Wokwi (R4-W1)/Board (B-3, B-4: 300-ms-Stoss unterscheidbar?) |
| AK-9 | ☑ Logik erfüllt, 60-s-Ablauf nicht geprüft | `wartezeitAbgelaufen`: Grenze 60 000 ms exakt, auch beim Überlauf von `millis()`. Der Abbruch nach 60 s im Ablauf: Board B-8 (Wokwi nicht in 10 s prüfbar) |
| AK-10 | ☐ nicht geprüft | Nur `.ino`/Wokwi (R4-W5)/Board |
| AK-11 | ☑ erfüllt (Logik) | Nach jedem Abbruch (Still, Beugung, Oberarm, Hand, von aussen in Warte-Zustand 1 und 2) bleibt die alte Referenz samt Achse, bei «keine Referenz» bleibt es dabei, und ein neuer Start klappt sofort. Meldungstext `Grund: ruhig` steht im `.ino` (Code-Durchsicht ok, Zeile 521) |
| AK-12 | ☑ Logik-Teil erfüllt, Rest nicht geprüft | Abbruch im Warte-Zustand lässt die Referenz stehen. «Eine fehlgeschlagene Messung genügt» im `.ino`: Wokwi R4-W4 / Board B-11 |
| AK-13 | ☒ **nicht erfüllt** | 4 von 7 Fällen bestanden, 3 knapp darüber. Alle Prüfungen (Still, Bewegung, Abschluss) liefern `true`, die Lage ist immer bekannt. Der Winkelfehler in der Alltagsbewegung danach: hängend/hängend, Drift +: **2,63°**; schräg/schräg, Drift +: **2,60°**; Arm hängt still, Drift +: **2,58°** (Grenze 2,5°). Bestanden: vorn/hängend 2,15°, oben/seitlich 2,40°, seitlich/vorn 1,87°, kurze Wartezeit ohne Drift 2,11°. Siehe «Fehler» |
| AK-14 | ☐ nicht geprüft | Zeiten im `.ino`: Wokwi/Board |
| AK-15 | ☐ nicht geprüft (teilweise) | Code-Durchsicht: Lesen blockiert nie, kein `String`. RAM 57 % (Grenze 70 %). Zykluszeit 50 Hz nicht gemessen: Wokwi/Board |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 (kaputte Eingaben über Serial) | ☑ geschützt (Logik), `.ino` nur Durchsicht | Alle 256 Zeichen und NaN/Inf-Fuzz bestanden. Code liest einzeln, höchstens 64 pro Durchlauf, ohne `String`/Puffer. Am Board nicht geprüft |
| B-6 (falsche Einmessung) | ☑ teilweise geschützt | Jede Phase wird geprüft, alte Referenz bleibt bei jedem Abbruch (Logik). Verfrühte Bestätigung (AK-3) und Wartezeit-Ablauf im Ablauf nicht geprüft. Durch AK-13 knapp **nicht** sicher: nach langem Warten ist der Winkelfehler an der Grenze |
| B-7 (Einmessen über USB auslösen/blockieren) | ☐ nicht geprüft | Zeichenfilter in der Logik ok. Der Rest (Ignorieren während Messung, 60-s-Grenze, keine Warnung währenddessen) steckt im `.ino`: Wokwi/Board |

## Fehler in Worten für den Developer

- **AK-13 (Genauigkeit nach langem Warten), knapp nicht erfüllt.** Mit je 59 s Wartezeit in beiden Warte-Zuständen und einem Gyro-Fehler, der in dieser Zeit langsam um bis zu 0,05 °/s wandert (Schlimmstfall laut Anforderung, beide Sensoren so, dass sich die Fehler nicht aufheben), ist der Winkel in der Alltagsbewegung nach dem Einmessen um bis zu 2,63° falsch statt höchstens 2,5°. Betroffen ist der Fall mit Fehler-Vorzeichen «+»: Arm hängt (frei oder still) und Arm schräg. Dieselbe Alltagsbewegung mit kurzer Wartezeit und ohne Fehler-Wanderung ergibt etwa 2,1°. Die Wartezeit kostet also etwa 0,4 bis 0,5° Genauigkeit. Alle drei Prüfungen (Still, Bewegung, Abschluss) und die Lage-Erkennung liefern richtig. Es verbessert sich nichts, wenn der Arm beim Warten still hängt (2,58°). Das passt zum Grenzfall «Rest-Verdrehung um die Senkrechte» in der Anforderung (bis ca. 4,6°, durch Bewegung danach korrigiert), die Korrektur kommt hier in der Alltagsbewegung offenbar noch zu spät oder zu schwach.
  - **Vermutete Ursache: Code**, mit Vorbehalt «unklar». Die Anforderung nennt AK-13 mit genau diesen Annahmen als Ziel, also ist es dort erfüllbar. Möglich wäre auch, dass die Schlimmstfall-Rampe des Testers (Fehler bleibt nach den Wartezeiten bei +0,05 °/s stehen) strenger ist als gemeint. Ist die Rest-Verdrehung nach dem Warten nicht anders als im Normalbetrieb zu beheben, soll der Engineer mit dem Team klären, ob 2,5° bei diesem Schlimmstfall überhaupt erreichbar sind.

## Am echten Board prüfen

Weder Logik-Tests noch Wokwi (nicht verfügbar) belegen das. Die Liste steht in `docs/tests/board-test-REQ-004.md`. Besonders wichtig:

- Enter, Leerzeichen und Taste im echten Seriellen Monitor (alle vier Zeilenende-Einstellungen), «Beides NL & CR» zählt nur einmal (AK-1, AK-2).
- Ganzer Ablauf: Zeiten der drei Phasen, 300-ms-Warte-Stoss ist von den anderen Stössen unterscheidbar, Textzeilen genau einmal (AK-5 bis AK-8, AK-14).
- Bestätigung während einer Phase wird ignoriert (AK-3), 60-s-Abbruch (AK-9), keine Überstreckungs-Warnung während dem Einmessen (AK-10).
- IMU abziehen in einer Phase und in einem Warte-Zustand: `Grund: IMU` schon nach einer fehlgeschlagenen Messung (AK-12).
- Meldung heisst jetzt `Grund: ruhig` (klein).
- Zykluszeit 50 Hz bleibt (AK-15).

## Hinweise für den Engineer

- **Wokwi: nicht geprüft** (Kontingent aufgebraucht). Alle Kriterien, die nur das `.ino` betreffen (AK-2, 3, 4, 8, 10, 14, 15, Zeiten aus AK-5 bis AK-7, 60-s-Abbruch), haben deshalb **keinen automatischen Beleg**, nur den Board-Test. Die alten Wokwi-Szenarien aus REQ-001/002 gehen vom alten Ablauf aus und sind laut `docs/wokwi/REQ-004-stoerfaelle.md` veraltet.
- **Flash 93 % (30082 von 32256 Bytes)**: Nur noch 2174 Bytes Reserve. Neue Anforderungen (z. B. echte IMU-Bibliothek, Status-LED) könnten nicht mehr passen. Für `docs/offene-hinweise.md`.
- **`Wire.setWireTimeout` fehlt** weiterhin (B-3, bekannt seit REQ-001). Für `docs/offene-hinweise.md`, falls noch nicht drin.
- **Unklare Annahme (Tester):** Bei der zweiten Bestätigung (Start Phase 3) ist die Hand-Stellung in der REQ nicht festgelegt. Der Test lässt sie beliebig. Bitte bestätigen oder in der REQ festlegen.
- **AK-13-Schwelle:** Je nach Entscheid des Teams (Code verbessern oder Schlimmstfall-Annahme lockern) bitte klären. Test-Korrekturen verbraucht: 1 von 3 (Handdrehung bei der Bestätigung).
- Compiler auf diesem PC: `clang++` (kein `g++`), das ist laut Vorgabe ok. Der `cppcheck`-Stil-Hinweis zu `pf` ist harmlos.

## Fehlende Werkzeuge

Keine. (`g++` fehlt, `clang++` ersetzt es. `wokwi-cli` ist da, wurde aber wegen des aufgebrauchten Kontingents nicht benutzt.)
