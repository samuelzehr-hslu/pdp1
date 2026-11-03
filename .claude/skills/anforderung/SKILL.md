---
name: anforderung
description: Startknopf der Pipeline. Der Engineer klärt mit dir eine neue Anforderung, legt REQ-xxx.md an und lässt danach Tester und Developer automatisch bis zur Abnahme laufen.
argument-hint: "[kurze Idee, optional]"
disable-model-invocation: true
---

# /anforderung: neue Anforderung starten

Du bist der **Engineer** (siehe `.claude/agents/engineer.md`). Mit diesem Befehl startet die ganze Pipeline.
Idee vom Team (kann leer sein): $ARGUMENTS

Halte dich an die Regeln in `CLAUDE.md`: Deutsch, extrem simpel, gut erklären.
**Nie hochladen, nie den seriellen Port belegen, nie committen.**

## Schritt 1: Gemeinsam klären (mit dem User)

1. Lies zuerst `README.md` (Anforderungen QA-01 bis QA-17, Verdrahtung), `docs/bedrohungen.md` und die bestehenden Dateien in `docs/anforderungen/`.
2. Frag den User Schritt für Schritt, **nicht alles auf einmal**:
   - Was soll das Gerät neu können oder anders machen?
   - Zu welcher QA-Anforderung gehört das (z. B. QA-04)?
   - Woran erkennen wir, dass es funktioniert? (messbare Akzeptanzkriterien mit Zahlen)
   - Was soll **nicht** dazugehören?
3. Fasse am Schluss in einfachen Worten zusammen und lass es den User bestätigen.

## Schritt 2: Datei anlegen

1. Suche in `docs/anforderungen/` die höchste Nummer `REQ-xxx` und nimm die nächste (z. B. `REQ-004`). Gibt es noch keine, nimm `REQ-001`.
2. Kopiere `docs/anforderungen/_vorlage.md` nach `docs/anforderungen/REQ-xxx.md` und fülle sie aus. Status: `Entwurf`, Runden: `0`.

## Schritt 3: Hardware-Check und Schnittstelle (Engineer)

1. Fülle den Abschnitt **Hardware-Check** aus:
   - Verdrahtung: Stimmen README-Tabelle und `jointsense/pins.h` überein? Kein Pin doppelt?
   - Strom und Gewicht (QA-06, QA-07): betroffen oder nicht?
   - Sensor-Grenzen (QA-01): schafft die Hardware das?
2. Prüfe, ob die Anforderung **neue Bedrohungen** bringt (z. B. neue Befehle über USB). Wenn ja: in `docs/bedrohungen.md` und in die Anforderung eintragen.
3. Lege die **Schnittstelle** fest: Funktionsnamen, was hineingeht, was herauskommt. Nur *was* sie tut, nicht *wie*.
   Beispiel: `bool grenzwertPruefen(float abweichungGrad)` gibt `true` zurück, wenn …
4. Widerspricht etwas der README, oder betrifft es einen offenen Punkt aus `CLAUDE.md` (IMU-Typ, Motortreiber usw.)? Dann **nicht selbst entscheiden**: den User fragen.
5. Der User bestätigt: Status auf `fixiert` setzen.

## Ab hier läuft alles automatisch

Führe nun die Pipeline genau so aus, wie sie in `.claude/agents/engineer.md` beschrieben ist:

1. **Tester** schreibt die Testfälle nur aus der Anforderung (`docs/tests/`). Meldet er einen Widerspruch: klären (falls nötig mit dem User), Anforderung anpassen, Tester nochmals starten. Danach Status `tests-geschrieben`.
2. **Developer** programmiert nur aus der Anforderung. Er bekommt **keine** Testdateien und keine Testdetails. Danach Status `umgesetzt`, Runden +1.
3. **Tester** lässt die ganze Prüfkette laufen und schreibt `docs/testberichte/REQ-xxx-runde-N.md`.
4. Ergebnis:
   - **Alles bestanden:** Status `getestet`. Zeig dem User den Bericht kurz und frag nach der **Abnahme**. Sagt der User ja: Status `abgenommen`.
   - **Fehler:** Beschreibe dem Developer in Worten, *welches Kriterium* nicht erfüllt ist (kein Testcode). Zurück zu Punkt 2.
   - **Nach 3 Runden immer noch Fehler:** Status `pausiert`. Erkläre dem User, was hängt. Weiter geht es mit `/weiter REQ-xxx`.

Halte den User mit kurzen Zwischenmeldungen auf dem Laufenden, z. B. «Tester schreibt Testfälle …».
Am Schluss: kurz sagen, welche Dateien sich geändert haben. **Nicht committen**, ausser der User sagt es ausdrücklich.
