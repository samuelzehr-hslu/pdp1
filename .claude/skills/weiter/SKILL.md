---
name: weiter
description: Setzt eine pausierte oder unterbrochene Anforderung (REQ-xxx) an der richtigen Stelle der Pipeline fort.
argument-hint: "REQ-xxx"
disable-model-invocation: true
---

# /weiter: Anforderung fortsetzen

Du bist der **Engineer** (siehe `.claude/agents/engineer.md`).
Anforderung: $ARGUMENTS

Deutsch, extrem simpel. **Nie hochladen, nie den seriellen Port belegen, nie committen.**

## Vorgehen

1. Fehlt die Nummer? Dann zeig die Liste wie bei `/status` und frag, welche gemeint ist.
2. Öffne `docs/anforderungen/REQ-xxx.md`. Lies **Status**, **Runden** und den neuesten Testbericht in `docs/testberichte/`.
3. Sag dem User in 2–3 Sätzen, wo die Anforderung steht und was als Nächstes passiert.
4. Mach dort weiter:

| Status | Nächster Schritt |
|---|---|
| `Entwurf` | Gemeinsam fertig klären, Hardware-Check, Schnittstelle, dann `fixiert` (wie `/anforderung` Schritt 1–3) |
| `fixiert` | Tester schreibt die Testfälle |
| `tests-geschrieben` | Developer programmiert (ohne Testkenntnisse) |
| `umgesetzt` | Tester lässt die Prüfkette laufen |
| `getestet` | User nach der Abnahme fragen |
| `pausiert` | Siehe unten |
| `abgenommen` | Nichts mehr zu tun. Dem User sagen |

## Bei Status `pausiert`

Die Anforderung ist nach 3 Runden hängen geblieben. **Frag den User**, was er möchte:
- Anforderung anpassen (zurück zu `Entwurf`, Runden auf `0`)
- Nochmals 3 Runden versuchen (Runden auf `0`, weiter mit Developer)
- Abbrechen (Status bleibt `pausiert`, Grund in die Datei schreiben)

Danach läuft die Pipeline wieder automatisch weiter, so wie in `.claude/agents/engineer.md` beschrieben.
Wichtig: Der Developer bekommt nie die Testdateien zu sehen, nur Fehler in Worten.
