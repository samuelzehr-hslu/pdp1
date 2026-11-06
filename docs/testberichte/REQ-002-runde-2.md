# Testbericht REQ-002, Runde 2
Datum: 2026-11-06   Ergebnis: **NICHT BESTANDEN**

Kurz: Die Messung stimmt bei Drehung des Oberarms, beim Schwenken, in jeder Lage, bei verdrehten IMUs (knapp) und in der 10-Minuten-Drift (knapp unter 2°).
Drei Dinge sind **nicht in Ordnung**:
1. **Unterarm drehen bei vorgehaltenem, gebeugtem Arm (AK-1):** Der Winkel läuft um 50 bis 146° weg. Bei hängendem Arm stimmt es.
2. **Einmessen mit reinem Pendeln des Oberarms (AK-6/AK-7):** bricht fast immer mit `Grund: Oberarm` ab (Anforderung verlangt: muss klappen). Ausserdem bricht es bei nur **einmal** Beugen (40°) mit Kreisen ab, und bei einem zweiten Einmessen mit alter Referenz ist der Winkel danach bis 11,8° falsch.
3. **Wokwi konnte nicht laufen:** Das Wokwi-Konto hat sein monatliches Kontingent an CI-Minuten aufgebraucht (`API Error: You have used up your Free plan monthly CI minute quota`). Alle Wokwi-Prüfungen sind **«nicht geprüft»**, nicht «bestanden».

## Prüfkette

| Schritt | Ergebnis | Bemerkung |
|---|---|---|
| Kompilieren + RAM/Flash | bestanden, **Flash-Risiko** | `arduino-cli compile --fqbn arduino:avr:uno --warnings all`: keine Warnung. **Flash 31922 von 32256 Byte = 98 %** (nur 334 Byte frei). RAM 1182 Byte globale Variablen = **57 %** (Grenze AK-15: 1434 Byte = 70 %), 866 Byte frei. |
| cppcheck | bestanden | `warning,style,performance,portability` auf `jointsense.ino` und `logik.cpp`: keine Funde. |
| clang++ Logik- und Fuzz-Tests | **nicht bestanden** | Compiler `clang++` (kein `g++`). `test_REQ-001`: 147 von 147 ok (Regression sauber). `test_REQ-002`: 217 von 223 ok (6 Fehler). `test_REQ-002_drift`: 28 von 28 ok. `test_REQ-002_oberarm`: 13 von 21 ok (8 Fehler). Keine Compile-Fehler in den Testdateien. |
| Python-Szenarien | bestanden | `ausfall_verlaeufe.py`, `winkelverlauf.py` laufen, REQ-001-Tests damit ok. |
| Wokwi W1 bis W6 (`pruefe_REQ-002.py`) | **nicht geprüft** | Wokwi meldet: Kontingent aufgebraucht (Free-Plan, CI-Minuten pro Monat). Die 24 «FEHLER» im Lauf kommen davon, nicht von der Firmware (kein einziger Lauf hat die Firmware gestartet). Kein Umweg versucht. |
| Wokwi REQ-001-Szenarien (`pruefe_REQ-001.py`) | **nicht geprüft** | Gleicher Grund (26 «FEHLER» durch Netzwerk-/Konto-Fehler `TLSSocket`, `code 1006`, Kontingent). Keine Aussage über die angepassten Szenarien möglich. Hinweis: Ich habe beide Skripte gleichzeitig gestartet. Ob das Kontingent erst dadurch leer war oder schon vorher, kann ich nicht sagen. |
| Bibliotheken | bestanden | Nur `Wire` (Core `arduino:avr` 1.8.8) sowie `math.h`, `stdint.h`, `stddef.h`. Stimmt mit `docs/tech-stack.md` überein. Keine IMU-Bibliothek (im tech-stack weiter `TODO`). |
| Sicherheitsregeln im Code | teilweise | Kein `String`, kein `new`/`malloc`. Pins nur über `pins.h`-Namen (`PIN_MOTOR_PWM`, `PIN_MOTOR_IN2`, `PIN_TASTER`). `isnan`/`isinf` in `logik.cpp` mehrfach. `Wire.setClock(400000)` und Gyro-Bereich ±500 °/s (Register `0x1B`) gesetzt. **`Wire.setWireTimeout` fehlt** (B-3, laut REQ-002 «Nicht dazugehörig», bleibt offen). |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Was passiert ist (in Worten) |
|---|---|---|
| AK-1 Unterarm drehen | **nicht bestanden** | Hängender Arm (Neigung 90°): bei 0°, −30°, −90° Änderung höchstens 0,17°, ok. **Arm nach vorn (Neigung 0°):** bei −30° ändert sich der Wert um **107,7°** (Alarm-Fehler). Zusatzmessung (Ellbogen −10, −20, −30, −40, −60° mit Neigung 0° und 20°): Fehler **50 bis 146°**, immer falsch. Nur bei 0° und −90° (Neigung 0) ist die Änderung ≤ 0,2°, **aber** dort ist die Lage in 112 von 400 bzw. 25 von 400 Zyklen «unbekannt» (kein Wert, also auch keine Warnung). |
| AK-2 Oberarm drehen | bestanden | Bei 0° und −90°, Neigung 90° und 0°: grösste Änderung 0,19°. |
| AK-3 Arm schwenkt | bestanden | Bei 0°, −30°, −90°, Neigung 0° und 45°: grösste Änderung 0,18°. |
| AK-4 Lage und Einbaudrehung | bestanden (knapp) | Lagen (hängend, vorn, oben, seitlich): Unterschied 0,16°. IMU im Sleeve verdreht (25 Kombinationen, 3 Achsen): grösster Fehler **1,998° / 1,948° / 1,895°** (Grenze 2°), 0 von 25 über 2°. Fast keine Reserve. |
| AK-5 Messbereich, Auflösung, Erkennung | bestanden | −100 bis +15° auf 2° genau, 1° echt = 0,5 bis 1,5° Wert. Erkennungsrate bei +7°: 100 %, bei +3° (kein Alarm): 100 %. |
| AK-6 Einmessen Ablauf | **nicht bestanden** (Logik), Ablauf **nicht geprüft** | Logik: Einmessen mit 2 s still, 4 s Beugen und Oberarm-**Kreisen** klappt (ausser unten). **Fehler:** (1) Oberarm **pendelt** (eine Achse, 15° oder 30°): bricht ab, siehe «Anteil Oberarm-Abbruch». (2) Kreise 15°/20° mit Einbau 1: Winkel danach bis 2,06 / 2,09° falsch (Grenze 2°). (3) Zweites Einmessen mit alter Referenz (bei −20° eingemessen): beim anschliessenden Beugen bis −90° mit allen Bewegungen gleichzeitig ist der Winkel in **allen 5 Einbauten 11,3 bis 11,8° falsch**. Wokwi (Stoss bei t0+2050, 100 ms, Bestätigung 150 ms, Gesamtdauer 6000 ± 100 ms): **nicht geprüft**. |
| AK-7 Abbruch | **nicht bestanden** | (a) Spannweite 20° (hin und zurück, Weg 40/160°): Abbruch richtig. (b) Unterarm dreht in Phase 1: Abbruch richtig, Messung läuft danach mit alter Referenz richtig weiter (Fehler 0,03 bis 0,52°, **in Runde 1 war das falsch, jetzt behoben**). (d) Oberarm hängt still, nur 3° oder nur um die Senkrechte: Abbruch richtig, nach Abbruch Lage bekannt, nie `true`. **Fehler:** Wenn sich der Oberarm bewegt (Kreise 20°) und der Unterarm **nur einmal** 40° beugt/streckt (Spannweite über 30°), bricht das Einmessen mit `Grund: Oberarm` ab. Die Anforderung verlangt: klappt. Mit 2 Mal Beugen und gleicher Bewegung klappt es (35 bis 70°). Ebenso bricht reines Pendeln ab (AK-6). Meldungstexte `Einmessen abgebrochen - Grund: ...`, Reihenfolge der Gründe und (c) IMU-Ausfall im Ablauf: **nicht geprüft** (Wokwi). |
| AK-8 Wiederholgenauigkeit | bestanden | 10 Einmessungen: Streuung 0,22° (erlaubt 3°). |
| AK-9 Einschalten | Logik bestanden, Ablauf **nicht geprüft** | Logik: ohne Einmessen nie eine Abweichung, auch bei überstrecktem Arm. Zeile `Keine Referenz` alle 1000 ms ± 100 ms und kein Stoss nach dem Start: Wokwi **nicht geprüft**. |
| AK-10 Drift mit Ruhe | bestanden (knapp) | 10 min, beide Fehlermuster (Gyro-Offset ± 2 °/s, Rauschen): grösster Fehler **1,956° und 1,909°** (Grenze 2°). Nur zwei Fehlermuster getestet, Reserve unter 0,1°. |
| AK-11 Drift ohne Ruhe | bestanden (knapp) | 10 min Dauerbewegung: grösster Fehler **1,983° und 1,832°** (Grenze 2°). Reserve nur 0,02° im schlechtesten Fall. Mit anderen Fehlerwerten kann es kippen. |
| AK-12 Referenz bleibt sicher | bestanden | +8° 10 min halten (Arm ruhig und bewegt): Wert 7,76 bis 8,37°, Warnung die ganze Zeit an. Ruhe bei 0° und −45°: Drift unter 0,4°. Referenz wird nie automatisch verändert. |
| AK-13 Nach einem Ausfall | bestanden (Alarm), Hinweis; Wokwi **nicht geprüft** | Direkt nach `ellbogenLageVerwerfen` keine Abweichung, Lage nach 0,48 s wieder bekannt (≤ 5 s), danach bleibt sie bekannt, Winkel danach ≤ 1,7° falsch. Bewegt sich der Arm während des Ausfalls weiter: in 19 Zyklen direkt nach der Wiederkehr ist der Wert bis **2,2°** falsch (Teilprüfung schlägt an, weil sie «immer ≤ 2°» verlangt). Es gibt **keinen** Wert über 5°, also keinen Fehlalarm. Laut Wortlaut («keine Fehlalarme») bestanden. |
| AK-14 Ungültige Werte | Logik bestanden, Übersteuerung **nicht geprüft** | NaN, unendlich, > 16 g, > 2000 °/s, `dt` ≤ 0 oder > 0,5: immer `false`, Zustand unverändert (je 105 Versuche in 3 Zuständen). Fuzz: kein Absturz, nie NaN. Übersteuerung (Rohwert +32767 / −32768 überspringt den Zyklus, kein IMU-Ausfall): im Code vorhanden (`jointsense.ino` Zeile 352, alle drei Achsen), **in Wokwi nicht geprüft**. |
| AK-15 Tempo und Speicher | RAM bestanden, Zeit **nicht geprüft** | RAM 57 % (≤ 70 %), keine dynamische Speicherverwaltung. Zeile `Zyklus max: <n> us` im Code vorhanden, Messwert aber **nicht** ermittelt (Wokwi nicht verfügbar). |

## Verlangte Beobachtungen

| Beobachtung | Ergebnis |
|---|---|
| **Flash-Belegung** | **98 %** (31922 von 32256 Byte, 334 Byte frei). Bestätigt die Meldung des Developers. Jede kleine Erweiterung (z. B. `Wire.setWireTimeout`, Fehlerbehebung für AK-1) kann den Speicher sprengen. Hohes Risiko. |
| **Dauer `einmessenAbschliessen`** und **Zykluszeit** (`Zyklus max:`) | **Nicht prüfbar:** Wokwi gesperrt (Kontingent), kein Board (Hochladen verboten). Nur Anhaltspunkt vom PC: `einmessenAbschliessen` kostet etwa das 5- bis 70-fache eines normalen Messzyklus (rund 25 µs gegen 0,4 bis 4,5 µs auf dem PC, schwankt stark). Auf dem Uno (ohne Gleitkomma-Chip) kann das die vom Developer geschätzten 0,2 bis 0,4 s ergeben. Das wäre einmalig am Ende des Einmessens ein Loch von rund 10 bis 20 Messzyklen. Das ist über der Grenze von 15 ms je Zyklus (AK-15 meint den normalen Zyklus). Auf dem Board nachmessen. |
| **Anteil Abbrüche `Grund: Oberarm` bei reinem Pendeln um eine Achse** | Testlauf (`test_REQ-002_oberarm`): bei Pendeln 15° **9 von 10** Einmessungen abgebrochen (4 von 5 ohne, 5 von 5 mit alter Referenz), bei Pendeln 30° ebenfalls **9 von 10**. Zusatzmessung (5 Einbauten × 3 Oberarm-Drehungen 0°/45°/90° = 15 Fälle): **15 von 15** abgebrochen bei 15° und 15 von 15 bei 30°, alle mit `Grund: Oberarm`. Zum Vergleich: kleine Kreise (15° und 30°): 0 von 15 abgebrochen. Der Abbruch ist fail-safe (das Gerät rät nicht), widerspricht aber dem Beispiel «z. B. kleine Kreise oder Pendeln» in AK-6. |
| `rotationZ` in Wokwi | **nicht geprüft** (Kontingent). Die Szenarien W5 und die Vorspanne setzen `rotationX/Y/Z` ein. |
| −1000 °/s ergibt genau −32768 | **nicht geprüft** (Kontingent). Szenario W1 setzt `rotationX` auf −1000 und +1000, ist aber nicht gelaufen. |

## Korrektur zu Runde 1

In Runde 1 stand: «Wokwi hat kein Gyroskop, Werte immer 0». Das war **falsch**. Wokwi hat ein Gyroskop. Die Regler heissen `rotationX`, `rotationY`, `rotationZ` (°/s). Die Runde-1-Probe hat `gyroX` benutzt. Diesen Regler gibt es nicht, Wokwi meldet dafür keinen Fehler, die Werte blieben darum 0. Daraus kam der falsche Schluss. Die Aussagen «W3, W5, Übersteuerung in Wokwi nicht prüfbar» aus Runde 1 galten wegen dieses Fehlers nicht. In Runde 2 sind sie wegen des aufgebrauchten Wokwi-Kontingents nicht prüfbar (anderer Grund).

## Fehlerbeschreibung für den Developer (in Worten, ohne Testcode)

1. **Unterarm drehen bei vorgehaltenem Arm (AK-1):** Ist der Arm nach vorn gehalten (oder leicht nach unten geneigt, bis 20°) und der Ellbogen gebeugt (−10 bis −60°), dann ändert sich der gemessene Winkel stark, wenn man die Hand dreht (50 bis 146°). Bei hängendem Arm stimmt es, auch bei gestrecktem (0°) und stark gebeugtem Ellbogen (−90°). Bei diesen zwei Winkeln fehlt beim vorgehaltenen Arm zeitweise die Lage (Wert unbekannt, keine Warnung möglich).
2. **Einmessen bricht zu oft ab (AK-6/AK-7):** (a) Pendelt der Oberarm nur in einer Richtung (15° oder 30°), bricht das Gerät fast immer mit «Oberarm» ab. Die Anforderung nennt Pendeln ausdrücklich als gültige Bewegung. (b) Beugt man den Arm nur **einmal** (40°) und bewegt dabei den Oberarm in kleinen Kreisen, bricht es ebenfalls mit «Oberarm» ab. Mit zweimal Beugen klappt es.
3. **Zweites Einmessen (mit alter Referenz) liefert danach einen falschen Winkel:** Wird ein zweites Mal eingemessen (hier bei −20° Ellbogen), ist der Winkel beim späteren Beugen mit gleichzeitigem Drehen, Schwenken und Neigen etwa 11,5° daneben, bei allen fünf getesteten Einbaulagen. Das erste Einmessen und das Einmessen ohne alte Referenz sind in Ordnung. (Mögliche Ursache: Die zweite Einmessung beginnt nicht bei 0°, die Referenz wird dann nicht richtig auf den echten 0°-Winkel bezogen. Das ist nur eine Vermutung.)
4. **Kleine Reserve:** Einbau mit verdrehten IMUs (AK-4) und die beiden Drift-Läufe (AK-10, AK-11) liegen bei 1,83 bis 1,998°. Schon ein anderer Zufallswert kann über 2° gehen. Einbau 1 mit Kreisen (15°/20°) liegt mit 2,06 / 2,09° schon darüber.
5. **Flash 98 %:** Es bleiben 334 Byte. Fehlerbehebungen müssen kleiner ausfallen oder Code muss sparen.

## Bedrohungen (aus docs/bedrohungen.md)

| Bedrohung | geschützt / nicht geschützt / akzeptiertes Risiko | Begründung |
|---|---|---|
| B-2 Falsche Sensordaten | geschützt (Logik), Übersteuerung **nicht geprüft** | Alle Messwerte und `dt` werden auf NaN, unendlich und unmögliche Grössen geprüft, abgelehnte Zyklen ändern den Zustand nicht, Fuzz ohne Absturz, kein NaN-Ergebnis. Gyro-Anschlag (±32768) wird in `jointsense.ino` übersprungen: nur durch Durchsicht bestätigt, Wokwi-Prüfung offen. |
| B-3 Lahmlegen | teilweise geschützt | RAM 57 %, keine dynamische Speicherverwaltung. **Flash 98 %.** Zykluszeit und Dauer von `einmessenAbschliessen` nicht gemessen. Hängender I²C-Bus: **nicht geschützt** (`Wire.setWireTimeout` fehlt, kein Watchdog). Laut REQ-002 nicht Teil dieser Anforderung, bleibt offen. |
| B-6 Falsche Einmessung / Selbstkorrektur | **nicht geschützt** (teilweise) | Geschützt: Abbruch bei zu kleiner Beugung, bei unruhiger Phase 1 und bei unbestimmter Oberarm-Achse (das Gerät rät nicht); Referenz wird nie automatisch verändert (+8° bleiben 10 min Warnung); nach Ausfall keine Warnung bis die Lage bekannt ist (0,48 s). **Nicht geschützt:** Bei vorgehaltenem, gebeugtem Arm und Drehen der Hand ist der Winkel völlig falsch (AK-1): Fehlalarme oder verpasste Warnungen. Nach einem zweiten Einmessen ist der Winkel bis ca. 11,5° falsch. |

## Nicht geprüft / nicht prüfbar

- **Alle Wokwi-Läufe** (W1 bis W6, REQ-001-Szenarien mit echtem Einmessen): Wokwi-Konto, monatliches Kontingent aufgebraucht. Betrifft: AK-6 Ablauf (Stoss, Bestätigung, 6000 ms), AK-7 Meldungstexte und Reihenfolge, AK-9 Zeile `Keine Referenz`, AK-13 im Gesamtgerät, AK-14 Übersteuerung, AK-15 `Zyklus max`, `rotationZ`, −1000 → −32768. Nächster Schritt: Kontingent erneuern (nächster Monat oder bezahlter Plan) oder am echten Board prüfen (ein Mensch lädt hoch).
- Zykluszeit und Dauer von `einmessenAbschliessen` auf dem Uno.
- Fehlermuster der Drift-Läufe: nur zwei (Gyro-Offset je Achse), andere könnten über 2° gehen.

## Fehlende Werkzeuge

`g++` fehlt, `clang++` wird genutzt. `arduino-cli`, `cppcheck`, Python, `wokwi-cli` vorhanden. `wokwi-cli` läuft, aber das Konto hat kein CI-Kontingent mehr.

## Dateien dieser Runde

- Logs und Zusatzmessungen: `scratch/r002c/` (nicht im Repo): `compile.txt`, `cppcheck.txt`, `logiktests.txt`, `wokwi002.txt`, `wokwi001.txt`, `w/w1.out` (Kontingent-Meldung) und die Zusatzprogramme `probe*.cpp` (Pendeln, AK-1-Winkel, AK-13, Zeitverhältnis). Sie ändern keinen Produkt-Code und keine Testdatei.
- Kein Produkt-Code und keine Testdatei geändert. Nichts aufs Board geladen, kein serieller Port, nichts committet.
