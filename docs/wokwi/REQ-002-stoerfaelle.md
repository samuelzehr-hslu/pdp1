# Wokwi-Prüfung zu REQ-002 (Beschreibung der Störfälle)

> **ÜBERHOLT (Neustart Runde 0, Phase 3 «Achse messen»):** Die Szenarien W1 bis W6 und der Vorspann der REQ-001-Szenarien
> gehen noch vom alten Ablauf aus (Einmessen 6000 ms, Bestätigung bei t0 + 6000 ms, kein Phase 3, kein `Grund: Hand`).
> Neu dauert das Einmessen 7500 ms ± 100 ms (Phase 3 von t0 + 6000 bis t0 + 7500: zwei Stösse 100/100/100 ms am Anfang, Hand drehen),
> die Bestätigung (150 ms) kommt danach. Wokwi-Kontingent ist aufgebraucht, darum **nicht angepasst und nicht ausgeführt**.
> Was nur in Wokwi oder am Board prüfbar ist (Zeiten, Stösse, Meldungstexte `Grund: …` inkl. `Hand`, Reihenfolge der Gründe,
> Gesamtdauer 7500 ms, Übersteuerung, `Zyklus max`) bleibt bis dahin «nicht geprüft» bzw. Board-Test.
> Die Winkel-Logik (AK-1 bis AK-14, AK-16) läuft unabhängig davon auf dem PC.

**Stand Runde 2 (Auftrag 1c, Anforderung geändert):** Die Szenarien liegen in `szenarien/req002_w*.yaml` und laufen mit
`python docs/wokwi/szenarien/pruefe_REQ-002.py` (Auftrag 2). Die Dateien mit Bewegung (W3 bis W6) werden von
`szenarien/erzeuge_einmess_szenarien.py` erzeugt, nicht von Hand geschrieben.

## Korrektur: Wokwi HAT ein Gyroskop

In Runde 1 stand hier «Wokwi hat kein Gyroskop (Rohwerte immer 0)». **Das war falsch.** Der Test von damals setzte die Regler `gyroX` und `gyroY`.
Diese Regler gibt es beim `wokwi-mpu6050` nicht, und Wokwi meldet dafür keinen Fehler. Richtig sind **`rotationX`, `rotationY`, `rotationZ`**
(in Grad pro Sekunde) und **`accelX`, `accelY`, `accelZ`** (in g), siehe <https://docs.wokwi.com/parts/wokwi-mpu6050>.
Mit einem Testprogramm gemessen (Gyro-Bereich ±500 °/s, 65,5 Rohwerte je °/s, I²C-Adresse `0x69` = IMU 2):

| Regler | Rohwert |
|---|---|
| `rotationX = 60` | 3930 |
| `rotationY = -60` | -3930 |
| `rotationX = 250` | 16375 |
| `rotationX = 1000` | 32767 (Anschlag, wird begrenzt) |

Noch **nicht geprüft** (steht für Runde 2 an): `rotationZ` (gleiches Verhalten erwartet) und ob `-1000` genau `-32768` ergibt.
Hinweis zur Übersteuerung: Der höchste Wert ist beim int16 `+32767`, der tiefste `-32768`. In AK-14 steht «±32768». Die Firmware muss also
**`>= 32767` und `<= -32768`** als Anschlag behandeln (Frage an den Engineer, siehe Bericht).

Folgen: Ein erfolgreiches Einmessen, Phase-1-Unruhe (W3), Oberarm-Abbruch (W6), W5 und die Übersteuerung sind in Wokwi **doch prüfbar**,
und die REQ-001-Szenarien mit Referenz laufen wieder (siehe unten). Plausibilitätsprobe (Auftrag 1c, nur Szenario-Format, keine Prüfung): W5 ohne `expect-pin` lief gegen die alte Firmware aus `scratch/r002/build`:
Wokwi nimmt die ~560 Zeilen an, die Firmware meldet `Referenz gesetzt.` und danach `Abweichung: 30.0 Grad -> VIBRATION` bei +30° (Vorzeichen und Achse der Stufen stimmen also).
Laufzeit: 1:27 min für 9,7 s simulierte Zeit (bei Stufen alle 50 ms: 2:37 min, darum 100 ms). Die Läufe mit Einmessen dauern real je ca. 1,5 bis 3 min; `pruefe_REQ-001.py` und `pruefe_REQ-002.py` zusammen sind darum **nicht** in einem 10-Minuten-Befehl zu schaffen, sie werden einzeln gestartet.
Was bleibt unsicher: ob die von Hand gesetzten Stufen (alle 100 ms,
Beschleunigung und Drehgeschwindigkeit aus einer einfachen Arm-Geometrie gerechnet) für die echte Firmware «eine saubere Bewegung» sind. Klappt das
Einmessen in Wokwi nicht, ist erst zu klären, ob es an den Stufen oder an der Firmware liegt (Vergleich mit den PC-Tests, die dieselbe Geometrie
mit 50 Hz rechnen). Es wird nicht geraten.

## Wie die Bewegung nachgestellt wird

Wokwi setzt Sensorwerte nur in Stufen (`set-control`). Das Skript rechnet die Bewegung alle 100 ms in Stufen um:
Beide IMUs liegen im Ruhezustand gleich (0/0/1 g, Arm gestreckt = 0°), die Scharnier-Achse ist die Sensor-Achse Y. Einmess-Bewegung (AK-6):
Phase 2 von t0+2100 bis t0+5900 ms (je 100 ms Luft zum Stoss und zur Bestätigung): Unterarm beugt 2 Mal 70° und streckt, der Oberarm macht dabei
kleine Kreise (20° um zwei waagrechte Achsen). Beschleunigung = Lage in der Mitte der Stufe, Drehgeschwindigkeit = Mittelwert der Stufe (grösster Wert 127 °/s,
passt in ±500 °/s). Überstreckung +30° = `accelX 0,5` / `accelZ 0,866`, jetzt als weiche Bewegung (500 ms) mit passender Drehgeschwindigkeit
statt als Sprung. Die Logik der Firmware wird nicht nachgebaut.

Was Wokwi prüft (das, was die Logik-Tests auf dem PC nicht sehen): **Zeiten, Vibrationsstösse, Meldungstexte, Taste, IMU-Ausfall, Zykluszeit-Zeile, Übersteuerung**.
Der Winkel selbst wird auf dem PC geprüft (`docs/tests/logik/test_REQ-002*.cpp`).

Regeln: höchstens 10 s simulierte Zeit je Störfall. Motor-Proben (`expect-pin`) in die **Mitte** der Fenster setzen (Toleranz ± 50 ms).
Zeit t0 = Moment des Tastendrucks (1000 ms nach Start). **Sechs** Störfälle statt der geplanten fünf: Die Anforderung hat vier Abbruchgründe
(a bis d), und jeder Meldungstext braucht einen eigenen Lauf (W2, W3, W4, W6), dazu W1 (Einschalten) und W5 (Erfolg). Alle sind unter 10 s.
Die Entscheidung, ob das zu viel ist, liegt beim Engineer.

## W1: Einschalten ohne Einmessen, Zykluszeit, Übersteuerung (AK-9, AK-15, AK-14) — 7,2 s

- Beide Kabel dran, Arm gestreckt, dann `imu2` kippen (`accelX = 0.5`, `accelZ = 0.866`, ca. +30° überstreckt, ohne Referenz).
- Erwartung: Motor (Pin 9) bleibt die ganze Zeit aus, nach dem Start kein Vibrationsstoss (kein automatisches Einmessen).
- Zeile `Keine Referenz` etwa alle 1000 ms (± 100 ms), Zeile `Zyklus max: <n> us` etwa alle 1000 ms, `<n>` höchstens 15000.
- Schnelles Drehen und Übersteuerung (`imu2`): `rotationX = 250` (gültig, Rohwert 16375), danach `-1000` (Rohwert am unteren Anschlag), danach `1000` (am oberen Anschlag).
  Erwartung: kein `FEHLER`, kein IMU-Ausfall, Motor aus, Gerät läuft weiter (die `Zyklus max`-Zeilen kommen weiter).

## W2: Einmessen ohne Bewegung (AK-6 Ablauf, AK-7 a und d) — 9 s

- Arm ruhig, Taste kurz drücken (t0). Phase 2 ohne Bewegung (IMU-Werte bleiben gleich).
- Erwartung Motor-Pin: aus von t0 bis t0 + 1950 ms; **an** bei t0 + 2050 ms (ein Stoss von 100 ms, gehört zu Phase 2); aus bei t0 + 2200 ms und bis zum Ende. Kein zweiter Stoss.
- Erwartung Text: genau eine Zeile `Einmessen abgebrochen - Grund: Beugung` bei t0 + 6000 ms ± 100 ms (Ende von Phase 2).
  Beide Gründe (a) Beugung und (d) Oberarm treffen zu. Die Anforderung (AK-7, Stand Runde 3) legt die Reihenfolge fest: (c) IMU, (b) ruhig, (a) Beugung, (d) Oberarm. Ganz ohne Bewegung steht also `Grund: Beugung`.
- Keine Bestätigungsvibration: Pin 9 aus bei t0 + 6075 ms und t0 + 6175 ms.
- Danach weiter `Keine Referenz` und kein Alarm bei überstrecktem Arm.

## W3: Arm in Phase 1 nicht ruhig (AK-7 b) — 7,5 s

- Taste drücken (t0). Von t0 bis t0 + 2000 ms dreht sich nur IMU 1 (Oberarm) mit `rotationX = 60` (Grad/s), IMU 2 ist ruhig («mindestens eine IMU» genügt). Danach normale Einmess-Bewegung (Beugen 70°, Oberarm-Kreise), damit (a) und (d) nicht der Grund sein können.
- Erwartung Text: `Einmessen abgebrochen - Grund: ruhig` bis t0 + 6120 ms. Keine Bestätigungsvibration (Pin 9 aus bei t0 + 6075 ms und t0 + 6175 ms). Kein `FEHLER`.
- Ob der Stoss bei t0 + 2000 ms trotzdem kommt, ist in der Anforderung nicht festgelegt, wird darum nicht geprüft.

## W4: IMU fällt in Phase 2 aus (AK-7 c, Zusammenspiel REQ-001) — 9 s

- Taste drücken (t0), normale Einmess-Bewegung. Bei t0 + 3000 ms `kabel2` abstecken (`pressed = 0`), bei t0 + 6200 ms wieder anstecken, danach überstreckt `imu2` weich auf +30°.
- Erwartung: `Einmessen abgebrochen - Grund: IMU`, Meldung `FEHLER` und `IMU 2 (Unterarm)` (REQ-001), Motor folgt nur dem Fehlermuster, **keine** Bestätigungsvibration, Gerät hängt nicht.
- Kabel wieder dran: Meldung `wieder da`, keine Überstreckungs-Warnung (es gibt keine Referenz, das Einmessen war abgebrochen).

## W5: erfolgreiches Einmessen, Bestätigung, Warnung, Übersteuerung (AK-6, AK-9, AK-14) — 9,7 s

- Taste drücken (t0), normale Einmess-Bewegung (Oberarm macht Kreise). Erwartung: Motor aus bis t0 + 1950, **an** bei t0 + 2050 (Stoss), aus bei t0 + 2250, aus während der Bewegung (t0 + 3000, t0 + 4500), **an** bei t0 + 6075 (Bestätigung, 150 ms), aus bei t0 + 6350.
- Kein `Einmessen abgebrochen`.
- Danach `imu2` weich auf +30° überstrecken: Warnung (Motor an) bis t0 + 7500 ms, weil jetzt eine Referenz da ist.
- Übersteuerung bei laufender Warnung: `rotationX = -1000` (t0 + 7600), dann `+1000` (t0 + 8000), dann 0. Erwartung: Motor bleibt **an**, kein `FEHLER`, kein `wieder da`, Gerät läuft weiter (der Zyklus wird übersprungen, es ist kein IMU-Ausfall).
- AK-13 (Ausfall nach erfolgreichem Einmessen, Lage bekannt in höchstens 5 s) passt nicht mehr in 10 s. Er wird in `ak04_ausfall_bei_ueberstreckung` und `ak10_einmessen_bei_ausfall` (REQ-001, Skript `pruefe_REQ-001.py`) geprüft:
  Nach der Wiederkehr zuerst kein Alarm, nach 5,5 s Alarm mit der alten Referenz.

## W6: Oberarm hängt in Phase 2 still (AK-7 d) — 7,5 s

- Taste drücken (t0). Phase 1 ruhig, Phase 2: Unterarm beugt 2 Mal 70° und streckt, **Oberarm bleibt still** (nur IMU 2 bewegt sich).
- Erwartung Text: genau eine Zeile `Einmessen abgebrochen - Grund: Oberarm` bis t0 + 6120 ms, **keine** andere Abbruch-Zeile (Beugung 70° und Ruhe in Phase 1 sind in Ordnung). Keine Bestätigungsvibration (Pin 9 aus bei t0 + 6075 und t0 + 6175). Kein `FEHLER`.
- Gegenprobe «Oberarm bewegt sich 20°, Einmessen klappt» ist W5.

## Nicht in Wokwi prüfbar

- Gyro-Rohwert genau `-32768` und `rotationZ`: noch nicht gemessen (Runde 2). Fällt das anders aus als erwartet, steht es im Bericht.
- Ob die echte Hardware beim schnellen Drehen wirklich am Anschlag klebt: nur am echten Board.
- Zykluszeit des echten Boards: Wokwi zeigt Simulationszeit der Firmware (`Zyklus max`), das ist nur ein Hinweis, kein Beweis für echte Hardware.

## Bestehende REQ-001-Szenarien

Der Vorspann der Szenarien, die eine gültige Referenz brauchen, stellt jetzt ein **erfolgreiches Einmessen** nach REQ-002 nach (Taste, 2 s still,
Beugen und Strecken mit Oberarm-Kreisen, Vorspann 7400 ms statt 5150 ms). Betroffen: `ak04_ausfall_bei_ueberstreckung`, `ak08_wiederkehr_mitten_im_stoss`,
`ak10_einmessen_abbruch`, `ak10_einmessen_bei_ausfall` und der Schluss von `ak11_ohne_referenz` (Einmessen bei t = 18000 ms). `pruefe_REQ-001.py` prüft sie wieder
und kontrolliert zusätzlich, dass der Vorspann geklappt hat (keine Zeile `Einmessen abgebrochen`, ausser im Szenario selbst). Nach einer Wiederkehr gilt
REQ-002 AK-13: erst kein Alarm, spätestens nach 5 s wieder Alarm (statt «nach 400 ms»). Die übrigen REQ-001-Szenarien (`normal_ohne_stoerung`, `ak02_*`,
`ak06_*`, `ak10_keine_bestaetigung`) brauchen keine Referenz und warten im Vorspann nur (5150 ms).
