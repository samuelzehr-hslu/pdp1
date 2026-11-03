# Testbericht REQ-xxx, Runde x

<!--
Vorlage für einen Testbericht. Der Tester legt pro Runde einen Bericht an:
  REQ-001_runde-1.md, REQ-001_runde-2.md usw.
Der Developer darf diese Berichte NICHT lesen. Er bekommt nur den Abschnitt
«Fehler in Worten für den Developer» vom Engineer weitergegeben.
-->

| Feld | Wert |
|---|---|
| **Anforderung** | [REQ-xxx](../anforderungen/REQ-xxx.md) |
| **Runde** | x von 3 |
| **Datum** | JJJJ-MM-TT |
| **Code-Stand** | Git-Commit oder «nicht committet» |
| **Gesamtergebnis** | ☐ bestanden / ☐ nicht bestanden |

## Prüfschritte

| # | Prüfschritt | Ergebnis | Details |
|---|---|---|---|
| 1 | Kompilieren (`arduino-cli`), alle Warnungen als Fehler | ☐ ok / ☐ Fehler / ☐ nicht gelaufen | Anzahl Warnungen: |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ☐ ok / ☐ knapp | Belegt: … Bytes (…%) |
| 2 | Code-Scanner `cppcheck` | ☐ ok / ☐ Funde / ☐ nicht gelaufen | |
| 3 | Logik-Tests (`g++`) | ☐ ok / ☐ Fehler / ☐ nicht gelaufen | … von … bestanden |
| 3b | Fuzz-Tests (zufällige und extreme Werte) | ☐ ok / ☐ Fehler / ☐ nicht gelaufen | |
| 4 | Python-Szenarien (Winkelverläufe) | ☐ ok / ☐ Fehler / ☐ nicht gelaufen | |
| 5 | Wokwi-Simulation inkl. Störfälle | ☐ ok / ☐ Fehler / ☐ nicht gelaufen | |
| 6 | Bibliotheken (feste Version, offizielle Quelle, siehe `docs/tech-stack.md`) | ☐ ok / ☐ Fehler | |

«Nicht gelaufen» immer begründen (z. B. «g++ nicht installiert»). Ein nicht gelaufener Schritt ist **kein** Bestanden.

## Akzeptanzkriterien

| Kriterium | Ergebnis | Bemerkung |
|---|---|---|
| AK-1 | ☐ erfüllt / ☐ nicht erfüllt | |
| AK-2 | ☐ erfüllt / ☐ nicht erfüllt | |

## Bedrohungen

Für jede Bedrohung aus der Anforderung bzw. aus [`docs/bedrohungen.md`](../bedrohungen.md):

| Bedrohung | Ergebnis | Wie geprüft |
|---|---|---|
| B-x | ☐ geschützt / ☐ nicht geschützt / ☐ akzeptiertes Risiko | |

## Fehler in Worten für den Developer

Was ist **nicht** erfüllt? In Worten beschreiben, **ohne** Testcode, Testwerte oder Dateinamen aus `docs/tests/`.
Beispiel: «Bei Zittern um die Grenze schaltet der Motor zu oft ein und aus (verletzt AK-2, Hysterese).»

- …

## Hinweise für den Engineer

Unklarheiten oder Widersprüche in der Anforderung, die beim Testen aufgefallen sind.

- …
