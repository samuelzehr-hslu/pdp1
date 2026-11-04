# Testbericht REQ-001, Runde 1
Datum: 2026-11-04   Ergebnis: **NICHT BESTANDEN (Prüfung unvollständig)**

**Kurzfassung in einfachen Worten:**
Im Programm wurde **kein Fehler gefunden**. Es kompiliert ohne Warnung, `cppcheck` meldet nichts, und in der Wokwi-Simulation verhalten sich alle Kriterien richtig, die man dort prüfen kann.
Trotzdem gilt die Runde **nicht als bestanden**: Die wichtigsten Tests (Logik- und Fuzz-Tests mit `g++`) konnten auf diesem PC **nicht laufen**, weil der C++-Compiler fehlt bzw. keine Standard-Bibliothek hat. «Nicht geprüft» ist nie «bestanden».

## Prüfkette

| Schritt | Ergebnis | Bemerkung |
|---|---|---|
| Kompilieren + RAM | bestanden | `arduino-cli compile --fqbn arduino:avr:uno --warnings all`: keine Warnung, Exit-Code 0. Flash 9322 Byte (28 %), RAM 457 Byte (22 %), 1591 Byte frei. Weit unter der 75-%-Grenze. |
| cppcheck | bestanden | `warning,style,performance,portability` auf `jointsense.ino` und `logik.cpp`: keine Funde. |
| g++ Logik- und Fuzz-Tests | **nicht geprüft** | `g++` ist nicht installiert. `clang++` (23.1.5, ARM64) ist da, findet aber keine Standard-Header (`cstdint`, `math.h`: «file not found»). Weder in `C:\Program Files\LLVM` noch in MSYS2 (`C:\msys64\clangarm64\bin` ist leer) gibt es eine C++-Bibliothek. Das Testprogramm `test_REQ-001.cpp` (Teil A bis F inkl. Fuzz) wurde deshalb nicht übersetzt und nicht gestartet. → siehe «Fehlende Werkzeuge». |
| Python-Szenarien | teilweise | Die zwei Skripte (`ausfall_verlaeufe.py`, `winkelverlauf.py`) laufen fehlerfrei und erzeugen 11 CSV-Dateien in `scratch/szenarien/`. Die **Auswertung** durch den C++-Test (Teil E) ist **nicht geprüft**, weil der Test nicht laufen kann. |
| Wokwi-Störfälle | bestanden mit Einschränkung | Siehe Abschnitt «Wokwi im Detail». Mit dem echten Motor-Wert (PWM 200 von 255) schlagen 2 Szenarien nur wegen der Messmethode an, nicht wegen des Programms. Mit Vollgas-Kopie (255) laufen alle Szenarien durch. |
| Bibliotheken | bestanden | Benutzt werden nur `Wire` (im Core enthalten) und `math.h`. Das passt zu `docs/tech-stack.md`. `arduino-cli 1.5.1` und Core `arduino:avr 1.8.8` stimmen mit den Einträgen überein (geprüft mit `arduino-cli version` und `core list`). |
| Sicherheitsregeln im Code | siehe unten | |

### Sicherheitsregeln im Code (Durchsicht)

| Regel | Ergebnis |
|---|---|
| Kein `String` | ok |
| Kein `new` / `malloc` | ok |
| Pins nur über `pins.h` | ok (im Sketch stehen keine Pin-Zahlen, nur `PIN_MOTOR_PWM`, `PIN_MOTOR_IN2`, `PIN_TASTER`, `IMU1_ADRESSE`, `IMU2_ADRESSE`; `0x6B` und `0x3B` sind Register der IMU, keine Pins) |
| `NaN`-Prüfung | ok, in `grenzwertPruefen` (`isnan`, `isinf`), unverändert |
| `Wire.setWireTimeout` | **nicht gesetzt**. Gehört laut REQ-001 zu **B-3** (eigene Anforderung), also hier kein Fehler, aber weiterhin offen. |

## Wokwi im Detail

Aufruf: `python docs/wokwi/szenarien/pruefe_REQ-001.py` (41 Prüfungen). Zwei Läufe: mit dem echten Programm (Motor-PWM 200) und mit einer **Kopie** des Programms in `scratch/variante/` (nur `MOTOR_STAERKE = 255`, sonst identisch; `jointsense/` wurde nicht verändert).

**Lauf 1, echtes Programm (PWM 200):** 7 Prüfungen rot. Ursachen:

1. **Messmethode, nicht Programm:** Der Motor-Pin wird mit PWM 200/255 (rund 78 %) angesteuert. `expect-pin` liest nur einen Augenblick. In rund 22 % der Zeit steht der Pin auch bei laufendem Motor auf 0. Betroffen: `ak04_ausfall_bei_ueberstreckung`, `ak10_einmessen_abbruch` (beide scheitern gleich an der ersten Probe «Motor an»), später auch die letzte Probe von `ak10_einmessen_bei_ausfall`. Die Serielle Ausgabe zeigt bei diesen Läufen korrekt `-> VIBRATION`. Das Wokwi-README warnt davor («bei weniger als 100 % PWM sind die Pin-Proben unzuverlässig, das ist dann zu melden»).
2. **Fehler in meinem eigenen Szenario (Tester):** In `ak10_keine_bestaetigung`, `ak10_einmessen_abbruch` und `ak10_einmessen_bei_ausfall` stand ein `wait-serial: "Einmessen abgebrochen"`. Die Zeile kommt aber schon **während** der Pausen davor, also bevor `wait-serial` zu warten beginnt. Folge: Timeout nach 45 s. Ich habe das `wait-serial` durch eine feste Pause von 300 ms ersetzt. Der Text wird weiter vom Skript im Log geprüft (Prüfung «AK-10: Zeile 'Einmessen abgebrochen'»). Die Anforderung wurde damit nicht abgeschwächt.
3. Folgefehler: Die Prüfungen «AK-6: IMU 2 wird laufend gemeldet» und «AK-8: Zeile 'wieder da' erscheint» sind nur rot, weil `ak04` vorher abgebrochen hat.

**Lauf 2, Kopie mit PWM 255, geänderte Szenarien:** 39 von 41 grün. Die zwei roten Prüfungen (`ak08_wiederkehr_mitten_im_stoss`) waren ein Verbindungsfehler von `wokwi-cli` («Emitted 'error' event on WebSocket»), kein Ergebnis des Programms. Zweimal einzeln wiederholt: Szenario komplett bestanden, `IMUs wieder da.` erscheint ohne das Wort FEHLER.

**Kontrolle mit echtem Programm nach der Szenario-Korrektur:** `ak10_keine_bestaetigung` bestanden. `ak10_einmessen_bei_ausfall` scheitert nur an der letzten Pin-Probe (Ursache 1).

Ergebnisse, die schon mit dem echten Programm (PWM 200) grün waren: Normalbetrieb ohne Störung, `ak02_kurze_aussetzer`, `ak02_erkennung_zeit`, `ak06_meldungen_takt`, `ak06_beide_ausfall`, `ak08_wiederkehr_mitten_im_stoss`, `ak10_keine_bestaetigung`, `ak11_ohne_referenz`.

Logs: `scratch/wokwi-req001-runde1.log` (Lauf 1), `scratch/wokwi-req001-voll.log` (Lauf 2), `scratch/wokwi-r1/` (Einzelläufe).

## Akzeptanzkriterien

«Nur Wokwi» heisst: Der zugehörige Logik-Test (`g++`) konnte nicht laufen. Das Kriterium ist in der Simulation und durch Lesen des Codes in Ordnung, aber die genaueren Logik- und Fuzz-Tests fehlen noch.

| Kriterium | Ergebnis | Was passiert ist (in Worten) |
|---|---|---|
| AK-1 Messung fehlgeschlagen | teilweise geprüft | Kabel ab = keine Bestätigung: Gerät erkennt den Fehlschlag, jede IMU einzeln (Wokwi). Der Fall «weniger als 6 Datenbytes» ist in Wokwi nicht nachstellbar. Im Code geprüft durch Lesen: `neigungLesen` gibt dann `false` zurück. Logik-Test A nicht gelaufen. |
| AK-2 Ausfall nach 3 Fehlschlägen | teilweise geprüft (Wokwi bestanden) | 30 ms Aussetzer (1 bis 2 Messungen): kein Ausfall, kein `FEHLER`. Kabelbruch: nach 25 ms noch kein Ausfall, nach 105 ms läuft der erste Stoss (≤ 100 ms laut QA-05, im Rahmen der Messgenauigkeit). Grenzfälle (Schwelle 0, Zähler 255, Fuzz) nicht gelaufen. Per Code gelesen: `imuStatusAktualisieren` entspricht der Tabelle in der Anforderung. |
| AK-3 Fehlmessungen nie benutzt | teilweise geprüft | Bei Ausfall folgt der Motor nur dem Muster, nach Wiederkehr gilt die alte Referenz, und Einmessen bei fehlender IMU wird abgebrochen (Wokwi mit PWM 255). Im Code: Der Zyklus mit einem Fehlschlag unter der Schwelle wird übersprungen (kein Winkel, kein `grenzwertPruefen`, Motor bleibt). Direkt in Wokwi nicht beweisbar. |
| AK-4 Ausfall hat Vorrang | teilweise geprüft (Wokwi bestanden mit PWM 255) | Arm überstreckt, Motor an, dann Kabelbruch: Warnung geht aus, in den Pausen des Musters bleibt der Motor aus trotz überstrecktem Arm. Mit PWM 200 war die Probe wegen der Messmethode rot (Ursache 1). Logik-Test C nicht gelaufen. |
| AK-5 Fehler-Vibration | teilweise geprüft (Wokwi bestanden mit PWM 255) | 3 Stösse (an/aus/an/aus/an), danach aus, Wiederholung nach 5000 ms von vorn, Start sofort bei Beginn. Zeitüberlauf von `millis()` und jede einzelne Millisekunde nur im Logik-Test B, der nicht lief. Per Code gelesen: `(jetztMs - startMs) % 5000` mit `uint32_t`, wie in der Anforderung. |
| AK-6 Meldung auf dem Seriellen Monitor | **bestanden** (Wokwi) | Sofort eine Zeile, danach alle 1000 ms (acht Zeitpunkte gemessen, immer die richtige Zeilenzahl). Jede Zeile enthält `FEHLER` und `IMU 1 (Oberarm)` bzw. `IMU 2 (Unterarm)`. Bei zwei ausgefallenen IMUs werden beide genannt. Eine zurückgekehrte IMU wird nicht mehr gemeldet. |
| AK-7 Messung läuft weiter, ≥ 25 Hz | nur indirekt geprüft | Erkennung ≤ 100 ms, Wiederkehr wird schnell bemerkt, Muster auf ± 50 ms genau. Die echte Schleifenrate ist in der Simulation nicht messbar. Eine fehlende IMU gibt sofort einen Fehler zurück und blockiert nicht. Hinweis zum Seriellen Monitor siehe «Beobachtungen». |
| AK-8 Wiederkehr | teilweise geprüft (Wokwi bestanden) | Eine zurückkehrende IMU allein beendet den Ausfall nicht (die zweite fehlt noch). Erst wenn beide da sind: Motor sofort aus, genau eine Zeile `IMUs wieder da.` ohne das Wort `FEHLER`. Mitten im Stoss geprüft. Logik-Tests (Wackelkontakt, selber Zyklus) nicht gelaufen. |
| AK-9 Alte Referenz, frischer Zustand | teilweise geprüft (Wokwi bestanden mit PWM 255) | 45 ms nach der Wiederkehr noch kein Alarm, 445 ms danach kommt der Alarm mit der alten Referenz zurück. Im Code wird `grenzZustand` beim Beginn **und** beim Ende des Ausfalls zurückgesetzt. Die genauen 80 ms sind in Wokwi nicht prüfbar. |
| AK-10 Einmessen abgebrochen | **bestanden** (Wokwi) | Fällt die IMU beim Einmessen kurz aus oder fehlt sie schon: Zeile `Einmessen abgebrochen`, keine Bestätigungsvibration, alte Referenz bleibt (der überstreckte Arm löst weiter Alarm aus). Alle drei Szenarien mit PWM 255 grün. `ak10_keine_bestaetigung` auch mit dem echten Programm grün. |
| AK-11 Keine Referenz | **bestanden** (Wokwi) | Start ohne IMU 2: keine Warnung, `Keine Referenz` genau 5 Mal in 5 s, auch nachdem IMU 2 wieder da ist, und 0 Mal nach dem Einmessen per Taste. Es wird nicht automatisch neu eingemessen. Mit dem echten Programm (PWM 200) bestätigt. Die gleichzeitige Zeile mit `FEHLER` ist im Code vorgesehen (Schritt 5 im `loop`). |

## Bedrohungen (aus docs/bedrohungen.md)

| Bedrohung | geschützt / nicht geschützt / akzeptiertes Risiko | Begründung |
|---|---|---|
| B-1 Kaputte Eingaben über USB/Serial | offen (nicht Teil von REQ-001) | Der Sketch liest nichts vom Seriellen Monitor, also keine neue Angriffsfläche. |
| B-2 Falsche Sensordaten | **teilweise geschützt** | Ausfall wird erkannt (Wokwi), Überstreckungs-Warnung geht aus, Fehler wird gemeldet, Müllwerte werden nicht benutzt. Fuzz-Tests der neuen Funktionen konnten nicht laufen. Plausibilität und Sprünge bleiben offen (nicht Teil von REQ-001). |
| B-3 Lahmlegen (I²C-Bus hängt) | **nicht geschützt** | `Wire.setWireTimeout` fehlt, kein Watchdog. Ist laut REQ-001 ausgeklammert. RAM-Belegung ist mit 22 % unkritisch. |
| B-4 Unsichere Bibliotheken | geschützt | Nur `Wire` aus dem Arduino-Core. |
| B-5 Neues Programm per USB | akzeptiertes Risiko | Steht so in der Bedrohungsliste. |

## Beobachtungen für den Engineer (keine Fehler)

- **Serieller Monitor kann kurz bremsen (AK-7, geringes Risiko):** Bei 9600 Baud braucht jedes Zeichen rund 1 ms, und der Sende-Puffer fasst 64 Zeichen. Fallen die Zeilen `FEHLER ...` (bis rund 55 Zeichen) und `Keine Referenz ...` (rund 50 Zeichen) in denselben Messzyklus, wartet das Programm einmal rund 40 ms. Das ist selten (höchstens ein Zyklus pro Sekunde) und unter der Grenze von 100 ms für die Erkennung, aber wegen QA-01 beim echten Board im Auge behalten.
- **Motorstärke 200 (78 %) macht Wokwi-Pin-Proben unzuverlässig.** Vorschlag: Für Wokwi eine eigene Firmware-Variante mit 255 bauen lassen (zum Beispiel über eine Einstellung in `jointsense.ino`, die der Developer anbietet) oder die Pin-Proben so umbauen, dass sie mehrmals kurz hintereinander lesen. Bis dahin ist für Wokwi die Vollgas-Kopie in `scratch/variante/` nötig.
- `docs/tech-stack.md` hat bei den Test-Werkzeugen noch `TODO` (g++/clang++, cppcheck, wokwi-cli). Installiert sind `clang++` 23.1.5, `wokwi-cli` 0.29.1 und `cppcheck`; die Versionen könnten eingetragen werden (Aufgabe des Developers).

## Fehlende Werkzeuge

- **C++-Compiler mit Standard-Bibliothek fehlt** (`g++` nicht vorhanden; `clang++` findet `cstdint` und `math.h` nicht). Dadurch sind **Logik- und Fuzz-Tests** und die **Auswertung der Python-Verläufe** «nicht geprüft». → Bitte `/einrichten` ausführen und die Anleitung für Windows ARM befolgen (zum Beispiel `clang++` aus MSYS2 `clangarm64` samt Standard-Bibliothek). Ich habe nichts selbst installiert.
- Danach genügt: `python docs/tests/alle_tests.py`. Dann kann Runde 2 (oder eine Wiederholung von Runde 1) die offenen Punkte schliessen.

## Geänderte Dateien dieser Runde

- `docs/wokwi/szenarien/ak10_einmessen_abbruch.yaml`, `ak10_einmessen_bei_ausfall.yaml`, `ak10_keine_bestaetigung.yaml`: `wait-serial "Einmessen abgebrochen"` durch Pause ersetzt (Fehler im Szenario, siehe oben).
- Kein Produkt-Code (`jointsense/`) wurde verändert. Nichts wurde hochgeladen, der serielle Port wurde nicht benutzt, nichts wurde committet.
