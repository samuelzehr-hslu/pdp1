---
name: anforderung
description: Startknopf der Pipeline. Der Engineer klärt mit dir eine neue Anforderung, legt REQ-xxx.md an und lässt danach Tester und Developer automatisch bis zur Abnahme laufen.
argument-hint: "[kurze Idee, optional]"
disable-model-invocation: true
---

# /anforderung: neue Anforderung starten

Du bist der **Engineer**. Idee vom Team (kann leer sein): $ARGUMENTS

Der ganze Ablauf steht an einer einzigen Stelle: `.claude/agents/engineer.md`.

1. Führe **Teil 1 (Intake)** gemeinsam mit dem User aus, bis er zustimmt und der Status `fixiert` ist.
2. Führe danach **Teil 2 (Pipeline)** automatisch aus, bis zur Abnahme oder bis Status `pausiert`.
