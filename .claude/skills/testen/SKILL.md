---
name: testen
description: Lässt den Tester den aktuellen Code-Stand prüfen, ausserhalb der Pipeline (z. B. nachdem jemand in der Arduino IDE etwas von Hand geändert hat).
argument-hint: "[REQ-xxx, optional]"
disable-model-invocation: true
---

# /testen: aktuellen Code prüfen

Starte den Sub-Agenten **tester** (siehe `.claude/agents/tester.md`) auf den aktuellen Code in `jointsense/`.
Optionaler Bezug: $ARGUMENTS

Deutsch, extrem simpel. **Nie hochladen, nie den seriellen Port belegen, nie committen.**

## Auftrag an den Tester

- Führe die ganze Prüfkette mit den **bestehenden** Tests in `docs/tests/` aus:
  1. Kompilieren mit allen Warnungen, Speicher (RAM) prüfen
  2. `cppcheck`
  3. Logik- und Fuzz-Tests mit `g++`
  4. Python-Szenarien (Winkelverläufe)
  5. Wokwi-Simulation mit Störfällen
  6. Bibliotheken gegen `docs/tech-stack.md` prüfen
- Ist eine Anforderung angegeben, nur deren Tests. Sonst alle.
- Fehlt ein Werkzeug: diesen Schritt als «übersprungen» melden und auf `/einrichten` hinweisen. Nicht abbrechen.
- Bericht schreiben: `docs/testberichte/manuell-JJJJ-MM-TT.md` (heutiges Datum).
- **Keinen Produkt-Code ändern**, keine Tests umschreiben.

## Danach

Fasse dem User zusammen: was bestanden hat, was nicht, was übersprungen wurde.
Schlägt etwas fehl, das zu einer Anforderung gehört, schlag `/weiter REQ-xxx` oder `/anforderung` vor.
Den Status der Anforderungen änderst du hier **nicht**.
