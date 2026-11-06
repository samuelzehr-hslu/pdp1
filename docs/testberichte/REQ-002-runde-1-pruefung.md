# Testbericht REQ-002, Runde 1 (Neustart, zweite Prüfung mit Werkzeugen)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-002](../anforderungen/REQ-002.md) |
| **Runde** | 1 von 3 (Neustart nach «Achse messen statt annehmen») |
| **Datum** | 2026-11-06 |
| **Code-Stand** | nicht committet. Seit dem Neustart-Bericht (`REQ-002-runde-1-neustart.md`) hat sich in `jointsense/` nichts geändert (Dateizeiten älter als der Neustart-Bericht). Gegenüber dem letzten Commit geändert: `jointsense.ino` (Phase 3, +35/−9 Zeilen) und `logik.h` (nur ein Kommentar). `logik.cpp` und `pins.h` sind unverändert. |
| **Gesamtergebnis** | **nicht bestanden** (ein Logik-Test schlägt fehl, Wokwi nicht geprüft, Hardware-Teile nur am Board prüfbar) |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`, Uno, `--warnings all`) | ok | Build fehlerfrei. Es erscheinen 4 Warnungen, alle aus dem Arduino-Core (`new.cpp`, «unused parameter 'tag'»), keine aus `jointsense/`. Eigene Warnungen: 0. |
| 1b | Speicher | ok, Flash knapp | **Flash: 28806 von 32256 Byte (89 %)**, 3450 Byte frei (vorher 98 %, 374 frei). **RAM: 1155 Byte (56 %)**, Grenze AK-15 ist 1434 Byte (70 %): eingehalten, 893 Byte frei. Flash weiter ein Risiko, Platz ist aber deutlich besser als vorher. |
| 2 | `cppcheck` | ok (1 Hinweis) | Kein «error» oder «warning». Ein Stil-Hinweis: `logik.cpp` Zeile 1080, Feld `pf` könnte `const` sein. Kein Fehler. |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`, g++ aus MSYS2) | **Fehler** | 6 Testdateien, 1 fehlgeschlagen (`test_REQ-002_oberarm`, 2 Prüfungen). Ergebnis: REQ-001 147/147, REQ-002 253/253, Drift 28/28, Hand 35/35, Oberarm 29/31, schiefes Einmessen 38/38. |
| 3b | Fuzz-Tests | ok | Fuzz-Teile in den Testdateien liefen und bestanden (u. a. REQ-001 Teil F, `srand(42)`). |
| 4 | Wokwi-Simulation | nicht geprüft (Team-Entscheidung) | Kein Gratis-Kontingent, Token wird nicht eingerichtet. Kein Wokwi-Befehl ausgeführt. |
| 5 | Bibliotheken | ok | Nur `Wire.h` und `math.h` (Arduino-Core), wie in `docs/tech-stack.md`. |
| 6 | Sicherheitsregeln | teilweise | Kein `String`, kein `new`/`malloc`, Pins über `pins.h`, `NaN`/unendlich-Prüfung in `logik.cpp` vorhanden. **`Wire.setWireTimeout` fehlt** (B-3, bekannt, laut REQ-002 «Nicht dazugehörig»). |

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 bis AK-3 (Drehung ändert Winkel nicht) | erfüllt | Grösste Winkeländerung 0,42° (Grenze 2°). |
| AK-4 (Lagen, Einbaudrehung) | erfüllt | Lagen: 0,29° Fehler (Grenze 2°). Einbaudrehung: **2,06°** (Grenze 2,5°), also nur wenig Reserve. |
| AK-5 (Messbereich, Auflösung, Erkennung) | erfüllt | Tests bestanden. |
| AK-6 (Ablauf, Einmessen mit Bewegung) | **nicht erfüllt** | Logik: «2 Mal beugen und der Oberarm macht 2 Umläufe kleiner Kreise (15°)» bricht bei allen 5 Einbaudrehungen ab, mit und ohne alte Referenz. Die Varianten mit 1 Umlauf und 20° klappen. Zeiten, Stösse, Phase 3 (Hardware): nicht geprüft (nur Code gelesen: Phase 3 ab 6000 ms, Ende 7500 ms). |
| AK-7 (Abbruch a, b, c, d, e; Meldungszeilen) | erfüllt (PC-Teil) | Hand nicht gedreht, falsche Bewegung (beugen statt drehen), Oberarm still, nur 3°, nur um die Senkrechte: alle brechen sauber ab, nie `true` nach Abbruch, alte Referenz bleibt. Meldungszeilen am Board nicht geprüft. |
| AK-8 (Wiederholgenauigkeit) | erfüllt | Teil der bestandenen Tests (Fehler bei geklappten Einmessungen max. 2,15°). |
| AK-9 (Einschalten, «Keine Referenz» alle 1000 ms) | nicht geprüft | Hardware-Teil. Im Code vorhanden (gelesen). |
| AK-10, AK-11 (Drift) | erfüllt | Grösster Fehler 1,99° (mit Ruhe), 1,96° (ohne Ruhe), Grenze 2,5°. |
| AK-12 (Referenz bleibt sicher) | erfüllt | Tests bestanden. |
| AK-13 (nach Ausfall) | erfüllt (PC-Teil) | Tests bestanden. Ablauf am Board nicht geprüft. |
| AK-14 (ungültige Werte) | erfüllt (PC-Teil) | `NaN`, unendlich, `dt` und Grenzwerte in den Tests bestanden. **Übersteuerung (Rohwert +32767/−32768): nicht geprüft** (nur Wokwi oder Board). |
| AK-15 (Tempo, Speicher) | teilweise | RAM ok (56 %). Zyklusdauer ≤ 15 ms: **nicht geprüft** (nur am Board). Zeile `Zyklus max:` im Code vorhanden (gelesen). |
| AK-16 (schiefes Einmessen) | erfüllt | 0 bis 10° in 8 Richtungen, Referenz 0° und −20°, 5 Einbaulagen: nie ein falscher Winkel ohne Abbruch, Einmessen klappt bis 8° in allen Richtungen. Grösster Fehler 2,15° (Grenze 2,5°). |

## Bedrohungen

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-2 (falsche Sensordaten) | geschützt (PC-Teil) | Fuzz-Tests mit `NaN`, unendlich, extremen Werten bestanden. Übersteuerung nicht geprüft. |
| B-3 (Lahmlegen) | teilweise | RAM ok. Zykluszeit nicht gemessen. `Wire.setWireTimeout` fehlt (anderes REQ). |
| B-6 (Selbstkorrektur/Einmessen verfälscht Messung) | geschützt (PC-Teil) | AK-7, AK-12, AK-16 bestanden: kein falscher Winkel ohne Abbruch. AK-6 (2 Umläufe) bricht ab statt falsch zu messen, das ist sicher, aber nicht gewünscht. |

## Fehler in Worten für den Developer

- **AK-6 (Einmessen mit Oberarm-Kreisen):** Beugt der Unterarm genau 2 Mal um mindestens 30° und macht der Oberarm dabei **2 volle Umläufe** kleiner Kreise (Spannweite 15°), bricht das Einmessen bei jeder Einbaudrehung der IMUs ab. Die Anforderung verlangt, dass es klappt. Mit nur 1 Umlauf, oder mit grösseren Kreisen (20°), klappt es. Das Gerät rät nicht (es bricht ab), das ist sicher, aber falsch bedient. Vermutete Ursache: Code (die Achsen-Erkennung der Oberarm-IMU verträgt mehrere Umläufe bei kleinen Kreisen nicht). Der Test legt den Abbruchgrund in diesem Fall nicht offen, darum bleibt es «Code, nicht ganz sicher».
- Mehr nicht. AK-1 bis AK-5, AK-7 (PC-Teil), AK-8, AK-10 bis AK-14 (PC-Teil) und AK-16 bestehen. Knappe Reserven (Fehler nahe 2,5°): AK-4 Einbaudrehung 2,06°, AK-10 1,99°, AK-16 2,15°.

## Am echten Board prüfen

- Zykluszeit (≤ 15 ms) und Zeile `Zyklus max:` alle 1000 ms.
- Gyro-Übersteuerung (AK-14): schnelles Drehen darf keinen IMU-Fehleralarm geben, der Zyklus wird übersprungen.
- Ablauf des Einmessens (AK-6): 2000 ms still, Stoss, 4000 ms Bewegung, zwei Stösse, 1500 ms Drehung, Bestätigung nach 7500 ms ± 100 ms.
- Abbruch-Zeilen (`Einmessen abgebrochen - Grund: ...`) und `Keine Referenz` nach dem Einschalten (AK-9).
- Reale Bedienung: Einmessen im Stehen mit hängendem Arm, 2 Mal beugen mit Oberarm-Kreisen, Hand drehen.
- Ob alle Wokwi-Szenarien (W1 bis W6, REQ-001-Störfälle) später am Board nachgestellt werden: bisher nirgends geprüft.

## Hinweise für den Engineer

- `Wire.setWireTimeout` fehlt im Code (B-3, schon bekannt) → nach `docs/offene-hinweise.md`.
- Flash jetzt 89 % (3450 Byte frei), deutlich besser als 98 %. Bitte `docs/tech-stack.md` (Flash/RAM als `TODO`) mit diesen Zahlen nachtragen.
- Wokwi (Schritt 4) wegen Team-Entscheidung nicht ausgeführt: Übersteuerung (AK-14), AK-9, AK-6-Zeiten und REQ-001-Störfälle sind damit **ungeprüft**.
- Reserve zur 2,5°-Grenze ist bei AK-4 (Einbaudrehung), AK-10, AK-11 und AK-16 klein (0,3 bis 0,5°). Echte Sensoren können schlechter sein als die Simulation.
- Test `test_REQ-002_oberarm`: 2 Prüfungen schlagen fehl (siehe AK-6). Ich habe keine Tests geändert und keine Korrekturen gemacht.
- Neue Tests `test_REQ-002_hand.cpp` und `test_REQ-002_schief.cpp` sind noch nicht committet (`git status`).

## Fehlende Werkzeuge

- `wokwi-cli`: nicht geprüft, Team-Entscheidung (kein Kontingent, kein Token).
- Alle anderen Werkzeuge vorhanden: `arduino-cli`, `g++` (MSYS2 `ucrt64`, im PATH ergänzt), `cppcheck`, Python 3.11.
