# Testbericht REQ-003, Runde 1

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-003](../anforderungen/REQ-003.md) |
| **Runde** | 1 von 3 |
| **Datum** | 2026-11-12 |
| **Code-Stand** | nicht committet (`jointsense/` unverändert gegenüber HEAD) |
| **Gesamtergebnis** | ☑ bestanden (automatisch prüfbarer Teil). AK-1, AK-2 und AK-3 (Zeit) sind **nicht geprüft**, nur am Board prüfbar. |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`), alle Warnungen als Fehler | ☑ ok | 0 Warnungen. Ausgabe vom selben, unveränderten Code (`scratch/compile_req003.txt`), nicht neu gelaufen. Flash 28822 Byte (89 %). |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ☑ ok | 1155 Bytes (56 %), unter 75 %. Flash mit 89 % ist knapp, bitte beobachten. |
| 2 | Code-Scanner `cppcheck` | ☑ ok | Kein «error», kein «warning». Nur ein Stilhinweis (`logik.cpp`, `pf` könnte `const` sein), schon in `offene-hinweise.md`. |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`) | ☑ ok | 7 Testdateien, 0 fehlgeschlagen. Siehe unten. |
| 3b | Fuzz-Tests (zufällige und extreme Werte) | ☑ ok (REQ-001/002) / nicht nötig (REQ-003) | REQ-003 hat keine neue Funktion, daher kein Fuzz-Test. Die Fuzz-Tests von REQ-001/002 bestehen weiter. |
| 4 | Wokwi-Simulation inkl. Störfälle | ☐ nicht geprüft | Wokwi-Kontingent diesen Monat aufgebraucht (Team-Entscheid 2026-11-12). Wokwi wurde nicht gestartet. |
| 5 | Bibliotheken (feste Version, offizielle Quelle) | ☑ nicht wiederholt (Code unverändert, keine neue Bibliothek) | Nur `Wire` (im Core) und `math.h`. |
| 6 | Sicherheitsregeln im Code | ☑ teilweise ok | Kein `String`, kein `new`/`malloc`, Pins nur über `pins.h`, `NaN`-Prüfung (`isnan`/`isinf`) in `logik.cpp` vorhanden. `Wire.setWireTimeout` fehlt weiterhin (eigene Anforderung für B-3, schon in `offene-hinweise.md`). Betrifft REQ-003 nicht. |

### Zahlen pro Testdatei (`python docs/tests/alle_tests.py`, Compiler `clang++`)

| Testdatei | Ergebnis |
|---|---|
| `test_REQ-001` | 147 von 147 bestanden |
| `test_REQ-002` | 253 von 253 bestanden |
| `test_REQ-002_drift` | 28 von 28 bestanden |
| `test_REQ-002_hand` | 35 von 35 bestanden |
| `test_REQ-002_oberarm` | 29 von 29 bestanden |
| `test_REQ-002_schief` | 38 von 38 bestanden |
| `test_REQ-003` (neu) | 11 von 11 bestanden (dazu 8 Zusatz-Prüfungen, die nie als Fehler zählen, alle ok) |

Die bestehenden Tests von REQ-001 und REQ-002 bestehen weiter.

**Korrektur an meinem eigenen Test (Tippfehler, zählt nicht als Test-Korrektur):** In `test_REQ-003.cpp` war in einem `printf` das `\n` als echter Zeilenumbruch gespeichert, die Datei liess sich deshalb nicht übersetzen. Ich habe es zu `\n` zurückgeändert. Was getestet wird, ist unverändert.

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 (Motor aus, LED aus) | ☐ nicht geprüft, nur Board | Automatisch geprüft ist nur die Polung: Anode an D9, Kathode an GND, also D9 = 0 gibt LED aus. Das Verhalten im Betrieb prüft das Team am Board (Anleitung unten). Im Code setzt `feedbackAusgeben(false)` D9 auf 0 (`analogWrite(PIN_MOTOR_PWM, 0)`). |
| AK-2 (Motor vibriert, LED leuchtet) | ☐ nicht geprüft, nur Board | Wie AK-1: Polung ok. Der Code setzt D9 auf `MOTOR_STAERKE` (200). Im Betrieb nur am Board prüfbar. |
| AK-3 (LED und Motor im gleichen Zyklus, ≤ 100 ms) | ☑ Verdrahtung ok / ☐ Zeit nicht geprüft | Automatisch bestanden: Zwischen D9 und LED sitzt nur der 220-Ω-Widerstand, zwischen Kathode und GND nichts. Die LED hängt am selben Pin D9 wie der Motor, daher gibt es keinen eigenen Schaltweg, der verzögern könnte. Die Zeit in ms ist nur am Board von Auge prüfbar (Grenze 100 ms ist von Auge kaum messbar, siehe Hinweis). |
| AK-4 (LED an D9 über 220 Ω, Kathode an GND, passt zu README und `pins.h`) | ☑ erfüllt | `diagram.json`: genau eine Motor-LED, Anode nur an D9, genau ein Widerstand 220 Ω, Kathode nur an GND. `PIN_MOTOR_PWM = 9`, README «D9 → Treiber IN1» stimmt überein. |

## Bedrohungen

REQ-003 nennt keine Bedrohung («Nicht betroffen», keine neue Schnittstelle, kein neuer Befehl).

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| keine betroffen | nicht zutreffend | Siehe Anforderung. |

## Fehler in Worten für den Developer

Keine. Der Code erfüllt, was automatisch prüfbar ist.

## Am echten Board prüfen

**Anleitung für den Board-Test von AK-1, AK-2, AK-3 (von Auge).** Das Hochladen macht ein Mensch, nicht Claude. Alles dauert etwa 10 Minuten.

Was du brauchst: Arduino Uno, Kabel, eine LED (egal welche Farbe), ein Widerstand um 220 Ω, zwei IMUs wie gewohnt, ein Steckbrett oder zwei Kabel.

1. **Strom weg, dann LED stecken.** Das lange Bein der LED (Plus, Anode) in den Widerstand, das andere Ende des Widerstands an **Pin D9**. Das kurze Bein (Minus, Kathode) an **GND**. So wie in `jointsense/diagram.json`. Falsch herum eingesteckt leuchtet die LED nie, wende sie dann einmal.
2. **Motor und Treiber abziehen.** D9 darf nur die LED treiben, sonst ist der Test unklar. Es ist so gewollt (Team-Entscheid 2026-11-12), dass keine Treiber und kein D8 dabei sind.
3. **Hochladen** (Arduino IDE oder `arduino-cli upload`, der Mensch macht das und sagt vorher den anderen im Team Bescheid, wegen des gemeinsamen Boards).
4. **Einschalten und Ruhe halten.** Arm gestreckt, aber nicht über die Grenze. **Prüfe AK-1:** Die LED ist **aus**, solange der Arm ruhig und nicht überstreckt ist.
5. **Einmessen** wie in der README beschrieben (Taste, Anweisungen auf dem Seriellen Monitor, falls du ihn offen hast). Beim Einmessen gibt es kurze Vibrationsstösse. Die LED blinkt dann ebenfalls kurz. Das ist richtig, weil sie dem Motor folgt.
6. **Überstrecke den Arm** über die Grenze und halte ihn dort. **Prüfe AK-2:** Die LED **leuchtet**, solange du überstreckt bleibst.
7. **Arm zurück in den sicheren Bereich.** Die LED geht **aus** (AK-1 nochmals).
8. **AK-3 von Auge:** Wiederhole 6 und 7 mehrmals. Die LED soll praktisch im selben Moment angehen wie der Motor (ohne Treiber also: sofort nach der Entprellung, ohne spürbare Verzögerung). Ein Zeitnehmen auf 100 ms genau ist von Auge nicht möglich. «Kein spürbares Hinterherhinken» genügt als Sichtprüfung.
9. **Ergebnis notieren:** AK-1 ok/nicht ok, AK-2 ok/nicht ok, AK-3 ok/nicht ok. Wenn die LED nie leuchtet: zuerst Polung der LED und Kabel an D9 prüfen. Wenn sie dauernd leuchtet: Kabel an GND prüfen.

Hinweis zur Helligkeit: Bei Stärke 200 von 255 leuchtet die LED etwas weniger hell als bei voller Kraft. Das ist normal, sie muss aber klar sichtbar an sein.

## Hinweise für den Engineer

- **AK-1/AK-2/AK-3 (Zeit) sind nicht geprüft.** Sie können erst nach dem Board-Test abgenommen werden. Status `getestet` ist bei offenem Board-Test mit Vorbehalt zu verstehen.
- **AK-3 («≤ 100 ms») ist von Auge nicht messbar.** Wenn die Zeit wirklich belegt werden soll, braucht es später Wokwi (nächster Monat) oder ein Messgerät. Kleiner Vorschlag: AK-3 sinngemäss lassen und im Board-Test «kein spürbares Hinterherhinken» als Kriterium akzeptieren. Das ist dem Team zu entscheiden.
- Die Wokwi-Störfälle von REQ-001 laufen auf `diagram_kabelbruch.json`. Die hat dieselbe Motor-LED-Verdrahtung wie `diagram.json` (Zusatzprüfung, ok).
- Schon in `docs/offene-hinweise.md`: Wokwi-Pin-Proben bei Stärke 200 unzuverlässig, `Wire.setWireTimeout` fehlt, Flash 89 %, cppcheck-Stilhinweis `pf`. Kein neuer Eintrag nötig, ausser eine Zeile: «REQ-003 Board-Test AK-1/AK-2/AK-3 steht aus (Anleitung im Bericht REQ-003 Runde 1)».
- Die neue Test-Dateien `test_REQ-003.cpp` und `verdrahtung_REQ-003.py` sind noch nicht in Git eingecheckt.

## Fehlende Werkzeuge

Keine. `arduino-cli`, `clang++`, `cppcheck`, Python sind vorhanden. `wokwi-cli` wurde bewusst nicht benutzt (Kontingent aufgebraucht).
