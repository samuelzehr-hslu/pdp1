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

## Hardware-Check

Macht der Engineer, **bevor** die Anforderung fixiert wird.

| Prüfpunkt | Ergebnis | Bemerkung |
|---|---|---|
| Verdrahtung: README ↔ `pins.h` stimmen überein | ☐ ok / ☐ Widerspruch | |
| Kein Pin doppelt belegt | ☐ ok / ☐ Konflikt | |
| Strom: Akkubetrieb ≥ 4 h weiterhin möglich (QA-06) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |
| Gewicht: Gesamtmasse ≤ 250 g weiterhin möglich (QA-07) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |
| Sensor-Grenzen: Messbereich, Abtastrate, Auflösung reichen (QA-01) | ☐ ok / ☐ nicht betroffen / ☐ kritisch | |

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
