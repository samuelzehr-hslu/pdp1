# REQ-xxx: Titel der Anforderung

<!--
Vorlage für eine Anforderung. Kopieren als REQ-001.md, REQ-002.md usw.
Der Engineer füllt sie zusammen mit dem Team aus (Skill /anforderung).

Status-Ablauf:
  Entwurf → fixiert → tests-geschrieben → umgesetzt → getestet → abgenommen
  Dazu: pausiert (z. B. nach 3 Runden ohne Erfolg oder bei offener Rückfrage)
-->

| Feld | Wert |
|---|---|
| **ID** | REQ-xxx |
| **Status** | Entwurf |
| **Runde** | 0 von 3 |
| **Test-Korrekturen** | 0 von 3 |
| **Bezug** | QA-xx (siehe README, «Zentrale quantitative Anforderungen») |
| **Erstellt von** | Name |
| **Datum** | JJJJ-MM-TT |

## Beschreibung

Was soll das System können? In 2–4 einfachen Sätzen, aus Sicht der Person, die den Sleeve trägt.

## Akzeptanzkriterien

Jedes Kriterium muss **messbar** sein (Zahl + Einheit), damit der Tester es prüfen kann.

- [ ] AK-1: …
- [ ] AK-2: …

## Schnittstelle

Wie heissen die Funktionen und was geht rein und raus? **Nicht**, wie sie innen funktionieren.
Developer und Tester halten sich beide genau an diese Namen.

| Funktion | Eingaben | Rückgabe | Bedeutung |
|---|---|---|---|
| `beispielFunktion(float winkelGrad)` | Winkel in Grad | `bool` | `true`, wenn … |

Datei: `jointsense/logik.h` (reine Logik, ohne Arduino-Befehle)

## Physik-Annahmen

Nur wenn es um Bewegung oder Sensorwerte geht, sonst «nicht betroffen». Der Developer programmiert danach, der Tester baut seinen Simulator **nur** daraus, darum mit Zahlen:

- **Achsen und Vorzeichen:** Was ist 0°, was ist positiv?
- **Einbau der IMUs:** Wie dürfen sie sitzen und verdreht sein?
- **Bewegungen:** Welche, wie schnell und welche **gleichzeitig**?
- **Sensorfehler:** Offset, Rauschen, Beschleunigung durch Bewegung.
- **Was die Person tun muss:** z. B. beim Einmessen.
- **Machbarkeit:** Welche Grösse lässt sich aus welchen Sensordaten bestimmen, und was nicht?

## Hardware-Check

Macht der Engineer, **bevor** die Anforderung fixiert wird.

| Prüfpunkt | Ergebnis | Bemerkung |
|---|---|---|
| Verdrahtung: README ↔ `pins.h` stimmen überein | ☐ ok / ☐ Widerspruch | |
| Kein Pin doppelt belegt | ☐ ok / ☐ Konflikt | |
| Strom: Akkubetrieb ≥ 4 h weiterhin möglich (QA-06) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |
| Gewicht: Gesamtmasse ≤ 250 g weiterhin möglich (QA-07) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |
| Sensor-Grenzen: Messbereich, Abtastrate, Auflösung reichen (QA-01) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |
| Testbarkeit: Jedes Kriterium ist mit Logik-Tests, Wokwi oder am Board prüfbar | ☐ ok / ☐ Teile nur am Board / ☐ nicht prüfbar | Welches Kriterium wird womit geprüft? Ändert es Tests anderer Anforderungen? |

### Neue Bauteile

Nur wenn ein Teil dazukommt oder ersetzt wird, sonst «nicht betroffen». Ohne Datenblatt wird die Anforderung nicht fixiert.

| Prüfpunkt | Ergebnis | Bemerkung |
|---|---|---|
| Bauteil: genaues Modell und Datenblatt | ☐ vorhanden / ☐ fehlt | Modell, Link |
| Spannung passt zu Stromquelle und Logik (3.3 V / 5 V) | ☐ ok / ☐ Problem | Zahlen |
| Dauer- und Anlaufstrom passen zu Pin und Stromquelle | ☐ ok / ☐ Problem | Rechnung |
| Treiber oder Schutzteile nötig (z. B. Freilaufdiode) | ☐ nein / ☐ ja | Welche? |
| I²C: kein Adresskonflikt | ☐ ok / ☐ Konflikt / ☐ kein I²C | Adresse |
| Eignung für die Aufgabe | ☐ ok / ☐ nur per Versuch klärbar | Was zeigt der Versuch? |
| Rechnung von einem Teammitglied angeschaut | ☐ ja | Name, Datum |
| Erster Test am echten Board | – | Was ist zu prüfen, wenn das Teil zum ersten Mal angeschlossen wird? |

## Bedrohungen

Welche Einträge aus [`docs/bedrohungen.md`](../bedrohungen.md) betreffen diese Anforderung?
Bringt sie eine **neue** Bedrohung mit (z. B. neue Befehle über USB)? Dann dort eintragen.

- B-x: …

## Offene Fragen

Fragen, die das Team klären muss. Solange hier etwas offen ist, bleibt der Status «Entwurf» oder «pausiert».

- …

## Verlauf

| Datum | Wer | Was |
|---|---|---|
| JJJJ-MM-TT | Engineer | Anforderung angelegt (Entwurf) |
