# REQ-001: Welcher Test deckt welches Kriterium ab?

Anforderung: `docs/anforderungen/REQ-001.md` (Sensorausfall erkennen und sicher melden).
Die Tests sind **vor** dem Programmieren nur aus der Anforderung entstanden.

## Dateien

| Datei | Art | Start |
|---|---|---|
| `docs/tests/logik/test_REQ-001.cpp` | Logik- und Fuzz-Tests in C++ (`g++` oder `clang++`) | `python3 docs/tests/alle_tests.py` |
| `docs/tests/szenarien/ausfall_verlaeufe.py` | erzeugt Messverläufe (CSV) für den C++-Test, baut die Logik **nicht** nach | wird von `alle_tests.py` automatisch gestartet |
| `docs/wokwi/szenarien/*.yaml` + `diagram_kabelbruch.json` | Wokwi-Szenarien fürs ganze Gerät | `python3 docs/wokwi/szenarien/pruefe_REQ-001.py` |

`alle_tests.py` wurde angepasst: Es startet jetzt zuerst die Python-Szenarien und lässt die Tests im Hauptordner laufen
(damit sie `scratch/szenarien/*.csv` finden).

## Abdeckung pro Akzeptanzkriterium

| AK | Was wird geprüft | Logik-Test (C++) | Wokwi |
|---|---|---|---|
| AK-1 | Jede IMU einzeln beurteilt; Fehlschlag = keine Bestätigung | Teil A (zwei getrennte Zustände) | `ak02_*`, `ak06_beide_ausfall` (Kabel ab = keine Bestätigung). **«weniger als 6 Bytes» nicht prüfbar** (Begründung im Wokwi-README) |
| AK-2 | 1 und 2 Fehlschläge = kein Ausfall, 3 in Folge = Ausfall, Erfolg setzt auf 0; Erkennung höchstens 100 ms; Schwelle 0 wie 1; Zähler 255 ohne Überlauf | Teil A, Teil D, Teil E (alle Szenarien), Teil F (Fuzz) | `ak02_kurze_aussetzer`, `ak02_erkennung_zeit` |
| AK-3 | Werte aus Fehlmessungen nie benutzt | – (Hardware-Teil) | indirekt: `ak04_*` (Alarm nur mit alter Referenz), `ak10_einmessen_bei_ausfall` |
| AK-4 | Ausfall hat Vorrang vor Überstreckung, auch wenn der Motor gerade vibrierte | Teil C (alle 8 Kombinationen), Teil D, Teil E, Teil F | `ak04_ausfall_bei_ueberstreckung` (Pausen im Muster trotz überstrecktem Arm) |
| AK-5 | Muster 100 an / 100 aus / 100 an / 100 aus / 100 an, dann aus; sofort bei Beginn; alle 5000 ms von vorn | Teil B (jede Millisekunde, 3 Perioden, Grenzen 99/100/199/200/…), Teil D, Teil E, Teil F | `ak04_*`, `ak02_erkennung_zeit`, `ak08_*` |
| AK-5 (Zeitüberlauf) | Muster stimmt über den Überlauf von `millis()` hinweg | Teil B (12 Startzeiten kurz vor 2^32), Teil E (`ausfall_zeitueberlauf`, `ausfall_lang_minuten`, `ausfall_unregelmaessige_takte`), Teil F | – |
| AK-6 | Serielle Zeile sofort, dann alle 1000 ms ± 100 ms; `FEHLER` + Name der IMU; beide, wenn beide ausfallen | – (Hardware-Teil) | `ak06_meldungen_takt` (Zeilen zählen), `ak06_beide_ausfall`, `ak04_*` |
| AK-7 | Messung läuft weiter, ≥ 25 Hz, Schleife blockiert nicht | – | indirekt (Erkennung ≤ 100 ms, Wiederkehr nach 45 ms, Muster ± 50 ms): `ak02_erkennung_zeit`, `ak08_*`. **Echte Schleifenrate nicht prüfbar** |
| AK-8 | Beide wieder da: Motor sofort aus, Zeile `wieder da` ohne `FEHLER`; eine IMU allein beendet den Ausfall nicht | Teil A, Teil B (Reset des Musters), Teil D (Wiederkehr im selben Zyklus, mitten im Stoss, Wackelkontakt), Teil E, Teil F | `ak04_*`, `ak06_beide_ausfall`, `ak08_*` |
| AK-9 | Alte Referenz bleibt; Entprellung/Hysterese frisch, kein Sofort-Alarm, Alarm frühestens nach 80 ms | – (Zustand von Entprellung liegt in den bestehenden Funktionen, Signaturen stehen nicht in der REQ) | `ak04_ausfall_bei_ueberstreckung` (45 ms nach Wiederkehr kein Alarm, später Alarm mit alter Referenz). **Genau 80 ms nicht prüfbar**, siehe Wokwi-README |
| AK-10 | Fehlschlag beim Einmessen: abbrechen, Referenz unverändert, keine Bestätigungsvibration, Zeile `Einmessen abgebrochen` | – | `ak10_einmessen_abbruch`, `ak10_keine_bestaetigung`, `ak10_einmessen_bei_ausfall` |
| AK-11 | Ohne erfolgreiches Einmessen: keine Warnung, `Keine Referenz` alle 1000 ms ± 100 ms, Einmessen nur per Taste | – | `ak11_ohne_referenz` |

## Fuzz-Tests (Bedrohung B-2, fail-safe)

In `test_REQ-001.cpp`, Teil F, mit `srand(42)`:

- `imuStatusAktualisieren`: 300 000 Aufrufe mit zufälligem Zustand (Zähler 0 bis 255), zufälliger Schwelle (auch 0 und 255).
- `fehlerMusterMotorAn`: 300 000 Aufrufe mit zufälligem Zustand und extremen Zeiten (0, 1, 4294967295, 4294967294, 2^31 …).
- `motorSollAn`: 100 000 Aufrufe: bei Ausfall ohne Fehlermuster ist der Motor **nie** an (fail-safe).
- Ganze Kette (Erkennung + Muster + Motor) mit Zufallsfolgen, 20 Läufe × 20 000 Messzyklen, Start auch kurz vor dem Zeitüberlauf, meistens «Arm zu weit» (schlimmster Fall).
- `NaN`/`INFINITY`: Die drei neuen Funktionen bekommen keine Kommazahlen. Das gehört weiter zu `grenzwertPruefen` (bestehend, nicht Teil von REQ-001).

## Bedrohungen

| Bedrohung | Wo geprüft |
|---|---|
| B-2 (falsche Sensordaten: Ausfall erkennen und melden, Motor-Warnung aus) | alle Tests oben; Rest von B-2 (Plausibilität, Sprünge) ist **nicht** Teil von REQ-001 |
| B-3 (hängender I²C-Bus) | nicht Teil von REQ-001 |
