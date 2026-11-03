# Szenarien (Python)

Python erzeugt hier **typische Bewegungen des Arms** als Zahlenreihen (CSV), z. B. «Arm langsam überstrecken» oder «Zittern an der Grenze».
Mit diesen Zahlen füttert der Tester die **echte** C++-Logik in den Logik-Tests.

**Wichtig:** Die Logik wird in Python **nicht** nachgebaut. Sonst gäbe es zwei Versionen, die irgendwann nicht mehr übereinstimmen.

Der Developer darf diesen Ordner nicht lesen (wie alles unter `docs/tests/`).

## Starten

```bash
python docs/tests/szenarien/winkelverlauf.py      # Mac/Linux: python3 ...
```

Ergebnis: CSV-Dateien in `scratch/szenarien/` (nicht im Repo) mit den Spalten `zeit_ms, winkel_grad`.

## Enthaltene Szenarien

| Name | Was passiert | Erwartung (Beispiel) |
|---|---|---|
| `langsam_ueberstrecken` | −20° → +10° → −20° in 6 s | Motor geht an, sobald die Grenze lange genug überschritten ist, und wieder aus |
| `zittern_an_grenze` | Arm zittert ±1° um die Grenze | Motor flattert nicht ständig an/aus (Hysterese) |
| `kurzer_ausreisser` | Eine einzelne Messung springt auf +15° | Kein Alarm (Entprellung) |

Neue Szenarien: eine Funktion in `winkelverlauf.py` ergänzen und in `main()` aufrufen. Keine Zufallszahlen ohne festen Startwert.
