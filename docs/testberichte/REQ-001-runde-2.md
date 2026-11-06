# Testbericht REQ-001, Runde 2

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-001](../anforderungen/REQ-001.md) |
| **Runde** | 2 von 3 (Wiederholung von Runde 1, weil dort die PC-Tests nicht laufen konnten) |
| **Datum** | 2026-11-06 |
| **Code-Stand** | Unverändert seit Runde 1 (`jointsense.ino` 12367 Byte, `logik.cpp`/`logik.h` unverändert, gleiche Flash-/RAM-Zahlen wie in Runde 1) |
| **Gesamtergebnis** | **BESTANDEN** (mit zwei Hinweisen, siehe unten) |

**Kurzfassung in einfachen Worten:**
Jetzt laufen die Logik- und Fuzz-Tests endlich. Alle **147 von 147** bestehen, auch die zufälligen und extremen Werte und alle Python-Verläufe (Ausfall, Wackelkontakt, Zeitüberlauf, mehrere Minuten Dauer). Compiler und `cppcheck` melden weiterhin nichts. Es wurde **kein Fehler im Programm** gefunden. Die Teile, die nur auf dem Board bzw. in Wokwi prüfbar sind (Hardware-Teil), kommen aus Runde 1: Der Code ist seither nicht verändert worden, deshalb habe ich Wokwi nicht noch einmal gestartet.

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno, `--warnings all`) | ok | Exit-Code 0, keine Warnung. |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ok | 457 Byte (22 %), 1591 Byte frei. Flash 9322 Byte (28 %). Weit unter der 75-%-Grenze. |
| 2 | Code-Scanner `cppcheck` 2.22.0 | ok | `warning,style,performance,portability` auf `jointsense.ino` und `logik.cpp`: keine Funde. |
| 3 | Logik-Tests (`clang++` 22.1.8, alle Warnungen = Fehler) | ok | Teil A bis D: alle bestanden. Gesamt **147 von 147**. |
| 3b | Fuzz-Tests (Teil F, `srand(42)`) | ok | Zufällige und extreme Werte (Schwelle 0, Zähler 255, Zeitüberlauf, 20 Läufe x 20000 Zyklen): nichts stürzt ab, bei Ausfall zählt immer nur das Muster. |
| 4 | Python-Szenarien (Teil E) | ok | 2 Skripte erzeugen die Verläufe, der C++-Test wertet sie aus: `ausfall_imu1_dauer`, `ausfall_beide`, `ausfall_wackelkontakt`, `ausfall_zeitueberlauf`, `ausfall_lang_minuten`, `ausfall_unregelmaessige_takte` u. a. Längste Erkennungszeit 40 ms (bei unregelmässigen Takten 44 ms), also unter 100 ms. |
| 5 | Wokwi-Simulation inkl. Störfälle | nicht wiederholt (Ergebnis aus Runde 1 gilt) | Code unverändert, siehe «Wokwi (aus Runde 1)». |
| 6 | Bibliotheken | ok (aus Runde 1, Code unverändert) | Nur `Wire` (im Core enthalten) und `math.h`. Passt zu `docs/tech-stack.md`. |
| 7 | Sicherheitsregeln im Code | ok (aus Runde 1) | Kein `String`, kein `new`/`malloc`, Pins nur über `pins.h`, `NaN`/`inf`-Prüfung in `grenzwertPruefen`. `Wire.setWireTimeout` fehlt: gehört laut REQ-001 zu B-3, kein Fehler hier. |

## Wokwi (aus Runde 1, nicht wiederholt)

Weil sich `jointsense/` seit Runde 1 nicht geändert hat, habe ich die Simulation nicht neu gestartet. Stand aus Runde 1 (Details in `REQ-001-runde-1.md`):

- Mit einer Kopie des Programms mit Motorstärke 255: 39 von 41 Prüfungen grün, die 2 roten waren ein Verbindungsfehler von `wokwi-cli` und beim Wiederholen grün.
- Mit dem echten Programm (Motorstärke 200 von 255) schlagen einzelne Pin-Proben an, weil `expect-pin` nur einen Augenblick liest und das Signal bei 78 % PWM auch bei laufendem Motor kurz auf 0 steht. Das ist ein Problem der Messmethode, nicht des Programms (die serielle Ausgabe zeigte `-> VIBRATION` richtig).

## Akzeptanzkriterien

| Kriterium | Ergebnis | Was passiert ist (in Worten) |
|---|---|---|
| AK-1 Messung fehlgeschlagen | erfüllt | Logik: Ein Fehlschlag zählt, jede IMU hat ihren eigenen Zustand (Teil A, D). Kabel ab = keine Bestätigung wird in Wokwi erkannt. Der Fall «weniger als 6 Datenbytes» ist nicht in Wokwi nachstellbar, im Code gelesen: `neigungLesen` gibt dann `false` zurück. |
| AK-2 Ausfall nach 3 Fehlschlägen | erfüllt | 1 oder 2 Fehlschläge lösen nichts aus, der 3. in Folge schon. Erfolg setzt den Zähler auf 0. Grenzfälle (Schwelle 0 wie 1, Zähler bleibt bei 255) und alle Szenarien bestehen. Längste Erkennung 40 ms nach dem 1. Fehlschlag (QA-05: höchstens 100 ms). |
| AK-3 Fehlmessungen nie benutzt | erfüllt (Logik-Teil), Hardware-Teil durch Lesen und Wokwi | Die Logik bekommt bei Fehlschlag nie Werte. Dass der Zyklus unter der Schwelle wirklich übersprungen wird (kein Winkel, kein `grenzwertPruefen`, Motor bleibt), steht so im Code, ist auf dem PC aber nicht ausführbar. |
| AK-4 Ausfall hat Vorrang | erfüllt | Bei Ausfall zählt nur das Fehlermuster, «zu weit» wird nie beachtet (Teil C und Fuzz). Wokwi (PWM 255): In den Pausen des Musters bleibt der Motor aus, obwohl der Arm überstreckt ist. |
| AK-5 Fehler-Vibration | erfüllt | An bei 0 bis 99, 200 bis 299, 400 bis 499 ms, sonst aus, Wiederholung alle 5000 ms, Start sofort. Geprüft für jede Millisekunde, auch über den Zeitüberlauf von `millis()` hinweg (Teil B, F, Szenario `ausfall_zeitueberlauf`). Wokwi bestätigt den Takt. |
| AK-6 Meldung auf dem Seriellen Monitor | erfüllt (Wokwi) | Sofort eine Zeile, danach alle 1000 ms, mit `FEHLER` und `IMU 1 (Oberarm)` bzw. `IMU 2 (Unterarm)`. Bei beiden ausgefallen werden beide genannt. Das ist Hardware-Teil und nicht auf dem PC testbar. |
| AK-7 Messung läuft weiter, mindestens 25 Hz | nur indirekt geprüft | Eine fehlende IMU gibt sofort einen Fehler zurück und blockiert nicht (Code gelesen, Wokwi: Erkennung, Wiederkehr und Muster rechtzeitig). Die echte Schleifenrate ist in der Simulation nicht messbar. Siehe Hinweis 1. |
| AK-8 Wiederkehr | erfüllt | Logik: Erfolg einer IMU beendet nur ihren Ausfall, Muster und Motor sind sofort aus (Teil D, Szenario `ausfall_wackelkontakt`). Wokwi: genau eine Zeile `IMUs wieder da.` ohne `FEHLER`, auch mitten im Stoss. |
| AK-9 Alte Referenz, frischer Zustand | erfüllt (Wokwi und Code) | Im Code wird der Entprell-/Hysterese-Zustand beim Beginn und beim Ende des Ausfalls zurückgesetzt. Wokwi: 45 ms nach der Wiederkehr noch kein Alarm, danach kommt er mit der alten Referenz zurück. Die genauen 80 ms sind in Wokwi nicht messbar (die Entprellung selbst ist unverändert und bereits getestet). |
| AK-10 Einmessen abgebrochen | erfüllt (Wokwi) | Fällt die IMU beim Einmessen aus: Zeile `Einmessen abgebrochen`, keine Bestätigungsvibration, alte Referenz bleibt. Hardware-Teil, nur in Wokwi prüfbar. |
| AK-11 Keine Referenz | erfüllt (Wokwi) | Start ohne IMU 2: keine Warnung, `Keine Referenz` alle 1000 ms, kein automatisches Einmessen, erst die Taste misst ein. Die gleichzeitige Zeile mit `FEHLER` ist im Code vorgesehen, aber nicht einzeln in Wokwi belegt. |

## Bedrohungen (aus docs/bedrohungen.md)

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-1 Kaputte Eingaben über USB/Serial | offen (nicht Teil von REQ-001) | Der Sketch liest nichts vom Seriellen Monitor, keine neue Angriffsfläche. |
| B-2 Falsche Sensordaten | teilweise geschützt | Ausfall wird erkannt, Warnung geht aus, Fehler wird gemeldet (Logik- und Fuzz-Tests bestanden). Plausibilität und Sprünge sind nicht Teil von REQ-001 und bleiben offen. |
| B-3 Lahmlegen (I²C-Bus hängt) | nicht geschützt | `Wire.setWireTimeout` und Watchdog fehlen. Laut REQ-001 ausdrücklich ausgeklammert. |
| B-4 Unsichere Bibliotheken | geschützt | Nur `Wire` aus dem Arduino-Core. |
| B-5 Neues Programm per USB | akzeptiertes Risiko | Steht so in der Bedrohungsliste. |

## Fehler in Worten für den Developer

Keine. Es wurde kein Verstoss gegen ein Akzeptanzkriterium gefunden.

## Hinweise für den Engineer (keine Fehler)

1. **Serieller Monitor kann kurz bremsen (AK-7, geringes Risiko):** Bei 9600 Baud braucht jedes Zeichen rund 1 ms, der Sende-Puffer fasst 64 Zeichen. Fallen die langen Zeilen `FEHLER ...` und `Keine Referenz ...` in denselben Messzyklus, wartet das Programm einmal bis rund 40 ms. Selten (höchstens ein Zyklus pro Sekunde) und unter 100 ms, aber auf dem echten Board im Auge behalten (QA-01).
2. **Wokwi und Motorstärke 200:** Die Pin-Proben sind mit dem echten Programm (PWM 200) unzuverlässig. Vorschlag: Die Motorstärke für Wokwi auf 255 stellbar machen (Developer-Aufgabe) oder die Proben mehrfach kurz hintereinander lesen lassen (Tester-Aufgabe, nur Szenarien in `docs/wokwi/`). Für diesen Bericht reicht das Ergebnis aus Runde 1 mit der 255-Kopie.
3. Nicht auf echter Hardware geprüft: Hardware-Teile (AK-3 Zyklus überspringen, AK-6, AK-10, AK-11) sind nur durch Wokwi und Code-Lesen belegt. Ein kurzer Test auf dem echten Board (Kabel abziehen) durch das Team ist sinnvoll vor der Abnahme.

## Fehlende Werkzeuge

Keine. `clang++` 22.1.8 (Standard-Bibliothek vorhanden), `python3`, `cppcheck` 2.22.0, `arduino-cli` 1.5.1 und `wokwi-cli` 0.29.1 waren verfügbar.

## Dateien und Protokolle

- Logik-/Fuzz-Ausgabe: `scratch/logik-runde2.txt`, Compiler: `scratch/compile-runde2.txt`, cppcheck: `scratch/cppcheck-runde2.txt`, Szenario-CSVs: `scratch/szenarien/`.
- Kein Produkt-Code (`jointsense/`) wurde verändert. Nichts wurde hochgeladen, der serielle Port wurde nicht benutzt, nichts wurde committet.
