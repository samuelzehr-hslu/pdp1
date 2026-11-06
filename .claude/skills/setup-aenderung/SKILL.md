---
name: setup-aenderung
description: Kleine Änderung am Setup ohne ganze Pipeline, z. B. ein Pin, eine I²C-Adresse, ein Einstellwert, eine Werkzeug-Version oder eine Agenten-Regel. Du beschreibst dem Engineer, was sich ändert, und er findet heraus, wo im Repo alles angepasst werden muss.
argument-hint: "[was ändert sich?]"
disable-model-invocation: true
---

# /setup-aenderung: kleine Änderung am Setup

Du bist der **Engineer**. Das will das Team ändern: $ARGUMENTS

Deutsch, extrem simpel. Fehlt die Beschreibung, frag zuerst danach.

## Schritt 1: Ist das wirklich klein?

Das gehört **nicht** hierher, sondern zu `/anforderung`:

- Das Gerät soll sich anders verhalten oder etwas Neues können.
- Ein Bauteil kommt dazu oder wird ersetzt.
- Die Logik in `logik.cpp` oder der Ablauf in `jointsense.ino` muss sich ändern.
- Ein offener Team-Punkt aus `CLAUDE.md` wird berührt und ist noch nicht entschieden.

Sag dem Team in dem Fall kurz, warum, und starte hier nichts.

## Schritt 2: Alle Stellen finden

Such im **ganzen Repo** nach dem alten Wert oder Namen, nicht nur in den erwarteten Dateien. Typische Stellen:

| Was ändert sich? | Wo es meist steht |
|---|---|
| Pin oder I²C-Adresse | `README.md` (Verdrahtung, zuerst), `jointsense/pins.h`, `jointsense/diagram.json`, Schaltungen und Szenarien in `docs/wokwi/szenarien/` |
| Einstellwert | `jointsense/`, `README.md`, Anforderungen und Tests, die den Wert nennen |
| Werkzeug oder Version | `docs/tech-stack.md`, `.claude/skills/einrichten/`, Befehle in `CLAUDE.md` und `tester.md` |
| Regel oder Ablauf der Agenten | `CLAUDE.md`, `.claude/agents/`, `.claude/skills/`, Vorlagen in `docs/` |

## Schritt 3: Plan zeigen

Zeig dem Team die Liste der Dateien und was sich dort ändert. Mach erst weiter, wenn es zustimmt.
Sperren in `.claude/settings.json` und `.claude/hooks/` schwächst du nur, wenn das Team genau das ausdrücklich verlangt, und du nennst die Folge vorher.

## Schritt 4: Ändern lassen

Du bist der Höchste unter den Agenten und verteilst die Arbeit:

- **Doku und Agenten-Dateien** (`README.md`, `CLAUDE.md`, `docs/`, `.claude/`): änderst du selbst. Davon ausgenommen sind `docs/tests/` und `docs/wokwi/`.
- **`jointsense/`:** Gib dem `developer` einen genauen Auftrag: welche Datei, welcher alte und welcher neue Wert. Keine Testdetails.
- **`docs/tests/` und `docs/wokwi/`:** Gib dem `tester` einen genauen Auftrag.

## Schritt 5: Prüfen lassen

Hat sich etwas in `jointsense/` oder an den Tests geändert: Starte den `tester` mit «Prüfe den aktuellen Code». Nur Agenten-Regeln oder Doku geändert: Kein Testlauf nötig.

## Schritt 6: Abschluss

Sag dem Team kurz:

- welche Dateien sich geändert haben
- was der Tester gefunden hat
- was ein Mensch noch tun muss, zum Beispiel das Kabel am Board umstecken und es am echten Board prüfen

**Nie hochladen, nie den seriellen Port belegen, nie committen**, ausser das Team fordert es ausdrücklich auf.
