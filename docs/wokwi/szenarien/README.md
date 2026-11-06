# Wokwi-Szenarien (Störfälle)

**Stand REQ-002 (Runde 2):** Wokwi hat ein **Gyroskop**. Die Regler heissen `rotationX`, `rotationY`, `rotationZ` (Grad/s) und `accelX/Y/Z` (g). `gyroX`/`gyroY`
gibt es nicht (Wokwi meldet dafür keinen Fehler, die Werte bleiben 0: daher stand in Runde 1 fälschlich «kein Gyroskop», siehe `docs/wokwi/REQ-002-stoerfaelle.md`).
Ein erfolgreiches Einmessen lässt sich darum nachstellen: `erzeuge_einmess_szenarien.py` rechnet die Einmess-Bewegung (Beugen/Strecken mit Oberarm-Kreisen)
in Stufen alle 100 ms um und schreibt die Szenarien `ak04_...`, `ak08_...`, `ak10_einmessen_abbruch`, `ak10_einmessen_bei_ausfall`, `ak11_ohne_referenz`
und `req002_w1`, `w3` bis `w6` (die `.yaml`-Dateien sind **erzeugt**, nicht von Hand ändern). Der Vorspann dieser Szenarien dauert 7400 ms (nicht mehr 5150 ms).
Die anderen REQ-001-Szenarien (`normal_...`, `ak02_...`, `ak06_...`, `ak10_keine_bestaetigung`) brauchen keine Referenz und warten im Vorspann nur (5150 ms).
`req002_w*.yaml` und `pruefe_REQ-002.py`: siehe `docs/wokwi/REQ-002-stoerfaelle.md`.

Hier liegen die Wokwi-Szenarien zu **REQ-001** (Sensorausfall). Sie prüfen das **ganze Gerät** in der Simulation, also das,
was man in der reinen Logik (`docs/tests/logik/`) nicht prüfen kann: Lesen der IMUs, Serielle Meldungen, Motor-Pin, Taste.

## Wie wird ein Kabelbruch nachgestellt?

Wokwi kann eine IMU nicht mitten im Lauf «abstecken». Darum gibt es die Schaltung `diagram_kabelbruch.json`.
Sie ist eine **Kopie** von `jointsense/diagram.json` mit einem Zusatz:

- In die SDA-Leitung jeder IMU ist ein Taster als «Kabel» eingebaut (`kabel1` = IMU 1 Oberarm, `kabel2` = IMU 2 Unterarm).
- `pressed = 1` heisst: Kabel dran. `pressed = 0` heisst: Kabel ab (die IMU antwortet nicht mehr, der Arduino bekommt kein «ACK»).
- Am Anfang sind beide Kabel **ab**. Jedes Szenario steckt sie zuerst an (ausser `ak11_ohne_referenz`, dort fehlt IMU 2 mit Absicht).
- Das Kippen des Arms: `imu2` mit `accelX = 0.5` und `accelZ = 0.87` entspricht rund +30 Grad (überstreckt). `accelX = 0`, `accelZ = 1` ist gestreckt.
- Der Motor ist die LED an Pin 9. `expect-pin ... pin "9" value 1` heisst: Motor an.

**Wichtig:** Ändert sich `jointsense/diagram.json`, muss `diagram_kabelbruch.json` von Hand nachgezogen werden
(die zwei Taster und ihre vier Kabel bleiben, sonst ist alles gleich).

## Starten

Zuerst kompilieren (lädt nichts aufs Board), dann aus dem Hauptordner:

```bash
arduino-cli compile --fqbn arduino:avr:uno --warnings all --output-dir scratch/build jointsense
python3 docs/wokwi/szenarien/pruefe_REQ-001.py        # Windows: python oder py
```

Das Skript startet alle Szenarien, wertet die Serielle Ausgabe aus und zeigt für jede Prüfung `ok` oder `FEHLER`.
Exit-Code 0 = bestanden, 1 = Fehler, 2 = nicht geprüft (`wokwi-cli` oder Firmware fehlt). Logs: `scratch/wokwi-req001/`.

Ein einzelnes Szenario von Hand (Beispiel):

```bash
wokwi-cli docs/wokwi --diagram-file szenarien/diagram_kabelbruch.json --scenario szenarien/ak02_erkennung_zeit.yaml --timeout 45000 --serial-log-file scratch/wokwi-serial.log
```

## Die Szenarien

| Datei | Prüft | Kurz erklärt |
|---|---|---|
| `normal_ohne_stoerung.yaml` | Gegenprobe (AK-2, AK-8) | Alles ok: kein `FEHLER`, kein `wieder da`, Motor aus. |
| `ak02_kurze_aussetzer.yaml` | AK-2 | Kabel nur 30 ms weg (1 bis 2 Messungen): kein Ausfall, kein `FEHLER`, Motor bleibt aus. |
| `ak02_erkennung_zeit.yaml` | AK-2, AK-5, QA-05 | 25 ms nach dem Kabelbruch noch kein Ausfall (Motor aus), nach 105 ms läuft der erste Stoss. |
| `ak04_ausfall_bei_ueberstreckung.yaml` | AK-3, AK-4, AK-5, AK-6, AK-8, AK-9 (und REQ-002 AK-13) | Arm ist überstreckt (Motor an), Kabelbruch: Warnung geht aus, Muster 3 Stösse (an 100 / aus 100 ...) und nach 5 s von vorn. Kabel wieder dran: Motor sofort aus, kein Sofort-Alarm, nach spätestens 5 s kommt der Alarm mit der alten Referenz zurück (REQ-002 AK-13). |
| `ak06_meldungen_takt.yaml` | AK-6 | Takt der Meldungen: das Skript bricht das Szenario zu verschiedenen Zeiten ab und zählt die Zeilen (1000 ms ± 100 ms). |
| `ak06_beide_ausfall.yaml` | AK-6, AK-8 | Beide IMUs weg: beide werden gemeldet. Kommt nur eine zurück, dauert der Ausfall an. Erst wenn beide da sind: `wieder da`. |
| `ak08_wiederkehr_mitten_im_stoss.yaml` | AK-7, AK-8 | Kabel wieder dran mitten im Stoss: Motor sofort aus. Danach erneuter Ausfall startet das Muster neu. |
| `ak10_einmessen_abbruch.yaml` | AK-10 | Beim Einmessen fällt die IMU kurz aus: `Einmessen abgebrochen`, die alte Referenz gilt weiter (überstreckter Arm löst weiter Alarm aus). |
| `ak10_keine_bestaetigung.yaml` | AK-10 | Abgebrochenes Einmessen: keine Bestätigungsvibration (Motor bleibt aus). |
| `ak10_einmessen_bei_ausfall.yaml` | AK-3, AK-10 | Taste drücken, während eine IMU fehlt: abgebrochen, keine Müllwerte als Referenz. |
| `ak11_ohne_referenz.yaml` | AK-11 | Start ohne IMU 2: keine Warnung, `Keine Referenz` alle 1000 ms, auch nach dem Einstecken. Erst nach Einmessen per Taste kommt die Warnung. |

## Was erwartet wird (kurz)

- Fehler wird gemeldet (`FEHLER` + Name der IMU), der Motor folgt nur dem Fehlermuster, das Gerät hängt nicht.
- Texte, die die Firmware ausgeben muss: `FEHLER`, `IMU 1 (Oberarm)`, `IMU 2 (Unterarm)`, `wieder da` (ohne `FEHLER` in der Zeile),
  `Einmessen abgebrochen`, `Keine Referenz`.

## Messgenauigkeit

- Die Motor-Proben (`expect-pin`) sitzen in der **Mitte** der 100-ms-Fenster. Das Muster darf also um ± 50 ms verschoben sein.
- Die Proben setzen voraus, dass der Motor-Pin bei «an» dauerhaft `HIGH` ist (volle Leistung). Wird der Motor mit weniger als 100 % PWM angesteuert, sind die Pin-Proben unzuverlässig. Das ist dann zu melden.
- Die Zeit in Wokwi ist **Simulationszeit**. Sie ist genau, sagt aber nichts über die echte Geschwindigkeit des Boards.

## Nicht prüfbar (mit Begründung)

| Was | Warum nicht |
|---|---|
| AK-1, zweiter Teil: «weniger als 6 Datenbytes» | Die simulierte IMU liefert bei Bestätigung immer alle 6 Bytes. Es lässt sich nur der Fall «keine Bestätigung» (Kabel ab) nachstellen. Der Fall «zu wenig Bytes» bleibt **nicht prüfbar** (nur durch Code-Durchsicht oder am echten Board mit gestörtem Bus). |
| AK-3 direkt («Werte nie benutzt») | Man sieht nur das Ergebnis: kein Alarm aus Müllwerten (Motor folgt nur dem Muster) und die alte Referenz bleibt gültig (`ak04...`, `ak10_einmessen_bei_ausfall`). Ob irgendwo intern doch ein falscher Wert gerechnet wird, bleibt bei der Code-Durchsicht. |
| AK-7 direkt («mindestens 25 Hz») | Die Firmware gibt keine Messrate aus, und Wokwi zeigt keine echte Zeit des Boards. Indirekt geprüft: Erkennung nach höchstens 100 ms, Wiederkehr nach ca. 45 ms erkannt, Muster stimmt auf ± 50 ms. Die Schleifenzeit mit echter Hardware (Wire-Zeiten) ist **nicht prüfbar**. |
| Hängender I²C-Bus | Gehört zu B-3 (eigene Anforderung), und Wokwi kann ihn nicht nachstellen. |
| AK-9, genaue Entprellzeit (80 ms) | Geprüft ist nur «45 ms nach der Wiederkehr noch kein Alarm» und «nach 445 ms ist der Alarm da». Die genauen 80 ms prüft die bestehende Logik (Entprellung), nicht diese Anforderung. |
