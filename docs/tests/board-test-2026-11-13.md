# Board-Test und Entscheide (13.11.2026)

Checkliste für morgen. Pro Punkt ein Satz. Abhaken, und neben jedem Test **ok** oder **nicht ok** notieren (mit einer kurzen Beobachtung).

**Vorher:** Den anderen im Team sagen, dass ihr das Board braucht. Das Hochladen macht ein Mensch selbst (Arduino IDE oder `arduino-cli upload`), Claude lädt nicht hoch.

**Aufbau:** LED mit 220 Ω an D9 (langes Bein über den Widerstand an D9, kurzes Bein an GND), Motor und Treiber abziehen, beide IMUs wie gewohnt anschliessen. Die LED zeigt, was der Motor tun würde. Genaue Anleitung: `docs/testberichte/REQ-003-runde-1.md`, Abschnitt «Am echten Board prüfen».

## Tests am Board

### REQ-003: LED folgt dem Motor

- [ ] **T1 (AK-1):** Nach dem Einschalten bleibt die LED aus, solange der Arm ruhig und nicht überstreckt ist.
- [ ] **T2 (AK-2):** Nach dem Einmessen leuchtet die LED, solange der Arm über die Grenze überstreckt gehalten wird.
- [ ] **T3 (AK-1):** Geht der Arm zurück in den sicheren Bereich, geht die LED wieder aus.
- [ ] **T4 (AK-3):** Beim mehrmaligen Überstrecken und Zurückgehen geht die LED ohne spürbare Verzögerung an und aus.

### REQ-002: Einmessen und Winkel

- [ ] **T5 (AK-9):** Nach dem Einschalten erscheint auf dem Seriellen Monitor etwa jede Sekunde `Keine Referenz`, und die LED bleibt beim Überstrecken aus.
- [ ] **T6 (AK-6):** Das Einmessen mit Tastendruck, 2 s still, 2 Mal beugen mit kleinem Oberarm-Kreis und einmal Hand drehen klappt und endet nach etwa 7,5 s mit der Bestätigung (ein längeres Aufleuchten).
- [ ] **T7 (AK-6):** Die Stösse kommen zur richtigen Zeit: einer nach 2 s («jetzt bewegen»), zwei kurze nach 6 s («jetzt Hand drehen»).
- [ ] **T8 (AK-7b):** Wer in Phase 1 den Arm bewegt, bekommt `Einmessen abgebrochen - Grund: ruhig`.
- [ ] **T9 (AK-7a):** Wer in Phase 2 kaum beugt, bekommt `Einmessen abgebrochen - Grund: Beugung`.
- [ ] **T10 (AK-7d):** Wer in Phase 2 den Oberarm still hängen lässt, bekommt `Einmessen abgebrochen - Grund: Oberarm`.
- [ ] **T11 (AK-7e):** Wer in Phase 3 die Hand nicht dreht, bekommt `Einmessen abgebrochen - Grund: Hand`.
- [ ] **T12 (AK-1):** Nach dem Einmessen löst das Drehen des Unterarms (Handfläche oben bis unten) in verschiedenen Armlagen keinen Alarm aus.
- [ ] **T13 (AK-16):** Ein leicht schiefes Einmessen (Arm etwa 5° nach vorn oder zur Seite) klappt, und danach stimmt die Warnung trotzdem.
- [ ] **T14 (AK-10):** Nach 10 Minuten normaler Bewegung mit kurzen Pausen warnt das Gerät immer noch genau bei der Überstreckung und nicht schon vorher oder erst später.
- [ ] **T15 (AK-12):** Eine länger gehaltene Überstreckung (etwa 1 Minute) bleibt die ganze Zeit gewarnt, die LED geht nicht von selbst aus.
- [ ] **T16 (AK-13):** Nach kurzem Abziehen und Wiedereinstecken einer IMU gibt es keinen Fehlalarm, und spätestens 5 s später warnt das Gerät wieder normal.
- [ ] **T17 (AK-14):** Sehr schnelles Drehen des Arms löst keinen Fehleralarm (IMU-Ausfall) aus.
- [ ] **T18 (AK-15):** Die Zeile `Zyklus max: … us` zeigt immer höchstens 15000 us.

## Entscheide, die wir fällen müssen

- [ ] **E1 IMU-Typ:** Bleiben wir beim MPU6050 (so ist der Code gebaut) oder nehmen wir den BNO085 (dann wird ein grosser Teil der Winkelberechnung neu geschrieben)?
- [ ] **E2 IMU-Bibliothek:** Brauchen wir eine Bibliothek, oder bleiben wir beim direkten Lesen der Register ohne Bibliothek wie bisher?
- [ ] **E3 Motortreiber:** Welchen Motortreiber kaufen beziehungsweise verbauen wir?
- [ ] **E4 Taste:** Bleibt die Taste fix an D2 (bisher nur «vorläufig» für Wokwi gewählt)?
- [ ] **E5 Status-LED:** Bekommt das Gerät eine eigene Status-LED, und wenn ja, an welchem Pin?
- [ ] **E6 Motorstärke:** Bleibt die Motorstärke bei 200 von 255, oder stellen wir sie nach dem Spür-Test am Arm anders ein?
- [ ] **E7 Achsen-Schwelle:** Bestätigen wir die Schwelle `ACHSE_LAMBDA_MIN = 3000`, die der Developer selbst gesetzt hat, oder passen wir sie nach den Board-Tests an?
- [ ] **E8 I²C-Schutz:** Starten wir als nächste Anforderung den Schutz gegen einen hängenden I²C-Bus (`Wire.setWireTimeout` und eventuell Watchdog, Bedrohung B-3)?
- [ ] **E9 Abnahme:** Nehmen wir REQ-002 und REQ-003 ab, wenn die Tests oben bestanden sind?
- [ ] **E10 Testskript:** Behalten wir `docs/tests/szenarien/winkelverlauf.py` (dann braucht es einen Test, der die Daten liest) oder löschen wir es?

## Nach dem Test

Ergebnisse dem Engineer geben (Chat mit Claude). Er trägt sie in die Anforderungen und in `docs/offene-hinweise.md` ein und setzt bei Bestehen die Abnahme fort.
