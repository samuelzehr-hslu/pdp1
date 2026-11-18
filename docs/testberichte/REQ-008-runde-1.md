# Testbericht REQ-008, Runde 1

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-008](../anforderungen/REQ-008.md) (Prototyp 2, Flex, Feather RP2040) |
| **Runde** | 1 von 3 |
| **Datum** | 2026-11-17 |
| **Code-Stand** | nicht committet |
| **Gesamtergebnis** | Code: keine Fehler gefunden. Ein statischer Test schlägt an (Ursache: Test, nicht Code, siehe unten). Alles, was nur am Board prüfbar ist (AK-1 Rate, AK-11, AK-12, Physik), ist **noch nicht geprüft**. Darum noch **nicht abnehmbar**. |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Board `rp2040:rp2040:adafruit_feather`, `--warnings all`) | ok | 0 Warnungen, Exit 0 |
| 1b | Speicher | ok | RAM 9316 Bytes von 262144 (3 %), Flash 63668 Bytes (unter 1 %) |
| 2 | `cppcheck` (`.ino` + `logik.cpp`) | ok (2 Stil-Hinweise) | Keine «error» oder «warning». Zwei «style»-Hinweise, siehe Hinweise für den Engineer |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`), beide Durchläufe | ok | Prototyp 1: 10 Dateien, 0 fehlgeschlagen (723 Prüfungen). Prototyp 2: 4 Dateien, 0 fehlgeschlagen (246 Prüfungen) |
| 3b | Fuzz-Tests (NaN, unendlich, extreme Werte, Zufall) | ok | in den Flex-Tests enthalten: 500 000 + 200 000 Zufallsaufrufe in `warnen`, Serial-/Tasten-Fuzz in `bedienung`, Einmessen mit NaN/unendlich |
| 3c | Statische Prüfung `flex_statisch.py` | **1 Fehler** von 38 | `Serial.begin(115200)`-Muster nicht gefunden (Ursache: Test, siehe unten). Alle anderen 37 ok, auch AK-18 (git: `jointsense/` und `tools/gui/` unverändert) |
| 3d | GUI statisch `gui_flex_statisch.py` | ok | 17 von 17 |
| 3e | GUI Verhalten `gui_flex_pruefen.js` (node 24) | ok | 22 von 22 |
| 4 | Wokwi | nicht geprüft | Entfällt (Kontingent aufgebraucht, Feather/DRV2605L gibt es in Wokwi nicht) |
| 5 | Bibliotheken (`docs/tech-stack.md`) | ok | Board-Paket `rp2040:rp2040` fest auf `6.4.0` (Earle Philhower, vom Team erlaubt). Keine weitere Bibliothek: `Wire` gehört zum Board-Paket, DRV2605L direkt über Register |
| 6 | Sicherheitsregeln im Code | ok | kein `String`/`new`/`malloc` (statisch), `Wire.setTimeout(10, true)`, `rp2040.wdt_begin(500)` + `wdt_reset()` in `loop()`, NaN-/Unendlich-Prüfung vor neuer Referenz, Pins nur über `pins.h` (stimmt mit README «Verdrahtung Prototyp 2 (Flex)» überein: A0/GPIO26, SDA 2, SCL 3, Taste GPIO7, LED 13, I²C `0x5A`) |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 | teilweise | 12 Bit und `analogRead(PIN_FLEX)` statisch ok, Zyklus 20 ms im Code (Konstante), 4 ADC-Messungen gemittelt. **Rate 50 ± 2 und «kein Zyklus > 40 ms» nur am Board** (BT-1) |
| AK-2 | erfüllt | Formel an allen gültigen ADC-Werten ≤ 1 % |
| AK-3 | erfüllt | Grenzen 99/100/3995/3996, Extremwerte |
| AK-4 | erfüllt (Logik) | Ausfall nach 3 ungültigen, zurück nach 3 gültigen, Motor ≤ 100 ms aus, LED 5 Hz. Meldung alle 1000 ms nur am Board (BT-3) |
| AK-5 | erfüllt (Logik) | 2 s ruhig, 3 %-Spannweite, Wackeln, 30 s Abbruch, ungültig/NaN = Abbruch. Stoss 300 ms, Meldungen, «alte Referenz bleibt» nur am Board (BT-4, BT-5) |
| AK-6 | erfüllt | Formel, Vorzeichen, Referenz ≤ 0 gibt 0.0 |
| AK-7 | erfüllt | Begrenzen 0..20, Runden (5,4 gibt 5, 5,5 gibt 6), NaN gibt 5, Vorgabe 5 statisch. Start-Meldung nur am Board (BT-11) |
| AK-8 | erfüllt (Logik) | `>`, 80 ms, Hysterese, sofort aus, Grenzwert 0 und 20, Physik-Sim: Motor an 80 ms nach dem Sprung (≤ 100 ms). Echte Latenz mit I²C nur am Board (BT-6) |
| AK-9 | erfüllt | Impulse < 80 ms, Neustart der Entprellung, Zittern |
| AK-10 | erfüllt (Logik) | `motorSollAn` alle Kombinationen, LED 1 Hz / dauernd. Meldung nur am Board (BT-7) |
| AK-11 | teilweise | Vorgabe 100 statisch ok, LED 5 Hz Logik ok. DRV2605L, 3 Fehlschläge, Meldung: **nur am Board** (BT-8) |
| AK-12 | teilweise | 500 ms und `wdt_reset()` statisch ok. Echtes Verhalten (Neustart, Motor ≤ 1 s aus): **nur am Board** (BT-9) |
| AK-13 | erfüllt (Logik + statisch) | Taste 30 ms, `serialZeichenVerarbeiten` (64 Zeichen, `\r\n` = 1 Bestätigung, Fuzz), I²C-Zeitlimit 10 ms statisch ok. Gestörter Bus real: Board (BT-10) |
| AK-14 | erfüllt (Code), 1 Testmuster zu streng | Baudrate ist 115200 (Konstante `SERIAL_BAUD`), `GUI_MODUS 1`, G/D/E-Zeilen. Der Test `flex_statisch.py` sucht nur den Text `Serial.begin(115200)` und findet `Serial.begin(SERIAL_BAUD)` nicht (Ursache **Test**). Datenrate und Zeilenformat am echten Board (BT-1, BT-12) |
| AK-15 | erfüllt (GUI ohne Browser) | statisch 17/17 und node 22/22: kein Netz, Web Serial 115200, Einmessen sendet `\n`, kaputte Zeilen ignoriert. Aussehen, 10-s-Verlauf, echter Browser: BT-13 |
| AK-16 | erfüllt | 0 Warnungen, feste Version, Flash/RAM in `tech-stack.md` (stimmt mit Messung überein) |
| AK-17 | erfüllt | Pins/Adresse nur in `pins.h`, `TEILER_WIDERSTAND_OHM` und `ADC_MAX` je einmal |
| AK-18 | erfüllt | git: keine Änderung in `jointsense/` und `tools/gui/`, Prototyp-1-Tests alle grün |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 Serial-Eingaben | geschützt | `serialZeichenVerarbeiten`-Fuzz (alle 256 Zeichen, lange Zeilen), Tasten-Fuzz, statisch kein `String`/`new`/`malloc`. Board: BT-10 |
| B-2 falsche Sensordaten | geschützt | Fuzz mit NaN/unendlich: nie Alarm, laufender Motor aus, nie Referenz |
| B-3 Lahmlegen | teilweise | Watchdog und I²C-Zeitlimit statisch vorhanden. Wirkung nur am Board (BT-9, BT-10) |
| B-4 Bibliotheken | geschützt | Board-Paket fest `6.4.0`, keine weitere Bibliothek |
| B-5 / B-11 neue Firmware (USB, UF2) | akzeptiertes Risiko | laut REQ |
| B-8 Flex fällt aus | geschützt (Logik) | AK-3/AK-4-Tests, Physik-Sim «ADC 0 während Motor läuft». Real: BT-3 |
| B-9 DRV2605L hängt | nicht geprüft | nur am Board (BT-8, BT-9) |
| B-10 LiPo | nicht im Code lösbar | Mensch prüft Polarität vor dem Anschliessen |

## Fehler in Worten für den Developer

Keine Code-Fehler gefunden. Nichts an den Developer weiterzugeben.

## Test-Hinweis (an den Engineer, nicht an den Developer)

- `docs/tests/flex_statisch.py`, Prüfung «AK-14: Serial.begin(115200)»: schlägt an, **Ursache Test**. Der Code ruft `Serial.begin(SERIAL_BAUD)` mit `const unsigned long SERIAL_BAUD = 115200;` auf. Das ist laut REQ in Ordnung (Baudrate stimmt), das Suchmuster verlangt aber die Zahl direkt in der Klammer. Ich habe es nicht geändert, weil es kein Tippfehler ist, sondern ein inhaltlicher Fehler im Test (Test-Korrektur 1 von 3, falls der Engineer es freigibt). Vorschlag: Muster so ändern, dass auch `Serial.begin(<Konstante>)` mit Konstante = 115200 gilt.

## Am echten Board prüfen

Vorher den anderen im Team sagen, dass ihr das Board braucht. Hochladen macht ein Mensch. **Zuerst nur über USB. Akku-Polarität mit Multimeter prüfen (Rot = Plus am `+` des JST-Steckers), erst dann Akku anstecken.** Details in `docs/tests/REQ-008-uebersicht.md`.

- [ ] BT-1 (AK-1, AK-14): `GUI_MODUS 1`, 60 s Ausgabe aufzeichnen, dann `python docs/tests/gui_flex_host/serial_log_pruefen.py <datei.txt>`: Zeilenformat ok, Rate 48 bis 52 pro Sekunde.
- [ ] BT-2 (AK-1): Flex biegen/strecken: ADC im GUI folgt, gerader Arm etwa 2000 bis 3000.
- [ ] BT-3 (AK-3, AK-4, B-8): Flex-Kabel abstecken (und Draht A0 nach 3V3): Motor ≤ 100 ms aus, LED 5 Hz, Meldung alle 1 s. Wieder anstecken: nach 3 gültigen Werten normal, Referenz noch da.
- [ ] BT-4 (AK-5): Taste kurz, separat Enter und Leerzeichen im Seriellen Monitor: Einmessen startet, LED langsam, nach ca. 2 s Stoss 300 ms. Wackeln: 2 s beginnen neu. Dauer-Wackeln: Abbruch nach 30 s, alte Referenz bleibt.
- [ ] BT-5 (AK-5): Beim Einmessen überstrecken: Motor warnt nicht.
- [ ] BT-6 (AK-8, AK-9): Mit Referenz langsam überstrecken: Motor nach Überschreiten, nicht bei kurzem Zucken. Zurück unter Grenzwert minus 1: aus. Kein Flattern.
- [ ] BT-7 (AK-10): Ohne Einmessen: keine Warnung, alle 1 s «Keine Referenz», LED langsam. Mit Referenz: LED dauernd.
- [ ] BT-8 (AK-11, B-9): DRV2605L abstecken, neu starten: «Motor-Ausfall» alle 1 s, LED 5 Hz, Messen läuft weiter (Rate bleibt 50). `MOTOR_STAERKE` ändern: Stärke spürbar anders.
- [ ] BT-9 (AK-12, B-3): Kurz eine Endlosschleife in `loop()` einbauen (nicht ins Repo): Neustart nach ca. 500 ms, Motor spätestens 1 s nach dem Start aus, auch wenn er vorher vibrierte.
- [ ] BT-10 (AK-13, B-1): Taste mehrfach schnell tippen: ein Druck = ein Einmessen. Zeile über 64 Zeichen mit Sonderzeichen: nichts passiert, danach Enter startet normal. Zeilenende «Beides, NL & CR»: nur **ein** Einmessen. I²C kurz stören (SDA festhalten): Rate im GUI bleibt ca. 50.
- [ ] BT-11 (AK-7): `GRENZWERT_PROZENT` auf 25 und -3: Meldung beim Start, begrenzt auf 20 bzw. 0.
- [ ] BT-12 (AK-14): `GUI_MODUS 0`: lesbare deutsche Texte bei 115200 Baud.
- [ ] BT-13 (AK-15): GUI in Chrome/Edge ohne Internet: Verbinden, Verlauf 10 s mit Grenzwert-Linie, ADC, Widerstand in kΩ, Zustand, Motor, Datenrate; «Einmessen» startet; Kabel abziehen: GUI hängt nicht.
- [ ] BT-14 (Physik, Versuch am Arm): Wie viele Prozent entsprechen 5°, 10°, 15° Überstreckung? Fehlalarme beim Drehen der Hand oder nach starkem Beugen (Kriechen)? Entscheidet, ob 5 % als Vorgabe taugt.

## Hinweise für den Engineer

- `cppcheck` Stil-Hinweise (kein Fehler): (1) `jointsense_flex.ino:401` C-Cast `(const uint8_t *)text` bei `Serial.write`. (2) `logik.cpp:173` Parameter `abweichungProzent` in `grenzwertPruefen` verdeckt die gleichnamige Funktion. Harmlos, optional beim nächsten Durchgang aufräumen.
- Zwei Compile-Varianten: tech-stack nennt RAM/Flash für `GUI_MODUS 0`, ich habe nur `GUI_MODUS 1` gebaut (Wert 1 stimmt mit der Doku überein).
- Die D-Zeilen werden laut `tech-stack.md` nur gesendet, wenn im USB-Puffer Platz ist. AK-14 verlangt «pro Messzyklus genau eine». Das lässt sich nur am Board belegen (BT-1): Fehlen Zeilen bei laufendem GUI, ist die Rate unter 48.
- Wokwi wurde nicht benutzt (REQ-Entscheid).

## Fehlende Werkzeuge

Keine. `arduino-cli`, `clang++`, `cppcheck` 2.22.0, Python und node 24 waren vorhanden. `wokwi-cli` nicht benutzt (Kontingent).
