# Testbericht REQ-002, Runde 3 (die letzte)
Datum: 2026-11-06   Ergebnis: **NICHT BESTANDEN** (knapp, siehe unten)

Kurz:
- **Gut:** Die in Runde 2 gefundenen grossen Fehler sind weg. Unterarm drehen bei vorgehaltenem Arm (AK-1), Oberarm drehen (AK-2), Schwenken (AK-3) und alle Armlagen (AK-4, Lagen) stimmen jetzt (grösste Winkeländerung 0,49° bei Grenze 2°). Einmessen mit 2 Mal Beugen und kleinen Kreisen **klappt in 400 von 400 Versuchen** (nie ein Abbruch). Abbruch-Logik, Referenz, Ungültige Werte, Fuzz: in Ordnung.
- **Nicht gut (knapp):** Bei mehreren Prüfungen liegt der Fehler **0,005° bis 0,44° über der Grenze von 2°**: IMU verdreht im Sleeve (AK-4, bis 2,084°), Einmessen mit kleinen Kreisen (AK-6, bis 2,27° bei 15°/20°), Drift mit und ohne Ruhe (AK-10/AK-11, 2,005° bis 2,029°). Ein Prüfpunkt in AK-13 (kein Wert über 2° direkt nach einem Ausfall) liegt bei 2,25°, aber nie über 5°.
- **Neu gefunden (Zusatzmessung):** Das Einmessen verträgt **keine Schieflage, bei der die Scharnier-Achse kippt**. Schon 1° gibt 1,8 bis 2,5° Fehler, 2° gibt 2,6 bis 4,6°, 5° gibt 7 bis 10°, ab 10° (bzw. ab 5° in einem Fall) bricht das Einmessen immer ab. Siehe Abschnitt «Zusatzmessung (c)».
- **Wokwi:** Kontingent weiterhin aufgebraucht. Alles, was nur Wokwi oder das Board prüft, ist **nicht geprüft**.

## Prüfkette

| Schritt | Ergebnis | Bemerkung |
|---|---|---|
| Kompilieren + RAM/Flash | bestanden, **Flash-Risiko** | `arduino-cli compile --fqbn arduino:avr:uno --warnings all`: **0 Warnungen**. **Flash 31882 von 32256 Byte (98 %), nur 374 Byte frei.** RAM 1182 Byte (57 %, Grenze AK-15: 1434 Byte = 70 %), 866 Byte frei. |
| cppcheck | bestanden | `warning,style,performance,portability` auf `jointsense.ino` und `logik.cpp`: keine Funde. |
| clang++ Logik- und Fuzz-Tests | **nicht bestanden** | Compiler `clang++` (kein `g++`). `test_REQ-001` (Regression): **147 von 147 ok**. `test_REQ-002`: 249 von 253 ok (4 Fehler: AK-4 Einbau ×3, AK-13 ×1). `test_REQ-002_drift`: 25 von 28 ok (3 Fehler: AK-10 ×2, AK-11 ×1). `test_REQ-002_oberarm`: 26 von 31 ok (5 Fehler: AK-6 ×3, 1 Mal Beugen ×1, Wiederholung nach Abbruch ×1). Alle Testdateien übersetzen ohne Fehler. Laufzeit 5 s. |
| Python-Szenarien | bestanden | `ausfall_verlaeufe.py`, `winkelverlauf.py` laufen, REQ-001-Tests damit ok. |
| Wokwi (W1, Versuch) | **nicht geprüft, Wokwi nicht verfügbar** | Ein Lauf (W1): `API Error: You have used up your Free plan monthly CI minute quota.` Abgebrochen, keine weiteren Läufe, kein Skript gestartet. (Ich habe dafür das neue `.hex`/`.elf` nach `scratch/build/` kopiert.) |
| Bibliotheken | bestanden | Nur `Wire` (Core `arduino:avr` 1.8.8, im Core enthalten) sowie `math.h`, `stdint.h`, `stddef.h`. Stimmt mit `docs/tech-stack.md` überein. Keine IMU-Bibliothek (im tech-stack weiter `TODO`). |
| Sicherheitsregeln im Code | teilweise | Kein `String`, kein `new`/`malloc`. Pins nur über `pins.h`-Namen. `isnan`/`isinf`/`isfinite` 12 Mal in `logik.cpp`. `Wire.setClock(400000)` (Zeile 87), Gyro ±500 °/s (Register `0x1B`), Übersteuerung ±32767/−32768 in `jointsense.ino` Zeile 352 (alle drei Achsen), `ellbogenLageVerwerfen` und `einmessenAbbrechen` im Ablauf. **`Wire.setWireTimeout` fehlt** (B-3, laut REQ-002 «Nicht dazugehörig»). |

## Verlangte Beobachtungen

| Beobachtung | Ergebnis |
|---|---|
| **(a) Zeilen `RESERVE`** | `test_REQ-002`: grösste Winkeländerung AK-1 bis AK-3 = **0,492°** (Grenze 2°, gute Reserve); grösster Fehler AK-4 = **2,084°** (Grenze 2°, **überschritten**). `test_REQ-002_drift`: grösster Fehler AK-10 (mit Ruhe) = **2,029°**, AK-11 (ohne Ruhe) = **2,023°** (beide **über** 2°). Zum Vergleich Runde 2: 1,998° / 1,956° / 1,983°. Runde 3 ist an diesen Stellen **etwas schlechter** als Runde 2 (jetzt knapp darüber). |
| **(b) Flash / RAM** | Flash **31882 / 32256 Byte = 98 %** (374 Byte frei, Runde 2: 334 frei). RAM **1182 Byte = 57 %**, 866 Byte frei. RAM ok (≤ 70 %). Flash: jede Erweiterung (z. B. `Wire.setWireTimeout`) kann sprengen. |
| **(c) Schiefes Einmessen** | Siehe eigener Abschnitt unten. |
| **(d) Kleine Kreise klappen** | **400 von 400** Einmessungen klappen (nie Abbruch), 100 pro Fall. Danach Fehler ≤ 2°: **71 / 78 / 62 / 71 von 100** (siehe Tabelle unten). |

### Zusatzmessung (d): kleine Kreise (Information)

100 Einmessungen je Zeile, zufällige Einbaulage beider IMUs (volle 3D-Drehung), Oberarm-Drehung beim Einmessen abwechselnd 0° und 90°, Sensor-Rauschen, in zwei von drei Fällen zusätzlich Gyro-Offset ±2 °/s. Beuge-Amplitude 70°, genau 2 Mal. Danach 8 s alles gleichzeitig (Beugen 0 bis −100°, Hand drehen, Oberarm drehen, Schwenken, Neigen).

| Kreise | Einmessen klappt | Fehler ≤ 2° | Fehler ≤ 2,5° | Fehler ≤ 3° | grösster Fehler |
|---|---|---|---|---|---|
| 1 Umlauf, 15° | 100 / 100 | 71 | 100 | 100 | 2,23° |
| 1 Umlauf, 20° | 100 / 100 | 78 | 100 | 100 | 2,28° |
| 2 Umläufe, 15° | 100 / 100 | 62 | 100 | 100 | 2,44° |
| 2 Umläufe, 20° | 100 / 100 | 71 | 100 | 100 | 2,27° |

Lesart: Das Einmessen **klappt immer** (das Gerät bricht nicht ab, rät aber nach dem Einmessen manchmal bis ca. 0,3 bis 0,4° daneben). Mit 2° Grenze ist der Winkel aber nur in etwa 6 bis 8 von 10 Einmessungen innerhalb der Grenze, mit 2,5° Grenze in 10 von 10. (Die Grenze hängt an meiner Simulation mit schwierigen Bewegungen; im Alltag kann es besser oder schlechter sein.)

### Zusatzmessung (c): Empfindlichkeit gegen schiefes Einmessen (Information, kein Bestanden/Nicht bestanden)

Aufbau: Beim Einmessen hängt der Arm gestreckt, aber um δ schief (0, 2, 5, 10, 20°, dazu feiner 0,5 / 1 / 1,5 / 3°), jeweils nach vorn, nach hinten, nach links, nach rechts. Einmessen wie im Betrieb (2 s still, 4 s 2 Mal Beugen + Kreise 20°). Danach die Prüfungen AK-1 bis AK-4 (75 Läufe je Zeile: Unterarm drehen 12 Läufe, Oberarm drehen 4, Schwenken 6, Lagen 3, je mit 3 Einbaulagen der IMUs), mit Sensor-Rauschen. Gemessen: grösste Winkeländerung (AK-1 bis AK-3) und grösster Fehler zum echten Winkel.

**Wichtig:** Entscheidend ist nicht «vorn oder seitlich», sondern **ob die Scharnier-Achse des Ellbogens beim Einmessen noch waagrecht liegt.** Beim normal hängenden Arm (Oberarm nicht in der Schulter gedreht, Ellbogen beugt nach vorn) liegt die Achse quer (links-rechts). Dann gilt:

| Schieflage beim Einmessen | Achse bleibt waagrecht (Schieflage nach **vorn/hinten**) | Achse kippt (Schieflage **seitlich**, links/rechts) |
|---|---|---|
| 0° | alles ≤ 0,75° | alles ≤ 0,75° |
| 0,5° | – | Fehler 1,10 bis 1,12° |
| 1° | – | Fehler **1,79 bis 1,83°** (knapp ok) |
| 1,5° | – | Fehler **2,58 bis 2,73°** (zu viel) |
| 2° | Fehler 0,57 bis 0,71° | Fehler **3,00 bis 3,07°** (zu viel) |
| 3° | – | Fehler 4,77 bis 4,81° |
| 5° | Fehler 0,51 bis 0,61° | Fehler **7,68 bis 7,90°** |
| 10° | Fehler 0,66 bis 0,77° | **Einmessen bricht immer ab** (75 von 75) |
| 20° | Fehler 0,83 bis 0,99° | **Einmessen bricht immer ab** (75 von 75) |

(«–» = nicht gemessen. «Fehler» = grösster Fehler zum echten Winkel über alle Läufe von AK-1 bis AK-4. Der Lagen-Unterschied in AK-4 ist bei seitlicher Schieflage 1,2° bei 1°, 2,4 bis 2,5° bei 2°, 5,5 bis 6,4° bei 5°.)

Der **Gegentest** bestätigt es: Ist der Oberarm beim Einmessen in der Schulter um 90° gedreht (twist 90°, dann liegt die Achse in Blickrichtung), ist es genau umgekehrt: Schieflage seitlich bis 20° ist harmlos (Fehler ≤ 0,95°), Schieflage **nach vorn/hinten** gibt 2° → 3,3 bis 4,6°, 5° → 10,1 bis 10,4°, ab 10° Abbruch (bei 5° nach vorn bricht es in 38 von 75 Fällen ab).

**Antwort für das Team:**
- **Schieflage in der Richtung, in der die Scharnier-Achse waagrecht bleibt** (Arm schwingt nach vorn oder hinten, solange man den Ellbogen normal nach vorn beugt): **bis mindestens 20° kein Problem** (Fehler ≤ 1°).
- **Schieflage in der anderen Richtung (Achse kippt, beim normalen Arm = Arm seitlich ausgestellt):** Der Fehler bleibt **≤ 2° nur bis etwa 1°** Schieflage. Ab 1,5° ist er schon über 2°. Das ist **praktisch nicht von Hand einzuhalten**. Bei 10° und mehr bricht das Gerät immer ab (das ist die fail-safe Seite, es zeigt dann `Grund: Oberarm`, bzw. liegt die Achse nicht eindeutig).
- Faustregel aus der Messung: Fehler ≈ 1,5 bis 2,5 × Schieflage (bei kleinen Winkeln). Der Arm muss beim Einmessen also **praktisch exakt seitlich anliegen**, die Ellbogenachse muss waagrecht sein (die Knochen hängen senkrecht, nicht seitlich abgespreizt).
- Wichtig für die Entscheidung: Der Bruch bei 2° bis 5° gibt **falsche Winkel ohne Warnung** (bis 10°), nur ab 10° bricht das Einmessen ab. Das widerspricht dem Grundsatz «entweder richtig oder Abbruch» (AK-7) im Bereich 1,5° bis ca. 8°.

## Akzeptanzkriterien

| Kriterium | Ergebnis | Was passiert ist (in Worten) |
|---|---|---|
| AK-1 Unterarm drehen | **bestanden** (Logik); Wokwi nicht geprüft | In allen Armlagen (hängend, vorn, oben, seitlich) bei 0°, −30°, −90°: grösste Änderung **0,49°** (Grenze 2°), Winkel immer bekannt, kein Alarm. Der grosse Fehler aus Runde 2 (vorn gehalten und gebeugt: 50 bis 146°) ist **behoben**. |
| AK-2 Oberarm drehen | **bestanden** (Logik) | Bei 0° und −90°, Arm hängend und vorn: Änderung ≤ 0,49° (gemeinsam mit AK-1 bis AK-3). |
| AK-3 Arm schwenkt | **bestanden** (Logik) | Bei 0°, −30°, −90°, Arm vorn und schräg: Änderung ≤ 0,49°. |
| AK-4 Lage und Einbaudrehung | **nicht bestanden (knapp)** | Lagen (hängend, vorn, oben, seitlich): bestanden (Unterschied klein). **IMUs im Sleeve verdreht:** Achse 0: grösster Fehler **2,026°** (1 von 25 über 2°), Achse 1: 1,982° (ok), Achse 2: **2,043°** (2 von 25 über 2°), 12 zufällige Einbaulagen: **2,084°**. Alle Einbauten lassen sich einmessen, Winkel immer bekannt. Überschreitung nur 0,03° bis 0,08°. |
| AK-5 Messbereich, Auflösung, Erkennung | **bestanden** | −100 bis +15° auf 2° genau, 1° echt = 0,5 bis 1,5° Wert, Erkennungsrate ≥ 95 % (kein Test in diesem Block fehlgeschlagen). |
| AK-6 Einmessen Ablauf | **nicht bestanden (Logik, knapp)**; Ablauf **nicht geprüft** | Einmessen mit 2 Mal Beugen + Kreisen 15°/20° klappt immer (400 von 400, auch mit alter Referenz). Danach stimmt der Winkel aber nicht in allen Fällen auf 2°: Prüfung mit 5 Einbaulagen: 15° Kreise: Einbau 1 mit 2,23° daneben; 20°: Einbau 1 und 3 mit 2,26 / 2,10°; 2 Umläufe 15°: Einbau 1, 3, 4 mit 2,27 / 2,10 / 2,10°. Mit alter Referenz (zweites Einmessen bei −20°) in allen Fällen ok. Wokwi (Stoss 100 ms, Bestätigung 150 ms, 6000 ± 100 ms): **nicht geprüft**. |
| AK-7 Abbruch | Logik **bestanden**, 2 Folgeprüfungen **knapp nicht**; Texte **nicht geprüft** | (a) Spannweite zu klein, (b) unruhige Phase 1, (d) Oberarm still / 3° / nur um die Senkrechte: bricht ab, nach Abbruch Lage bekannt, nie `true`, alte Referenz bleibt. (c) IMU-Ausfall und Meldungstexte `Grund: …` samt Reihenfolge: nicht geprüft (Wokwi). **Knapp nicht:** (1) «nur 1 Mal beugen, Kreise 20°» (nicht mehr Bedienung): Einmessen klappt, aber danach bis 2,1° falsch (Einbau 3, 4). (2) «erst Abbruch, dann zweiter Versuch» ist bei Einbau 3 nicht innerhalb 2° (genauer Wert nicht ausgegeben; Einbau 3 liegt bei den Kreis-Tests bei 2,01 bis 2,10°). Beides ist dieselbe Ursache wie bei AK-4/AK-6 (Reserve fehlt), nicht ein Fehler beim Abbrechen selbst. |
| AK-8 Wiederholgenauigkeit | **bestanden** | 10 Einmessungen: Streuung innerhalb 3°. |
| AK-9 Einschalten | Logik **bestanden**; Ablauf **nicht geprüft** | Ohne Einmessen nie eine Abweichung (auch bei überstrecktem Arm). Zeile `Keine Referenz` alle 1000 ms ± 100 ms: Wokwi nicht verfügbar. |
| AK-10 Drift mit Ruhe | **nicht bestanden (knapp)** | 10 min, Fehlermuster 0: **2,005°** (bei t = 32,5 s), Fehlermuster 1: **2,029°** (bei t = 508,5 s). Grenze 2°. Winkel immer bekannt. (Runde 2: 1,956° / 1,909°.) |
| AK-11 Drift ohne Ruhe | **nicht bestanden (knapp)** | 10 min Dauerbewegung, Fehlermuster 0: **2,023°** (bei t = 272,5 s). Fehlermuster 1 hat bestanden. (Runde 2: 1,983° / 1,832°.) |
| AK-12 Referenz bleibt sicher | **bestanden** | +8° 10 min halten: Warnung die ganze Zeit an. Ruhe: Drift ≤ 2° in 10 min. Referenz wird nie automatisch verändert. |
| AK-13 Nach einem Ausfall | **bestanden (Alarm)**, ein Prüfpunkt streng **nicht**; Wokwi nicht geprüft | Direkt nach `ellbogenLageVerwerfen` keine Abweichung, Lage nach 0,48 s wieder bekannt (≤ 5 s), danach bleibt sie bekannt, danach Fehler ≤ 1,7°. Bewegt sich der Arm während des Ausfalls weiter: in 19 von 226 Zyklen direkt nach der Wiederkehr ist der Wert bis **2,25°** falsch, **nie über 5°**, also kein Fehlalarm im Sinn von «Motor an». Der Prüfpunkt «nie über 2°» schlägt an (wie in Runde 2, damals 2,2°). Ich werte wie in Runde 2: Fehlalarm-Schutz bestanden. |
| AK-14 Ungültige Werte | Logik **bestanden**; Übersteuerung **nicht geprüft** | NaN, unendlich, > 16 g, > 2000 °/s, `dt` ≤ 0 oder > 0,5: `false`, Zustand unverändert. Fuzz: kein Absturz, nie NaN. Übersteuerung (Rohwert ±32767/−32768 überspringt den Zyklus): im Code vorhanden, in Wokwi nicht geprüft. |
| AK-15 Tempo und Speicher | RAM **bestanden**; Zeit **nicht geprüft** | RAM 57 % (≤ 70 %), keine dynamische Speicherverwaltung, Zeile `Zyklus max: <n> us` im Code vorhanden. Zykluszeit nicht gemessen (kein Wokwi, kein Board). |

## Bedrohungen (aus docs/bedrohungen.md)

| Bedrohung | geschützt / nicht geschützt / akzeptiertes Risiko | Begründung |
|---|---|---|
| B-2 Falsche Sensordaten | **geschützt** (Logik), Übersteuerung **nicht geprüft** | Alle Messwerte und `dt` werden auf NaN, unendlich und unmögliche Grössen geprüft. Abgelehnte Zyklen ändern den Zustand nicht. Fuzz ohne Absturz, nie NaN. Gyro-Anschlag in `jointsense.ino` nur durch Lesen des Codes bestätigt. |
| B-3 Lahmlegen | **teilweise geschützt** | RAM 57 %, keine dynamische Speicherverwaltung. **Flash 98 %**, 374 Byte frei. Zykluszeit und Dauer `einmessenAbschliessen` nicht gemessen. Hängender I²C-Bus: **nicht geschützt** (`Wire.setWireTimeout` fehlt, kein Watchdog), laut REQ-002 nicht Teil dieser Anforderung. |
| B-6 Falsche Einmessung / Selbstkorrektur | **teilweise geschützt** | Geschützt: Abbruch bei zu kleiner Beugung, unruhiger Phase 1, still hängendem oder nur um die Senkrechte bewegtem Oberarm. Referenz wird nie automatisch verändert. Nach Ausfall keine Warnung bis die Lage bekannt ist (0,48 s). **Nicht geschützt:** Einmessen mit **seitlich schief hängendem Arm** (Achse kippt, ab ca. 1,5°): der Winkel ist danach bis ca. 10° falsch, **ohne** dass das Gerät abbricht (Abbruch erst ab ca. 10° Schieflage). Das kann Fehlalarme oder verpasste Warnungen geben, es ist ein Rest-Risiko für die Bedienung. |

## Nicht geprüft / nicht prüfbar

- **Alle Wokwi-Läufe** (Versuch W1 meldet wieder die Kontingent-Meldung, danach keine weiteren Läufe): betrifft AK-6 Ablauf (Stoss 100 ms, Bestätigung 150 ms, 6000 ± 100 ms), AK-7 Meldungstexte `Grund: …`, Reihenfolge der Gründe und IMU-Ausfall (c), AK-9 Zeile `Keine Referenz`, AK-13 im Gesamtgerät, AK-14 Übersteuerung, AK-15 `Zyklus max`, `rotationZ`.
- **Empfehlung Test am echten Board** (ein Mensch lädt hoch): (1) Zeile `Zyklus max: <n> us` im Seriellen Monitor lesen (Grenze 15000 us); Dauer von `einmessenAbschliessen` (geschätzt 0,2 bis 0,4 s) beobachten, ob die Bestätigung pünktlich nach 6000 ms kommt. (2) Einmessen mit Taste, Stoss und Bestätigung zählen und zeitlich schätzen. (3) Abbruchtexte provozieren (ohne Bewegung → `Grund: Beugung`, Hand beim Stillhalten drehen → `Grund: ruhig`, Oberarm still → `Grund: Oberarm`, IMU abziehen → `Grund: IMU`). (4) Hand rasch drehen: kein Fehlalarm, keine Meldung «IMU-Ausfall» (Übersteuerung). (5) Beim Einmessen den Arm absichtlich seitlich schief halten und beobachten, ob der Winkel danach falsch ist.
- Zykluszeit auf dem Uno.
- Drift: nur zwei Fehlermuster (Gyro-Offset je Achse) simuliert.

## Fehlerbeschreibung für den Developer (in Worten, ohne Testcode)

1. **Reserve fehlt (AK-4, AK-6, AK-10, AK-11):** Der Winkel ist an mehreren Stellen um 0,005° bis 0,44° zu ungenau (Grenze 2°): IMU im Sleeve verdreht (bis 2,08°), nach dem Einmessen mit kleinen Kreisen (bis 2,27°; in 100 zufälligen Einbaulagen bei Kreisen von 15° nur in 62 bis 78 von 100 Fällen ≤ 2°, immer ≤ 2,5°), Drift 10 min (2,005° bis 2,029°). Kein einzelner Ausreisser, sondern das ganze Niveau liegt knapp über der Grenze.
2. **Seitlich schiefes Einmessen (nicht in der Anforderung, B-6):** Hängt der Arm beim Einmessen so, dass die Ellbogenachse nicht waagrecht ist, sind die Winkel danach falsch (1° Schieflage → ca. 2°, 2° → 3 bis 4,6°, 5° → bis 10°), aber das Einmessen bricht erst ab ca. 10° ab. Die Bewegung der Phase 2 gibt dem Gerät offenbar nicht genug Information, um die Achse zu prüfen. Entweder ist die Achse richtig oder das Einmessen soll abbrechen (AK-7).
3. **Flash 98 %:** 374 Byte frei.

## Dateien dieser Runde

- Logs und Zusatzmessungen in `scratch/r002d/` (nicht im Repo): `compile.txt`, `cppcheck.txt`, `logiktests.txt`, `wokwi_w1.txt` (Kontingent-Meldung), `schief.txt`, `schief_fein.txt` (Zusatzmessung c), `probe.cpp` (Zusatzprogramm für (c), (d), AK-13-Fenster, baut auf `sim_ellbogen.h` auf). Die Zeile «0 Grad» in `schief_fein.txt` steht für 0,5° (gerundete Ausgabe), «1» = 1°, «2» = 1,5°, «3» = 3°.
- Kein Produkt-Code und keine Testdatei geändert. Nichts aufs Board geladen, kein serieller Port, nichts committet.
