# Testbericht REQ-xxx, Runde x

<!--
Vorlage für einen Testbericht. Der Tester legt pro Runde einen Bericht an:
  REQ-001-runde-1.md, REQ-001-runde-2.md usw.
  Nochmals geprüft ohne Code-Änderung: REQ-001-runde-1-wiederholung.md
  Bei /testen ohne Anforderung: manuell-JJJJ-MM-TT.md
Ab dem zweiten Bericht zur selben Anforderung: Bei Unverändertem reicht «wie Runde N».
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
| 1 | Kompilieren (`arduino-cli`), alle Warnungen als Fehler | ☐ ok / ☐ Fehler / ☐ nicht geprüft | Anzahl Warnungen: |
| 1b | Speicher: RAM-Belegung (Uno: 2048 Bytes) | ☐ ok / ☐ knapp | Belegt: … Bytes (…%) |
| 2 | Code-Scanner `cppcheck` | ☐ ok / ☐ Funde / ☐ nicht geprüft | |
| 3 | Logik-Tests und Szenarien (`alle_tests.py`) | ☐ ok / ☐ Fehler / ☐ nicht geprüft | … von … bestanden |
| 3b | Fuzz-Tests (zufällige und extreme Werte) | ☐ ok / ☐ Fehler / ☐ nicht geprüft | |
| 4 | Wokwi-Simulation inkl. Störfälle | ☐ ok / ☐ Fehler / ☐ nicht geprüft / ☐ nicht wiederholt | |
| 5 | Bibliotheken (feste Version, offizielle Quelle, siehe `docs/tech-stack.md`) | ☐ ok / ☐ Fehler / ☐ nicht wiederholt | |
| 6 | Sicherheitsregeln im Code | ☐ ok / ☐ Fehler / ☐ nicht wiederholt | |

«Nicht geprüft» immer begründen (z. B. «g++ nicht installiert») und auf `/einrichten` verweisen. Ein nicht geprüfter Schritt ist **kein** Bestanden.
«Nicht wiederholt» heisst: Der Code ist unverändert, das Ergebnis aus einer früheren Runde gilt (Runde nennen).

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
Bei jedem Fehler die vermutete Ursache nennen: Code, Test, Anforderung oder unklar.

- …

## Am echten Board prüfen

Was weder Logik-Tests noch Wokwi belegen können. Das Team prüft es vor der Abnahme von Hand.
Steht in der Anforderung bei «Neue Bauteile» ein erster Test am Board, gehört er auch hierher. Grüne Tests sagen nichts über Spannung und Strom aus.

- …

## Hinweise für den Engineer

Unklarheiten oder Widersprüche in der Anforderung und alles, was jemand noch erledigen sollte (keine Fehler). Der Engineer überträgt offene Punkte nach `docs/offene-hinweise.md`.

- …

## Fehlende Werkzeuge

(z. B. «cppcheck fehlt → siehe /einrichten»)
