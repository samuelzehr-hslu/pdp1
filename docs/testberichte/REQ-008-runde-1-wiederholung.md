# Testbericht REQ-008, Runde 1 (Wiederholung der statischen Prüfung)

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-008](../anforderungen/REQ-008.md) |
| **Runde** | 1 von 3 (Code unverändert) |
| **Datum** | 2026-11-17 |
| **Bezug** | [REQ-008-runde-1.md](REQ-008-runde-1.md): alles andere gilt unverändert |
| **Gesamtergebnis** | Der einzige rote Test aus Runde 1 ist behoben. Code und Board-Prüfung wie in Runde 1 (Board-Teile weiter offen, siehe dort). |

## Test-Korrektur 1 von 3 (vom Engineer freigegeben)

- Datei: `docs/tests/flex_statisch.py`, Prüfung «AK-14: Serial.begin(115200)».
- Vorher: verlangte die Zahl 115200 direkt in der Klammer.
- Jetzt: gilt auch `Serial.begin(KONSTANTE)`, wenn die Konstante im Sketch auf 115200 gesetzt ist. Der Wert muss genau 115200 sein, sonst bleibt die Prüfung rot.
- Die Anforderung (Baudrate 115200) wurde dadurch nicht geändert.

## Ergebnis

| Prüfschritt | Ergebnis | Details |
|---|---|---|
| 3c `flex_statisch.py` | ok | 38 von 38 Prüfungen bestanden (gefunden: `Serial.begin(SERIAL_BAUD)`, Wert 115200) |
| Alle übrigen Schritte (Kompilieren, cppcheck, Logik-/Fuzz-Tests, GUI, Bibliotheken, Sicherheitsregeln) | nicht wiederholt | Code unverändert, Ergebnis aus Runde 1 gilt |

AK-14 ist damit im Code erfüllt. Datenrate und Zeilenformat am echten Board: wie in Runde 1 (BT-1, BT-12). Die Board-Checkliste steht in Runde 1.
