# Logik-Tests (g++)

Hier liegen die Tests für die reine Logik in `jointsense/logik.h` (Grenzwert, Entprellung, Hysterese, Einmessen).
Sie laufen **auf dem PC**, nicht auf dem Arduino. Darum darf `logik.h` keine Arduino-Befehle enthalten.

**Wer schreibt die Tests?** Der **Tester**, und zwar **bevor** der Developer programmiert (nur aus der Anforderung `REQ-xxx.md`).
**Der Developer darf diesen Ordner nicht lesen oder ändern.** Ein Hook in `.claude/hooks/` sperrt ihn.

## Dateien

| Datei | Inhalt |
|---|---|
| `test_hilfe.h` | Einfache Prüf-Funktionen `pruefe(...)`, `pruefeNahe(...)`, `testErgebnis()` |
| `test_REQ-xxx.cpp` | Tests zu einer Anforderung, eine Datei pro Anforderung, jede mit eigenem `main()` |

## Tests starten

```bash
python docs/tests/alle_tests.py      # Mac/Linux: python3 docs/tests/alle_tests.py
```

Das Skript übersetzt jede `test_*.cpp` mit `g++` (oder `clang++`, falls kein g++ da ist, z. B. auf Windows ARM) (alle Warnungen = Fehler), legt die Programme in `scratch/tests/` ab und startet sie.

## Regeln für gute Tests

- Jeder Test nennt im Text das Akzeptanzkriterium, z. B. `"AK-2: Motor bleibt aus bei 4.9 Grad"`.
- Nur die **Schnittstelle** aus der Anforderung benutzen (Funktionsnamen, Ein- und Ausgaben).
- **Fuzz-Tests:** Die Funktionen auch mit tausenden zufälligen und extremen Werten aufrufen (`NAN`, `INFINITY`, sehr grosse Zahlen, negative Zeiten). Erwartung: kein Absturz, und im Zweifel bleibt der Motor **aus** (siehe `docs/bedrohungen.md`, B-2).
- Zufallswerte mit festem Startwert (`srand(42)`), damit ein Fehler wiederholbar ist.
