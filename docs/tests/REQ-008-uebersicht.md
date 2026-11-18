# REQ-008 (Prototyp 2, Flex-Sensor): Testplan und Board-Checkliste

Der Developer darf diese Datei nicht lesen (liegt unter `docs/tests/`).

## Wie laufen die Tests?

| Was | Befehl (aus dem Hauptordner) | Prüft |
|---|---|---|
| Logik- und Fuzz-Tests Prototyp 2 | `python docs/tests/alle_tests.py` (zweiter Durchlauf, Ordner `docs/tests/logik_flex/`, Programme `scratch/tests/flex_*`) | AK-2 bis AK-10, AK-13 (Taste), Status-LED, Physik-Annahmen, Fuzz |
| Code-Prüfung (statisch) | `python docs/tests/flex_statisch.py` | AK-1 (12 Bit), AK-7, AK-11, AK-12 (Watchdog-Text), AK-13 (I²C-Zeitlimit ≤ 10 ms, `serialZeichenVerarbeiten` benutzt), AK-14 (GUI_MODUS, 115200), AK-17, AK-18, kein `String`/`new`/`malloc` |
| GUI statisch | `python docs/tests/gui_flex_host/gui_flex_statisch.py` | AK-15: läuft ohne Internet, Web Serial, README |
| GUI Verhalten (node, ohne Browser) | `node docs/tests/gui_flex_host/gui_flex_pruefen.js` | AK-15: Zeilen lesen, Anzeige, kaputte Zeilen, Knopf «Einmessen» sendet `\n`, Datenrate |
| Aufzeichnung vom Board prüfen | `python docs/tests/gui_flex_host/serial_log_pruefen.py <datei.txt>` | AK-14 Zeilenformat, AK-1 Rate (nur mit Aufzeichnung vom echten Board) |
| Kompilieren | `arduino-cli compile --fqbn rp2040:rp2040:adafruit_feather --warnings all --output-dir scratch/build_flex jointsense_flex` | AK-16, RAM/Flash |
| Prototyp 1 weiter ok | `python docs/tests/alle_tests.py` (erster Durchlauf) | AK-18 |

`alle_tests.py` überspringt den Flex-Durchlauf mit «übersprungen, Ordner fehlt», solange `jointsense_flex/` nicht existiert.

## Testplan pro Akzeptanzkriterium

| AK | Womit | Bemerkung |
|---|---|---|
| AK-1 | Logik (nein), statisch, **Board** | 12 Bit und `analogRead(PIN_FLEX)` statisch. Rate 50 ± 2 und «kein Zyklus > 40 ms»: Aufzeichnung vom Board mit `serial_log_pruefen.py` |
| AK-2 | Logik | Formel an allen 3896 gültigen ADC-Werten, Richtung (ADC hoch = R tief) |
| AK-3 | Logik | Grenzen 99/100/3995/3996, Extremwerte, kein Absturz bei 0 |
| AK-4 | Logik + Physik-Sim | 3 ungültige in Folge / 3 gültige in Folge; Motor nach Ausfall aus innerhalb 100 ms; LED 5 Hz. «Meldung alle 1000 ms» nur am Board |
| AK-5 | Logik | 2 s ruhig, 3 %-Spannweite, Wackeln beginnt neu (neues Fenster beginnt mit dem verletzenden Wert: Zeiten auf einen Zyklus genau), 30 s Abbruch, ungültiger Wert, NaN und unendlich = Abbruch, Wiederholung. **Board:** Vibrationsstoss 300 ms, Meldungen, Taste/Serial lösen aus, «alte Referenz bleibt bei Abbruch» (steckt in der `.ino`) |
| AK-6 | Logik | Formel, Vorzeichen, Referenz ≤ 0 gibt 0.0 |
| AK-7 | Logik + statisch | `grenzwertBegrenzen`: begrenzen auf 0..20, runden auf ganze Prozent (5,4 gibt 5, 5,5 gibt 6), NaN gibt 5. Vorgabe 5 statisch. **Board:** Meldung beim Start bei Wert ausserhalb |
| AK-8 | Logik + Physik-Sim | Entprellung genau wie in der Klärung: `>` echt grösser, Motor an bei der ersten Messung ≥ 80 ms nach der ersten Überschreitung (bei 20-ms-Zyklus genau bei 80 ms), Messung darunter setzt zurück, Ausschalten sofort in derselben Messung bei Abweichung < Grenzwert − 1, andere Parameter, Grenzwert 0 und 20. Echte Latenz ≤ 100 ms (mit I²C): **Board** |
| AK-9 | Logik | Impulse kürzer als 80 ms, Neustart der Entprellung, Zittern an der Grenze |
| AK-10 | Logik | `motorSollAn` (alle 16 Kombinationen), LED 1 Hz / dauernd. **Board:** Meldung «Keine Referenz» alle 1000 ms |
| AK-11 | statisch (Vorgabe 100) + Logik (LED 5 Hz) + **nur Board** | DRV2605L suchen, 3 Fehlschläge, Meldung «Motor-Ausfall»: nur mit echtem Treiber |
| AK-12 | statisch (500 ms, Reset im Programm) + **nur Board** | Programm absichtlich hängen lassen; Motor nach Start höchstens 1 s an |
| AK-13 | Logik (Taste 30 ms, `serialZeichenVerarbeiten`) + statisch (I²C-Zeitlimit ≤ 10 ms, die .ino benutzt `serialZeichenVerarbeiten`) + **Board** | Logik: Bestätigung nur bei `\n`/`\r`/`' '` am Zeilenanfang, Zeilen > 64 Zeichen verworfen, Fuzz mit allen 256 Zeichenwerten. Nicht in der Logik prüfbar und darum **Board**: dass ein gestörter I²C-Bus das Messen wirklich nicht anhält, und «CR LF» löst nur ein Einmessen aus (Prototyp-1-Erfahrung: die REQ lässt offen, ob `\r\n` 1 oder 2 Bestätigungen gibt; die `.ino` muss das abfangen) |
| AK-14 | statisch + Aufzeichnung (Board) + GUI-Test (liest dieses Format) | Zeilenformat und Rate mit `serial_log_pruefen.py`. `GUI_MODUS 0` Texte: Sichtprüfung am Board |
| AK-15 | GUI node + statisch + **Aussehen am Board/Browser** | Feste Regeln der REQ: Port per Knopf über `requestPort` und `open({ baudRate: 115200 })`, Knopf «Einmessen» schreibt genau `\n`, Zustandstexte «Keine Referenz», «Bereit», «Einmessen», «Sensor-Ausfall», «Motor-Ausfall» (Gross-/Kleinschreibung zählt), Widerstand als `7.3 kΩ`, Datenrate als Zahl. Aussehen, 10-s-Verlauf, Web Serial in Chrome/Edge: nur mit echtem Browser |
| AK-16 | Kompilieren (Auftrag 2) + Sichtung `docs/tech-stack.md` | Warnungen aus eigenem Code, feste Version, RAM/Flash |
| AK-17 | statisch | Pins/Adresse nur in `pins.h`, Konstanten einmal |
| AK-18 | statisch (git) + Prototyp-1-Tests | `git status` für `jointsense/` und `tools/gui/` leer, erster Durchlauf von `alle_tests.py` grün |

**Wokwi:** wird für REQ-008 **nicht** benutzt. Das Gratis-Kontingent ist im Team als aufgebraucht vermerkt (Entscheid 2026-11-12, siehe `docs/offene-hinweise.md`), und Feather RP2040 und DRV2605L gibt es in Wokwi nicht (ein Pico mit Potentiometer ohne Treiber würde sofort in «Motor-Ausfall» laufen und nichts Neues zeigen). Es wurden keine Szenarien geschrieben und nichts ausgeführt.

## Bedrohungen

| ID | Womit geprüft |
|---|---|
| B-1 (Serial-Eingaben) | `serialZeichenVerarbeiten`-Tests und Fuzz (2 Mio. zufällige Zeichen, 3 Mio. Zeichen ohne Zeilenende, 64-Zeichen-Grenze); Taste-Fuzz; `String`/`new`/`malloc` statisch; Board-Kontrolle BT-10 |
| B-2 (falsche Sensordaten) | Fuzz mit NaN, unendlich, extremen Werten für `abweichungProzent`, `grenzwertPruefen` (nie Alarm, laufender Motor geht aus), `einmessenSchritt` (Abbruch, nie eine Referenz), `grenzwertBegrenzen` (NaN gibt 5) |
| B-3 (Lahmlegen) | Watchdog und I²C-Zeitlimit statisch + Board |
| B-4 (Bibliotheken) | Sichtung `docs/tech-stack.md` (Auftrag 2) |
| B-8 (Flex fällt aus) | Logik AK-3/AK-4, Physik-Sim «ADC 0 während Motor läuft» |
| B-9 (DRV2605L hängt) | statisch (Watchdog) + nur Board |
| B-10 (LiPo) | nicht im Code lösbar, Mensch prüft Polarität und PCM vor dem Anschliessen |
| B-11 (UF2) | akzeptiertes Risiko |

## Board-Checkliste (nur am echten Feather, ein Mensch macht sie)

**Vorher:** Den anderen im Team sagen, dass ihr das Board braucht. Hochladen macht ein Mensch. **Erst über USB, Akku erst nach der Polaritätsprüfung (siehe REQ-008 Hardware-Check).** Neben jeden Punkt **ok** oder **nicht ok** und eine kurze Beobachtung.

- [ ] **BT-1 (AK-1, AK-14):** `GUI_MODUS 1`, 60 s Ausgabe aufzeichnen, `serial_log_pruefen.py` darüber laufen lassen: alles ok, Rate 48 bis 52.
- [ ] **BT-2 (AK-1):** Flex biegen und strecken: ADC-Wert im GUI folgt, gerader Arm liegt etwa bei 2000 bis 3000.
- [ ] **BT-3 (AK-3, AK-4):** Flex-Kabel abstecken: nach höchstens 100 ms Motor aus, LED blinkt schnell (5 Hz), alle 1 s eine Meldung. Wieder anstecken: nach 3 gültigen Werten normal weiter, Referenz noch da (gilt «bereit», LED dauernd an). Dasselbe mit einem Draht von A0 nach 3V3 (Kurzschluss, ADC über 3995).
- [ ] **BT-4 (AK-5):** Taste kurz drücken (und separat Enter im Seriellen Monitor, und Leerzeichen): Einmessen startet, LED blinkt langsam, Arm still: nach etwa 2 s Vibrationsstoss 300 ms und Meldung. Beim Wackeln: kein Abbruch, die 2 s beginnen neu. Dauernd wackeln: nach 30 s Abbruch mit Meldung. War vorher eine Referenz da, warnt das Gerät danach weiter wie vorher.
- [ ] **BT-5 (AK-5):** Während des Einmessens den Arm überstrecken: der Motor warnt nicht.
- [ ] **BT-6 (AK-8, AK-9):** Mit Referenz: Arm langsam überstrecken: Motor startet nach dem Überschreiten des Grenzwerts, nicht schon bei kurzem Zucken. Zurück unter «Grenzwert minus 1 Prozentpunkt»: Motor aus. Kein Flattern an der Grenze.
- [ ] **BT-7 (AK-10):** Nach dem Einschalten ohne Einmessen: keine Warnung, alle 1 s «Keine Referenz», LED blinkt langsam. Mit Referenz: LED dauernd an.
- [ ] **BT-8 (AK-11):** DRV2605L abstecken (SDA oder SCL lösen) und neu starten: Meldung «Motor-Ausfall» alle 1 s, LED 5 Hz, das Messen läuft weiter (Rate im GUI bleibt 50). `MOTOR_STAERKE` ändern: Vibration wird schwächer bzw. stärker spürbar.
- [ ] **BT-9 (AK-12):** Watchdog: Für den Test kurz eine Endlosschleife in die `loop()` einbauen (nicht ins Repo): das Board startet nach etwa 500 ms neu und der Motor ist spätestens 1 s nach dem Start aus, auch wenn er vorher vibriert hat.
- [ ] **BT-10 (AK-13):** Taste mit Prellen (mehrfach schnell tippen): ein Druck = ein Einmessen. Serial: Zeile mit über 64 Zeichen und Sonderzeichen schicken: nichts passiert, kein Absturz, danach ein normales Enter startet das Einmessen. Zeilenende «Beides, NL & CR» (CR LF): es startet nur **ein** Einmessen. Zusätzlich I²C-Bus stören (SDA kurz festhalten): das Messen läuft weiter, die Rate im GUI bleibt etwa 50 pro Sekunde.
- [ ] **BT-11 (AK-7):** `GRENZWERT_PROZENT` auf 25 und auf -3 setzen: Meldung beim Start, Wert wird auf 20 bzw. 0 begrenzt.
- [ ] **BT-12 (AK-14):** `GUI_MODUS 0`: lesbare deutsche Texte, 115200 Baud.
- [ ] **BT-13 (AK-15):** GUI in Chrome oder Edge öffnen (ohne Internet): Verbinden, Verlauf der letzten 10 s mit Grenzwert-Linie, ADC, Widerstand, Zustand, Motor, Datenrate; Knopf «Einmessen» startet das Einmessen; Kabel abziehen: GUI hängt nicht.
- [ ] **BT-14 (Physik, nur Versuch am Arm):** Wie viele Prozent Abweichung entsprechen 5°, 10°, 15° Überstreckung? Gibt es Fehlalarme beim Drehen der Hand oder nach starkem Beugen (Kriechen)? Das entscheidet, ob 5 % als Vorgabe taugt.
