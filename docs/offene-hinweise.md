# Offene Hinweise

Hier sammelt der Engineer alles aus den Testberichten, was keine Fehler sind, aber jemand noch erledigen sollte. So geht nichts verloren.
Vor einer neuen Anforderung lesen. Erledigte Zeilen löschen (die Geschichte steht in Git).

| Hinweis | Quelle | Wer |
|---|---|---|
| **Wokwi hat doch ein Gyroskop.** Der Test zu REQ-002 setzte `gyroX`/`gyroY`, richtig sind `rotationX/Y/Z`. Gegenprobe am 2026-11-06: `rotationX = 60` ergibt den Rohwert 3930, also 60 °/s bei ±500 °/s. Neu prüfen: «W3, W5 und Übersteuerung nicht prüfbar» (`REQ-002-runde-1.md`) und die REQ-001-Szenarien, die dadurch «nicht prüfbar» wurden (`ak04`, `ak08`, `ak10_einmessen_abbruch`, `ak10_einmessen_bei_ausfall`, Schluss von `ak11`). | Analyse 2026-11-06 | Engineer, Tester |
| `docs/tech-stack.md` enthält noch `TODO` bei den Versionen der Werkzeuge. | `manuell-2026-11-04.md`, REQ-001 Runde 1 | Developer |
| Wokwi-Pin-Proben sind mit Motorstärke 200 von 255 unzuverlässig. Entweder die Stärke für Wokwi umstellbar machen oder die Proben mehrmals lesen lassen. | REQ-001 Runde 1 und 2 | Team entscheidet |
| `docs/tests/szenarien/winkelverlauf.py` erzeugt Verläufe, die kein Test liest. Die Kernlogik (Hysterese, Entprellung) hat dadurch keinen Test mit diesen Verläufen. Eigene Anforderung dafür, oder Skript löschen. | Analyse 2026-11-06 | Team entscheidet |
| Lange Zeilen auf dem Seriellen Monitor können einen Messzyklus um rund 40 ms bremsen (AK-7 von REQ-001). Am echten Board beobachten. | REQ-001 Runde 1 und 2 | Team |
| REQ-001 ist noch nicht abgenommen, der empfohlene Test am echten Board (Kabel abziehen) steht aus. Der Runden-Zähler steht auf 0, die Berichte heissen aber Runde 1 und 2 (ältere Zählweise). | REQ-001 Runde 2 | Team, Engineer |
| Wokwi-Szenarien W1 bis W6 zu REQ-002 und die Vorspänne der REQ-001-Einmess-Szenarien (`erzeuge_einmess_szenarien.py`, `pruefe_REQ-002.py`) gehen noch vom alten Ablauf aus (6000 ms, ohne Phase 3 «Achse messen»). Erst umbauen, wenn Wokwi wieder verfügbar ist, sonst schlagen sie zu Unrecht fehl. Bis dahin «nicht geprüft». | Tester-Bericht REQ-002 Neustart (2026-11-06) | Tester, sobald Wokwi verfügbar |
| `Wire.setWireTimeout` kommt im Code nicht vor (B-3, eigene Anforderung). Die Sicherheitsregel verlangt es, bis die Anforderung dafür steht, bleibt es offen. | Tester-Bericht REQ-002 Runde 1 (Neustart) | Engineer, Team |
| Flash jetzt 89 % (28806 von 32256 Byte), RAM 56 % (1155 Byte). In `docs/tech-stack.md` eintragen. | Tester-Bericht REQ-002 Runde 1 (`REQ-002-runde-1-pruefung.md`) | Developer |
| Cppcheck-Stilhinweis: in `logik.cpp` Zeile 1080 könnte `pf` `const` sein. Kein Fehler. | Tester-Bericht REQ-002 Runde 1 (`REQ-002-runde-1-pruefung.md`) | Developer |
| REQ-002: Reserve zur 2,5°-Grenze bei AK-4, AK-10, AK-11 und AK-16 ist klein (0,3 bis 0,5°). Echte Sensoren können schlechter sein als die Simulation. Am Board beobachten. | Tester-Bericht REQ-002 Runde 1 (`REQ-002-runde-1-pruefung.md`) | Team |
| REQ-002: Ohne Wokwi nicht geprüft: AK-9 (`Keine Referenz`), AK-14 (Übersteuerung), AK-15 (Zykluszeit), AK-6-Zeiten und Phase 3, Abbruch-Zeilen im Seriellen Monitor. Am echten Board testen. | Team-Entscheidung, Tester-Bericht REQ-002 Runde 1 | Team |
| Die Tests `test_REQ-002_hand.cpp` und `test_REQ-002_schief.cpp` sind noch nicht in Git eingecheckt. | Tester-Bericht REQ-002 Runde 1 | Engineer |
| **REQ-002 Board-Test vor der Abnahme:** Einmessen mit einem Oberarm-Kreis (AK-6) bei echtem Arm, Zeitablauf 7500 ms, Serielle Meldungen (`Grund: …`, `Keine Referenz`, `Zyklus max`), Zykluszeit ≤ 15 ms und Rechenzeit von `einmessenAbschliessen`, Gyro-Übersteuerung. Winkelfehler nach dem Einmessen: Reserve bei 2,5° ist klein (bis 2,15° in der Simulation). | Tester-Bericht REQ-002 Runde 1 (Oberarm-Kreis) | Team (am Board) |
| REQ-002: Schwelle `ACHSE_LAMBDA_MIN = 3000` (`logik.cpp`) ist vom Developer gesetzt, nicht vom Team bestätigt. | Engineer, Probe 2026-11-06 | Team |
| Die Sperre für den Developer (`developer_blind.py`) erkennt `cd docs && cat tests/...` nicht. Sie ist ein Schutz gegen Versehen, kein Tresor. | Analyse 2026-11-06 | Team entscheidet |
