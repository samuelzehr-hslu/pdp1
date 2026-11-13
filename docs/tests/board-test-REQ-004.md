# Board-Test zu REQ-004 (Einmess-Schritte bestätigen)

Warum am Board? Der Ablauf mit Warten steckt in `jointsense.ino` (Hardware: Seriell, Taste, Motor, Zeit). Die PC-Tests (`test_REQ-004*.cpp`) prüfen nur die reine Logik. Wokwi ist diesen Monat nicht verfügbar (Kontingent aufgebraucht), siehe `docs/wokwi/REQ-004-stoerfaelle.md`.

**Vorher:** Den anderen im Team sagen, dass ihr das Board braucht. Hochladen macht ein Mensch (Arduino IDE oder `arduino-cli upload`). Aufbau wie `board-test-2026-11-13.md` (LED an D9 statt Motor, 220 Ω). Seriellen Monitor auf 115200 (oder was die Firmware nutzt) stellen. Neben jedem Test **ok** oder **nicht ok** und eine kurze Beobachtung notieren.

Abkürzungen: «Enter» = Eingabe im Seriellen Monitor abschicken. Das Zeilenende-Menü unten im Monitor hat vier Stellungen: «Kein Zeilenende», «Neue Zeile», «Zeilenumbruch (CR)», «Beides, NL & CR».

## Zeichen und Bestätigung (AK-1, AK-2, AK-3, AK-4)

- [ ] **B-1 (AK-1, AK-4):** Zeilenende «Neue Zeile». Nur Enter schicken (leere Zeile): Das Einmessen startet (Phase 1). Wiederholen mit «CR» und mit «Beides».
- [ ] **B-1b (AK-1):** Zeilenende «Kein Zeilenende». Ein einzelnes Leerzeichen schicken: Das Einmessen startet. Danach `a`, `1`, `x`, `+` schicken: nichts passiert (kein Start, kein Weiterschalten, keine Fehlermeldung).
- [ ] **B-1c (AK-1):** Taster kurz drücken statt Enter: Das Einmessen startet ebenso (wie bisher).
- [ ] **B-2 (AK-2):** Zeilenende «Beides, NL & CR». Im Warte-Zustand 1 ein Leerzeichen plus Enter schicken (also `" \r\n"`): Es startet **nur** Schritt 2. Das Gerät geht nicht gleich weiter in Warte-Zustand 2 oder Schritt 3. Dasselbe mit Taster **und** Enter fast gleichzeitig.
- [ ] **B-3 (AK-3):** Während Phase 1, 2 oder 3 Enter schicken (mehrmals): Es passiert nichts. Am Ende der Phase wartet das Gerät **trotzdem** (die frühe Bestätigung wurde nicht gemerkt). Erst ein neues Enter schaltet weiter.

## Ablauf und Zeiten (AK-5 bis AK-8, AK-14)

Stoppuhr oder Handy-Video (Ton aus) nutzen. Toleranz in diesen Tests von Hand: ± 0,3 s. Die genauen ± 100 ms sind am Board nicht messbar, nur am Logikanalysator.

- [ ] **B-4 (AK-4 bis AK-8):** Voller Ablauf mit Enter: Start, Phase 1 (2 s still). Danach ein Stoss (300 ms) und die Zeile `Schritt 1 fertig. Enter oder Taste fuer Schritt 2 (Bewegung).` (genau einmal). Warten (die Hand locker bewegen). Enter: **ein** kurzer Stoss (100 ms), 4 s 2 Mal beugen mit Oberarm-Kreis. Danach ein Stoss und die Zeile `Schritt 2 fertig. Enter oder Taste fuer Schritt 3 (Hand drehen).` (genau einmal). Warten. Enter: **zwei** Stösse, 1,5 s Hand drehen. Danach die Bestätigungsvibration (150 ms, länger). Es steht **keine** Zeile `Einmessen abgebrochen` im Monitor.
- [ ] **B-4b:** Wie B-4, aber nur mit dem Taster (kein Enter).
- [ ] **B-5 (AK-8):** Der Warte-Stoss (300 ms, «fertig, warte») ist am Arm vom 100-ms-Stoss («jetzt bewegen») und von den zwei Stössen («jetzt Hand drehen») unterscheidbar. Bitte kurz aufschreiben, wie es sich anfühlt (Stärke und Länge). Hinweis: Mit der LED statt dem Motor mindestens die Länge von Auge prüfen.
- [ ] **B-6 (AK-8):** Enter schon 100 ms nach dem Warte-Stoss (kurz nach Warte-Zustand): Der Warte-Stoss wird sofort vom Stoss der nächsten Phase ersetzt (kein 300-ms-Stoss, dann sofort der kurze).
- [ ] **B-7 (AK-14):** Messzeit: Von Start bis zur Bestätigungsvibration (ohne Wartezeit der Person) etwa 7,5 s (2 + 4 + 1,5), je Phase mit Stoppuhr ± 0,3 s.

## Warten, Abbruch, Sicherheit (AK-9 bis AK-12)

- [ ] **B-8 (AK-9):** Nach Schritt 1 nichts tun, 30 s abwarten (Stoppuhr): Nach 30 s ± 1 s steht `Einmessen abgebrochen - Grund: Bestaetigung`, keine Bestätigungsvibration. Danach sofort neu starten (Enter): klappt. Dasselbe nach Schritt 2.
- [ ] **B-8b (AK-9, AK-11):** Nach dem Abbruch durch Wartezeit: War vorher ein Einmessen erfolgreich, warnt das Gerät bei Überstreckung weiter wie vorher (alte Referenz). War keines erfolgreich, steht weiter `Keine Referenz` im Monitor.
- [ ] **B-9 (AK-10):** Während Phase 1 bis 3 **und** in beiden Warte-Zuständen den Arm überstrecken (über den Grenzwert): LED/Motor bleibt aus (ausser den Stössen des Einmessens). Sofort nach dem Ende des Einmessens oder nach einem Abbruch warnt das Gerät wieder normal.
- [ ] **B-10 (AK-11):** Phase 1 bewegen: `Einmessen abgebrochen - Grund: ruhig`. Phase 2 kaum beugen: `Grund: Beugung`. Phase 2 Oberarm still: `Grund: Oberarm`. Phase 3 Hand nicht drehen: `Grund: Hand`. Jedes Mal keine Bestätigungsvibration, alte Referenz bleibt, sofort wieder startbereit.
- [ ] **B-11 (AK-12):** In einem Warte-Zustand (und einmal in einer Phase) eine IMU abziehen: `Einmessen abgebrochen - Grund: IMU` und das Fehlermuster aus REQ-001. Nach dem Wiederanstecken ist das Gerät startbereit.
- [ ] **B-12 (AK-13):** Beide Wartezeiten je etwa 25 bis 29 s, dabei den Arm frei bewegen (hinsetzen, Laptop bedienen). Dann Phase 2 und 3 normal. Danach den Arm 3 Minuten normal bewegen, Hand drehen, beugen: Die Warnung kommt nur bei echter Überstreckung (kein Fehlalarm, keine verpasste Warnung, Genauigkeit wie nach einem normalen Einmessen).

## Zykluszeit und Angriffsfläche (AK-15, B-1, B-7)

- [ ] **B-13 (AK-15):** Im Warte-Zustand und in den Phasen die Zeilen `Zyklus max: … us` anschauen: höchstens 15 000. Auch wenn man 500 Zeichen auf einmal einfügt und abschickt: Das Gerät hängt nicht, die Zeilen kommen weiter.
- [ ] **B-14 (B-1):** Einen sehr langen Text mit Sonderzeichen und Umlauten einfügen (ca. 2000 Zeichen, ohne Leerzeichen und Zeilenumbruch): Nichts passiert, das Gerät läuft normal weiter.
- [ ] **B-15 (B-7):** Einen Text mit sehr vielen Leerzeichen einfügen (ca. 500): Das Einmessen startet höchstens und bricht dann ab (z. B. `Grund: Beugung`), **nie** eine neue Referenz ohne echte Bewegung. Die alte Referenz bleibt.

## Zuordnung zu den Kriterien

| Kriterium | Wo geprüft |
|---|---|
| AK-1 | PC (`istBestaetigungsZeichen`, alle 256 Zeichen), Board B-1, B-1b, B-1c, B-14 |
| AK-2, AK-3, AK-4 | nur Board (B-1, B-2, B-3) |
| AK-5, AK-6, AK-7 (Prüfungen) | PC (Auswerte-Funktionen), Board B-4, B-10 (Texte, Zeiten) |
| AK-8 | nur Board (B-4, B-5, B-6) |
| AK-9 | PC (`wartezeitAbgelaufen` inkl. Überlauf), Board B-8 (30 s) |
| AK-10 | nur Board (B-9) |
| AK-11, AK-12 | PC (Referenz bleibt, Neustart), Board B-8b, B-10, B-11 (Texte) |
| AK-13 | PC (`test_REQ-004_warten.cpp`, 29 s Wartezeit, Offset-Drift), Board B-12 |
| AK-14 | Board B-7 (Summe 7500 ms), QA-02 in der README anpassen |
| AK-15 | Board B-13, Code-Durchsicht (kein `String`, kein Warten auf Zeichen) |
