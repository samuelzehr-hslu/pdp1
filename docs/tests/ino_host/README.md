# REQ-007: Prüfen, dass das Aufräumen nichts verändert

Der Developer darf diesen Ordner nicht lesen (wie alles unter `docs/tests/`).

Die Ausgangslage (Commit `cb7d6a4`, **vor** dem Aufräumen) liegt in `scratch/REQ-007-ausgangslage/` (nicht im Repo, bei Bedarf neu erzeugen mit `git stash`/`git checkout cb7d6a4 -- jointsense tools` in einer Kopie und den Befehlen unten).
Python-Name: `python` (Mac/Linux `python3`).

| Was | Befehl (aus dem Hauptordner) | Prüft |
|---|---|---|
| Alle Logik-/Fuzz-/Szenario-Tests | `python docs/tests/alle_tests.py` | AK-1 (heute 723 Tests, 10 Dateien, alle bestanden; Ausgabe zum Vergleichen: `scratch/REQ-007-ausgangslage/alle_tests.txt`) |
| Firmware-Ausgabe Zeile für Zeile | `python docs/tests/ino_host/ino_vergleich.py pruefen` | AK-3 (beide Modi, 3 feste Szenarien: Serielle Zeilen mit Zeit, Motor-PWM, Baudrate; I2C-Zugriffe nur als Hinweis) |
| Mechanische Punkte | `python docs/tests/refactoring_pruefen.py` | AK-2, AK-4, AK-5, AK-7, AK-9 |
| GUI mit denselben Zeilen | `node docs/tests/gui_host/gui_vergleich.js tools/gui/jointsense_live.html scratch/REQ-007-nachher/gui_host` und dann `diff -r scratch/REQ-007-ausgangslage/gui_host scratch/REQ-007-nachher/gui_host` | AK-8 (ohne Browser: Attrappen-Browser, Texte, Farben, Klassen, Zeichenbefehle als Prüfsumme je Bild, CSV-Aufnahme, Demo-Modus) |
| GUI statisch | `python docs/tests/gui_host/gui_statisch.py` | AK-8: keine neuen Internet-Adressen/Bibliotheken, gleiche Element-Namen und Farben |

## So funktioniert `ino_vergleich.py`

Die echte `jointsense.ino` wird mit einer Arduino-Attrappe (`arduino_attrappe.h/.cpp`: `Serial`, `Wire`, `millis`, Pins) auf dem PC übersetzt (Kopie unter `scratch/`, `GUI_MODUS` wird nur dort umgestellt) und gegen festes Szenario gespielt (`ino_lauf.cpp`, Arm-Bewegung aus `sensor_tabelle.cpp` mit dem Simulator der Logik-Tests). Zeit ist simuliert, jede Ausgabe bekommt ihre Zeit. Darum stimmt der Vergleich Zeile für Zeile und ist wiederholbar (zweimal laufen lassen = gleiche Dateien, geprüft).

Szenarien: `normal` (Unsinn über Serial, Enter/Leertaste/Taste mit Prellen, Einmessen bis «Referenz gesetzt», Überstreckung, Zittern an der Grenze, kurze Aussetzer, IMU-2-Ausfall 7 s mit Wiederkehr, Gyro am Anschlag, zweites Einmessen ohne Bewegung = Abbruch Beugung), `abbruch` (Abbruch ruhig, Bestätigung, Beugung, IMU), `ohne_imu2` (IMU 2 fehlt beim Start, Einmessen mit fehlender IMU).

**Grenzen:** Rechenzeit wird nicht gemessen (AK-6 geht nur in Wokwi oder am Board), Flash/RAM auch nicht (AK-5: `refactoring_pruefen.py`). Wird die Attrappe nach dem Aufräumen für neue Arduino-Befehle gebraucht, fehlt dort eine Funktion: das ist ein Test-Fehler (nicht Code-Fehler) und wird in `arduino_attrappe.h/.cpp` ergänzt.
Prototypen: Das Skript erzeugt wie Arduino die Funktions-Vorwärtsdeklarationen selbst. Es liest alle `.ino` im Ordner (`jointsense.ino` zuerst) und übersetzt alle `.cpp` daneben mit.
