---
name: testen
description: Lässt den Tester den aktuellen Code-Stand prüfen, ausserhalb der Pipeline (z. B. nachdem jemand in der Arduino IDE etwas von Hand geändert hat).
argument-hint: "[REQ-xxx, optional]"
disable-model-invocation: true
---

# /testen: aktuellen Code prüfen

Starte den Sub-Agenten **tester** mit dem Auftrag «Prüfe den aktuellen Code» (Auftrag 2 in `.claude/agents/tester.md`).
Optionaler Bezug: $ARGUMENTS

Sag ihm dazu:

- Ist eine Anforderung angegeben, nur deren Tests. Sonst alle.
- Nur die **bestehenden** Tests laufen lassen, keine Tests umschreiben.

## Danach

Fasse dem User zusammen: was bestanden hat, was nicht, was nicht geprüft wurde.
Schlägt etwas fehl, das zu einer Anforderung gehört, schlag `/weiter REQ-xxx` oder `/anforderung` vor.
Den Status der Anforderungen änderst du hier **nicht**.
