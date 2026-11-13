# Board-Test REQ-005: Einmessen (still halten, dann frei bewegen)

Für das Team, am echten Board und am echten Arm. Wokwi wird für REQ-005 nicht benutzt (Kontingent nicht verfügbar).
Hochladen macht ein Mensch (Arduino IDE). Serieller Monitor: 115200 Baud (Zeilenende «Neue Zeile» oder «Beides», beides testen).
Zuerst `python docs/tests/alle_tests.py` laufen lassen: das prüft die Logik. Diese Liste prüft, was nur das Gerät kann.

## A. Anzeige vor dem Einmessen (AK-1)

- [ ] Ohne Referenz, beide IMUs angeschlossen: jede Sekunde (± 0,1 s) erscheinen die Zeile `Keine Referenz …` **und** `Neigung Oberarm: <a>  Unterarm: <b> Grad` (eine Nachkommastelle).
- [ ] IMU von Hand kippen: die Zahl ändert sich passend (Waagrecht = 0, senkrecht = ±90).
- [ ] Eine IMU abstecken: die Neigungs-Zeile fehlt, die `FEHLER`-Zeile bleibt (REQ-001).
- [ ] Während des Einmessens und mit gültiger Referenz erscheint **keine** Neigungs-Zeile.

## B. Schritt 1: Still (AK-2 bis AK-6)

- [ ] Taste kurz drücken (oder Enter): `Schritt 1: Arm haengen lassen und still halten.` genau einmal.
- [ ] Jede Sekunde `Ruhig: x.x s von 2.0 s`. Arm still halten: Zähler läuft bis 2.0 (± 0,1 s).
- [ ] Mitten drin kurz wackeln: der Zähler geht auf 0 und zählt neu, **kein** Abbruch.
- [ ] Nach 2.0 s: 1 Vibrationsstoss (ca. 300 ms) und `Schritt 1 fertig. Enter oder Taste fuer Schritt 2 (frei bewegen).`
- [ ] Dauernd wackeln: nach 30 s (± 0,1 s) `Einmessen abgebrochen - Grund: ruhig`, keine Bestätigungsvibration.
- [ ] Im Warte-Zustand nichts tun: nach 30 s `Einmessen abgebrochen - Grund: Bestaetigung`.
- [ ] Warte-Zustand: 29 s warten und dabei den Arm frei bewegen (AK-6), dann bestätigen und normal einmessen: danach stimmt der Winkel (gestreckt etwa 0°, Fehler höchstens ca. 2,5°).
- [ ] Enter/Taste während Schritt 1 und Schritt 2 werden ignoriert (nicht gemerkt): nach dem Abbruch startet nichts von allein.

## C. Schritt 2: Frei bewegen (AK-7 bis AK-13) — das eigentliche Ziel

- [ ] Bestätigen: **kein** Vibrationsstoss, `Schritt 2: Arm frei bewegen (beugen, Oberarm bewegen, Hand drehen).`
- [ ] Jede Sekunde eine Zeile `Beugung <b>/30  Oberarm <o>/15  Hand <h>/45  Achse <a>%`, ganze Zahlen, ` OK` hinter erfüllten Werten.
- [ ] Zahlen steigen nur (werden nie kleiner), solange Schritt 2 läuft.
- [ ] Beugungs-Zahl steigt beim ersten Beugen sofort (spätestens 2 s nach der Bewegung), auch wenn Oberarm und Hand noch nicht bewegt wurden (AK-8). Jeder der drei Grad-Werte steigt nur durch seine eigene Bewegung.
- [ ] **Normal bewegen** (2 Mal beugen, Oberarm kreisen, Hand drehen, Reihenfolge beliebig): `Referenz gesetzt.` und 150 ms Vibration. Notieren: wie lange hat es gedauert? Welche Zahl war zuletzt der Engpass?
- [ ] Reihenfolge ändern (zuerst Hand, dann Beugen, dann Oberarm), mit Pausen, mit zusätzlichem Herumschwenken: klappt trotzdem.
- [ ] **Schlampig** bewegen (kleine Kreise, langsam, ungenau): klappt es? Wenn nicht: Zeilen aus dem Monitor sichern (Fortschritt!), sie sind die Zahlen für die Grenzwert-Diskussion (nicht lockern ohne Team-Entscheid).
- [ ] Nur beugen, Oberarm still halten: nach 30 s Abbruch `Grund: Oberarm`, davor steht noch einmal die letzte Fortschrittszeile.
- [ ] Beugen und Oberarm, Hand nicht drehen: `Grund: Hand`. Nur Hand drehen: `Grund: Beugung`.
- [ ] Mit 5° schiefem Arm einmessen (AK-14): klappt oder bricht ab, aber kein falscher Winkel danach.

## D. Nach dem Einmessen (AK-10, REQ-002 AK-1 bis AK-4)

- [ ] Gestreckter Arm zeigt etwa 0°, Beugen zeigt negative Werte, Überstreckung positive (Vibration ab 5° wie bisher).
- [ ] Unterarm drehen, Oberarm drehen, Arm schwenken: der Winkel ändert sich um höchstens 2° (Augenmass).
- [ ] 10 Mal nacheinander einmessen, jedes Mal den gestreckten Arm lesen: Werte höchstens 3° auseinander (AK-18, ± 1,5°).

## E. Abbruch und Sicherheit (AK-15, AK-16, B-7)

- [ ] Bei **jedem** Abbruch: Zeile `Einmessen abgebrochen - Grund: …`, keine Bestätigungsvibration, alte Referenz bleibt (Arm lesen), sofort wieder startbar.
- [ ] Abbruch durch IMU-Ausfall (Kabel abziehen) in Schritt 1, im Warte-Zustand und in Schritt 2: `Grund: IMU`, danach `FEHLER`-Muster wie REQ-001.
- [ ] Während des ganzen Einmessens keine Überstreckungs-Warnung (Arm bewusst überstrecken, kein Motor ausser Warte-Stoss und Schluss-Vibration).
- [ ] B-7: Vom Laptop Leerzeichen/Enter schicken ausserhalb der Schritte und in den Schritten: nur das Erste (Start bzw. Bestätigung) wirkt, nichts wird gemerkt.

## F. Tempo und Speicher (AK-17)

- [ ] Zeile `Zyklus max`: im normalen Betrieb ≤ 15 ms.
- [ ] Während Schritt 2: die Auswertung darf einen Zyklus auf höchstens **100 ms** verlängern (AK-17, neu: vorher 40 ms), und das höchstens einmal pro Sekunde. In `Zyklus max` (Einheit µs) darf also bis ca. 100000 stehen, aber nur in der Zeile pro Sekunde, in der die Auswertung lief; ausserhalb von Schritt 2 weiter ≤ 15000 µs. Notieren: der höchste gemessene Wert.
- [ ] Während der Auswertung in Schritt 2 wird ein IMU-Ausfall (Kabel abziehen) trotzdem erkannt (REQ-001).
- [ ] Beim Kompilieren: Flash ≤ 32 256 Byte (besser nicht über 94 %), globale Variablen ≤ 70 % RAM.

## G. Wokwi-Szenarien

Nicht geprüft (Kontingent nicht verfügbar). Die Szenarien W1 bis W6 und die Vorspänne in `docs/wokwi/szenarien/` gehen noch vom alten Einmessen aus (siehe `docs/offene-hinweise.md`) und bleiben unverändert.
