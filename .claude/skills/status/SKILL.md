---
name: status
description: Zeigt eine Übersicht aller Anforderungen (REQ-xxx) mit Status, Runden und nächstem Schritt.
---

# /status: Übersicht über alle Anforderungen

Deutsch, extrem simpel. Dieser Befehl **liest nur**, er ändert nichts.

## Vorgehen

1. Lies alle Dateien `docs/anforderungen/REQ-*.md` (nicht `_vorlage.md`).
2. Hol aus jeder Datei: Nummer, Titel, Status, Runden.
3. Schau in `docs/testberichte/`, welcher Bericht der neueste zur Anforderung ist.
4. Zeig eine Tabelle:

| REQ | Titel | Status | Runden | Nächster Schritt |
|---|---|---|---|---|
| REQ-001 | … | getestet | 2 | Abnahme durch euch |

Nächster Schritt je Status:
- `Entwurf`: fertig klären, mit `/weiter REQ-xxx`
- `fixiert`: Tester schreibt Testfälle
- `tests-geschrieben`: Developer programmiert
- `umgesetzt`: Tester prüft
- `getestet`: Abnahme durch euch
- `pausiert`: ihr entscheidet, mit `/weiter REQ-xxx`
- `abgenommen`: fertig

Gibt es noch keine Anforderung: sag das und schlag `/anforderung` vor.
Füge unter der Tabelle höchstens 2–3 Sätze an, z. B. was zuerst erledigt werden sollte.
