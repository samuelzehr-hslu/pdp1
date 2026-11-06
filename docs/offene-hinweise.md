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
| Die Sperre für den Developer (`developer_blind.py`) erkennt `cd docs && cat tests/...` nicht. Sie ist ein Schutz gegen Versehen, kein Tresor. | Analyse 2026-11-06 | Team entscheidet |
