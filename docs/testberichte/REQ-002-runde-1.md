# Testbericht REQ-002, Runde 1
Datum: 2026-11-06   Ergebnis: **NICHT BESTANDEN**

Kurz: Einfache Drehungen (AK-1 bis AK-3), normale Messung, Einschalten, Wiederholgenauigkeit, ungültige Werte, Tempo und Speicher sind in Ordnung.
Sobald aber **mehrere Bewegungen gleichzeitig** passieren (Ellbogen beugt, dazu Unterarm dreht, oder der ganze Arm bewegt sich bei verdrehten IMUs, oder Sensor-Rauschen),
läuft der gemessene Winkel weg (bis über 300 Grad Fehler). Das betrifft AK-4 (Einbau), AK-7 (Messung nach Abbruch), AK-10, AK-11, AK-13.

## Prüfkette

| Schritt | Ergebnis | Bemerkung |
|---|---|---|
| Kompilieren + RAM | bestanden | `arduino-cli compile --warnings all` (Uno): keine Warnung. Flash 20962 Byte (64 %). RAM: 1092 Byte globale Variablen = **53 %** (Grenze AK-15: 1434 Byte = 70 %), 956 Byte frei. Kein Risiko. |
| cppcheck | bestanden | `warning,style,performance,portability` auf `jointsense.ino` und `logik.cpp`: keine Funde. |
| g++/clang++ Logik- und Fuzz-Tests | **nicht bestanden** | Compiler `clang++`. `test_REQ-001`: 147 von 147 ok. `test_REQ-002`: 211 von 223 ok (12 Fehler). `test_REQ-002_drift`: 23 von 28 ok (5 Fehler). Details unten. Keine Compile-Fehler in den Testdateien. |
| Python-Szenarien | bestanden | `ausfall_verlaeufe.py` und `winkelverlauf.py` laufen (REQ-001-Tests damit ok). Die REQ-002-Tests erzeugen ihre Bewegungen selbst im C++-Simulator `sim_ellbogen.h` (ruft die echte Logik auf, baut sie nicht nach). |
| Wokwi-Störfälle | teilweise bestanden, Rest **nicht prüfbar** | W1, W2, W4 bestanden (`pruefe_REQ-002.py`, 28 Prüfungen ok). **W3, W5 und Übersteuerung nicht prüfbar:** Der simulierte MPU6050 liefert kein Gyroskop (Rohwerte immer 0, mit einem Testprogramm nachgemessen, auch bei `gyroX = 1000`). Ohne Gyroskop findet die Firmware keine Scharnier-Achse, das Einmessen klappt in Wokwi nie (Abbruch «Beugung»). Empfehlung: am echten Board testen. |
| REQ-001-Szenarien angepasst | teilweise | Vorspann «Taste drücken» ersetzt durch Warten (gleiche Zeit). `pruefe_REQ-001.py`: 28 Prüfungen ok. **Nicht mehr prüfbar** (brauchen eine Referenz = erfolgreiches Einmessen): `ak04_ausfall_bei_ueberstreckung`, `ak08_wiederkehr_mitten_im_stoss`, `ak10_einmessen_abbruch`, `ak10_einmessen_bei_ausfall`, Schluss von `ak11_ohne_referenz`. Sie wurden im Skript auf «nicht pruefbar» gesetzt, nicht auf «bestanden». |
| Bibliotheken | bestanden | Nur `Wire` (im Core) sowie `math.h`, `stdint.h`, `stddef.h`. Stimmt mit `docs/tech-stack.md` überein. Keine IMU-Bibliothek. Core `arduino:avr` 1.8.8. |
| Sicherheitsregeln im Code | teilweise | Kein `String`, kein `new`/`malloc`. Pins nur über `pins.h`-Namen. `isnan`/`isinf` in `logik.cpp` vorhanden. **`Wire.setWireTimeout` fehlt**: laut REQ-002 «Nicht dazugehörig» (B-3, hängender Bus), darum kein Fehler dieser Runde, aber weiter offen. |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Was passiert ist (in Worten) |
|---|---|---|
| AK-1 Unterarm drehen | bestanden | Bei 0°, −30°, −90° Ellbogen und Drehung ±90° um die Unterarm-Achse: grösste Winkeländerung 0,23°, kein Alarm. Gilt nur für **gleichbleibenden** Ellbogen (siehe AK-4/AK-7/AK-10). |
| AK-2 Oberarm drehen | bestanden | Bei 0° und −90°: grösste Änderung 0,23°. |
| AK-3 Arm schwenken | bestanden | Bei 0°, −30°, −90°: grösste Änderung 0,23°. |
| AK-4 Lage und Einbaudrehung | **nicht bestanden** | Teil «Arm hängend, vorn, oben, seitlich»: bestanden (grösster Unterschied 0,11°). Teil «IMU im Sleeve verdreht (0 bis 360°)»: Beim reinen Beugen stimmt es für alle 25 Dreh-Kombinationen und alle 3 Achsen (grösster Fehler 1,7°, Messung mit eigenem Zusatzprogramm, nicht im Testlauf). Bewegt sich dabei aber auch der Arm (Neigen, Schwenken, Oberarm drehen) und sind die IMUs verschieden verdreht, weicht der Winkel in 20 von 25 Kombinationen weit ab (bis 124°). Auch bei normalem Einbau weicht der Winkel ab, sobald der Unterarm beim Beugen mitdreht (bis 46°). Der Testfall «Einbau» (alltägliche Bewegung nach dem Einmessen) schlägt darum für alle 3 Achsen fehl. |
| AK-5 Messbereich, Auflösung, Erkennung | bestanden | −100° bis +15° auf 2° genau. 1° echte Änderung = 0,5 bis 1,5° im Wert. Erkennungsrate bei +7°: 100 %, bei +3° (kein Alarm): 100 %. Hinweis: Beim reinen Beugen mit 0,5 Hz ist der Fehler bis 1,7° (Grenze 2°), wenig Reserve. |
| AK-6 Einmessen Ablauf | teilweise bestanden, Rest nicht geprüft | Logik: Einmessen mit 2 s still und 4 s Beugen liefert `true`, Referenz = Mittelwert, zweites Einmessen setzt neu. Wokwi (W2): Motor aus bis t0+1950 ms, **ein** Stoss bei t0+2050 ms, wieder aus bei t0+2200 ms, kein zweiter Stoss, Ende von Phase 2 (Abbruchzeile) nach 5900 ms noch nicht da, nach 6120 ms da. **Nicht geprüft:** die Bestätigungsvibration von 150 ms nach erfolgreichem Einmessen und die Gesamtdauer 6000 ± 100 ms bei Erfolg (in Wokwi kein erfolgreiches Einmessen möglich). |
| AK-7 Abbruch | **nicht bestanden** | (a) Beugung unter 30°: bestanden (Logik: `false`, alte Referenz bleibt; Wokwi: Zeile `Einmessen abgebrochen - Grund: Beugung`, keine Bestätigung). (c) IMU fällt aus: bestanden (Wokwi W4: `Grund: IMU`, `FEHLER ... IMU 2 (Unterarm)`, keine Bestätigung, `wieder da`). (b) «ruhig»: Logik liefert `false` und lässt die alte Referenz, **aber** wenn der Unterarm in Phase 1 gedreht wurde (40°/s) und danach gebeugt wird, stimmt die Messung mit der alten Referenz nicht mehr (Fehler bis 58°). Das verletzt «Die Lage bleibt nach einem Abbruch bekannt, die Messung läuft mit alter Referenz und Achse richtig weiter». In Wokwi nicht prüfbar (kein Gyroskop). |
| AK-8 Wiederholgenauigkeit | bestanden | 10 Einmessungen: Streuung 0,21° (erlaubt 3°). |
| AK-9 Einschalten | bestanden | Logik: ohne Einmessen nie eine Abweichung, auch bei überstrecktem Arm. Wokwi W1: kein Stoss nach dem Start, Motor die ganze Zeit aus (auch bei +30°), Zeile `Keine Referenz` etwa alle 1000 ms (Zählung bei 1, 2, 3 s: ± 130 ms ok). |
| AK-10 Drift mit Ruhe | **nicht bestanden** | 10 min Bewegung (Beugen 0 bis −100°, dazu Unterarm drehen, Oberarm drehen, Schwenken, Heben) mit Sensorfehlern: der Winkel läuft weg. Grösster Fehler 441° bzw. 337° (zwei Fehlermuster der Gyro-Offsets), also weit über 2°. |
| AK-11 Drift ohne Ruhe | **nicht bestanden** | Gleiche Bewegung ohne Pause: grösster Fehler 441° bzw. 337°. |
| AK-12 Referenz bleibt sicher | bestanden (wörtlich), Zusatz nicht bestanden | +8° 10 min halten: Wert bleibt 7,8 bis 8,2°, Warnung bleibt die ganze Zeit an. Ruhe bei 0° und −45°: Drift unter 0,3°. Referenz wird nie automatisch verändert. **Zusatzfall (nicht wörtlich im AK):** +8° halten, aber der ganze Arm bewegt sich: der Wert läuft weg (Fehler bis 352°, kleinster Wert 28°). Die Warnung bleibt zufällig an, der Wert ist aber falsch. |
| AK-13 Nach einem Ausfall | **nicht bestanden** | Richtig: direkt nach `ellbogenLageVerwerfen` keine Abweichung, die Lage ist nach 4,5 s wieder bekannt (≤ 5 s), einmal bekannt bleibt sie bekannt. Falsch: Der Winkel stimmt in den Tests schon **vor** dem Ausfall nicht (Arm bewegt sich gleichzeitig in mehreren Richtungen, mit Sensor-Rauschen: Fehler bis 10°), und nach der Wiederkehr liegt er bis 82° daneben. Das ist dieselbe Ursache wie bei AK-10, nicht der Ausfall selbst. Wokwi: nicht prüfbar. |
| AK-14 Ungültige Werte | bestanden (Logik), Übersteuerung nicht geprüft | NaN, unendlich, über 16 g, über 2000°/s (z. B. 9999) in jedem der 12 Messwerte, `dt` ≤ 0 oder > 0,5: immer `false`, Zustand unverändert (je 105 Versuche in 3 Zuständen). 15,9 g, 1999°/s, `dt` 0,001 und 0,5 sind gültig. Fuzz: 3 × 20000 + 30000 wilde Aufrufe, kein Absturz, nie NaN. **Nicht geprüft:** Übersteuerung (Rohwert ±32768 überspringt den Zyklus ohne IMU-Ausfall): in Wokwi nicht erzeugbar. Durch Code-Durchsicht umgesetzt (`jointsense.ino`, `imuLesen` und `loop`), aber nicht getestet. |
| AK-15 Tempo und Speicher | bestanden (mit Hinweis) | RAM 1092 Byte = 53 % (≤ 70 %). Keine dynamische Speicherverwaltung. Zeile `Zyklus max: <n> us` etwa alle 1000 ms. Wokwi: grösster Wert 1468 µs in Ruhe, 2572 µs im Einmessen (Grenze 15000 µs). Hinweis: Das ist die simulierte Zeit bei ruhigen Sensorwerten (gyro = 0). Mit echten Bewegungen und echtem I²C-Bus kann es länger dauern, es ist aber viel Reserve. |

## Fehlerbeschreibung für den Developer (in Worten, ohne Testcode)

Die Winkelmessung stimmt, solange sich immer nur **eine** Sache ändert. Sie versagt bei Kombinationen:

1. **Beugen und gleichzeitig Unterarm drehen** (auch bei normal eingebauten IMUs): der Winkel weicht bis ca. 46° ab. Das Drehen des Unterarms bei festem Ellbogen (AK-1) ist dagegen richtig. Auch ein **Unterarm, der vor dem Beugen schon verdreht ist** (z. B. 80°), liefert beim Beugen einen falschen Winkel (bis 58°).
2. **Ganzer Arm bewegt sich (Neigen, Schwenken, Oberarm drehen) und die beiden IMUs sind verschieden um die Knochenachse verdreht eingebaut:** in 20 von 25 Kombinationen wächst der Fehler auf über 100°. Bei gleich eingebauten IMUs (Dreh-Winkel 0) und diesen Bewegungen ist es richtig. Der Developer hat vermutet, dass die Achse der Oberarm-IMU beim Einmessen nicht sicher bestimmbar ist (nur der Unterarm bewegt sich): Das passt zu diesem Befund.
3. **Sensor-Rauschen (Gyro 0,1°/s, Beschleunigung 0,01 g) plus Bewegung in mehreren Richtungen:** schon nach wenigen Sekunden bis 10° Fehler, im 10-Minuten-Lauf mit den Offsets von ±2°/s weicht der Winkel völlig ab (Fehler über 300°, «Winkel kippt um»). Ohne Rauschen und Offsets ist es sauber.
4. Dasselbe Problem zeigt sich nach einem Abbruch «nicht ruhig» (AK-7), nach einem Ausfall (AK-13) und im 10-Minuten-Dauerlauf (AK-10, AK-11).

Reine Beugung (0 bis −100°, 0,5 Hz) ohne weitere Bewegung hat bis 1,7° Fehler. Das ist knapp unter der Grenze von 2°.

## Bedrohungen (aus docs/bedrohungen.md)

| Bedrohung | geschützt / nicht geschützt / akzeptiertes Risiko | Begründung |
|---|---|---|
| B-2 Falsche Sensordaten | geschützt (Logik), Übersteuerung nicht getestet | Alle 12 Messwerte und `dt` werden auf NaN, unendlich und unmögliche Grössen geprüft, abgelehnte Zyklen ändern den Zustand nicht, Fuzz ohne Absturz, `abweichungGrad` nie NaN. Gyro-Anschlag (±32768) wird in `jointsense.ino` übersprungen: nur durch Durchsicht bestätigt, in Wokwi nicht erzeugbar. |
| B-3 Lahmlegen | teilweise geschützt | RAM 53 %, keine dynamische Speicherverwaltung, Zykluszeit 1,5 bis 2,6 ms (simuliert). **Hängender I²C-Bus: nicht geschützt** (`Wire.setWireTimeout` fehlt, kein Watchdog). Gehört laut REQ-002 nicht dazu, bleibt offen. |
| B-6 Falsche Einmessung / Selbstkorrektur | **nicht geschützt** (teilweise) | Geschützt: Abbruch bei zu kleiner Beugung, Referenz wird nie automatisch verändert (+8° bleiben 10 min Warnung), nach Ausfall keine Warnung bis die Lage bekannt ist (4,5 s). **Nicht geschützt:** Bei Bewegungen in mehreren Richtungen und bei verdrehten IMUs kann der Winkel völlig falsch werden. Dann gibt es Fehlalarme oder verpasste Warnungen. |

## Nicht prüfbar / nicht geprüft

- Wokwi hat **kein Gyroskop** im simulierten MPU6050. Darum nicht prüfbar: W3 (Phase 1 nicht ruhig), W5 (erfolgreiches Einmessen, Bestätigung 150 ms, Gesamtdauer 6000 ms, Ausfall danach), Übersteuerung (AK-14). Empfehlung: Ablauf am echten Board prüfen (Einmessen, Bestätigung, schnelles Drehen).
- REQ-001-Wokwi-Szenarien, die eine gültige Referenz brauchen, sind nicht mehr prüfbar (siehe Tabelle). Die REQ-001-Logik-Tests laufen weiter (147 von 147 ok).
- Zykluszeit nur in der Simulation mit ruhigen Werten, nicht am echten Board.
- Das AK-6-Zählen der FEHLER-Zeilen (`ak06_meldungen_takt`) fiel in einem Lauf einmal um eine Zeile zu knapp aus und bestand in allen Wiederholungen (Messstreuung beim Schreiben der Log-Datei, nicht reproduzierbar).

## Fehlende Werkzeuge

Keine. `arduino-cli`, `clang++`, `cppcheck`, Python und `wokwi-cli` (mit Token) waren vorhanden. `g++` fehlt, `clang++` wird genutzt.

## Dateien dieser Runde

- Berichte/Logs: `scratch/r002/` (nicht im Repo).
- Neu: `docs/wokwi/szenarien/req002_w1_einschalten.yaml`, `req002_w2_einmessen_ohne_bewegung.yaml`, `req002_w4_imu_ausfall_in_phase2.yaml`, `pruefe_REQ-002.py`.
- Geändert: REQ-001-Szenarien (Vorspann), `pruefe_REQ-001.py`, `docs/wokwi/szenarien/README.md`, `docs/wokwi/REQ-002-stoerfaelle.md`.
