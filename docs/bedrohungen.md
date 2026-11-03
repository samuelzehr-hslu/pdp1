# Bedrohungen (Cybersecurity und Ausfälle)

Diese Liste sammelt, was JointSense stören oder angreifen könnte.

- Der **Engineer** prüft bei jeder Anforderung, ob eine neue Bedrohung dazukommt, und trägt sie hier ein.
- Der **Developer** kennt diese Liste und schützt den Code dagegen.
- Der **Tester** prüft jede Bedrohung und schreibt ins Testprotokoll: *geschützt / nicht geschützt / akzeptiertes Risiko*.

**Haltung:** Wir arbeiten nach bestem Wissen und Gewissen. Ziel ist nicht perfekte Sicherheit, sondern dass nichts Gefährliches oder Unerwartetes passiert.

**Grundregel (fail-safe):** Wenn etwas schiefgeht, geht der Motor **aus**, nicht an. Das Gerät meldet den Fehler, statt still weiterzulaufen.

## Ausgangslage

Der Arduino hat **kein WLAN und kein Internet**. Angreifen oder stören kann man ihn nur über das, was in ihn hineingeht: USB/Serial, die Sensoren (I²C), die Taste und das aufgespielte Programm.

## Liste

| ID | Bedrohung | Beispiel | Mögliche Folge | Schutz (Idee) | Status |
|---|---|---|---|---|---|
| B-1 | **Kaputte Eingaben über USB/Serial** | Unsinn, sehr lange Texte, falsche Zahlen | Speicher läuft über, Arduino stürzt ab | Eingaben auf Länge und Wertebereich prüfen, festen Puffer nutzen, kein `String` | offen |
| B-2 | **Falsche Sensordaten** | IMU abgesteckt, Wackelkontakt, Werte wie `NaN`, unendlich oder 9999°, plötzliche Sprünge | Motor vibriert ohne Grund oder bleibt still, obwohl er warnen müsste | Werte auf Plausibilität prüfen (Bereich, `isnan`), Sensor-Ausfall erkennen und melden, Motor aus | offen |
| B-3 | **Lahmlegen** | I²C-Bus hängt, Speicher (RAM) wird voll | Gerät reagiert nicht mehr, Warnung fällt aus | `Wire.setWireTimeout`, eventuell Watchdog, keine dynamische Speicherverwaltung, RAM-Belegung im Blick | offen |
| B-4 | **Unsichere Bibliotheken** | Fremde oder veraltete Bibliothek aus unbekannter Quelle | Fehler oder Schadcode im eigenen Programm | Nur offizieller Library Manager, feste Version in `docs/tech-stack.md` | offen |
| B-5 | **Neues Programm per USB aufspielen** | Jemand mit USB-Kabel lädt eine eigene Firmware hoch | Gerät macht etwas anderes | Auf Uno/Nano technisch **nicht** verhinderbar (kein Schutz für das Aufspielen vorhanden) | **akzeptiertes Risiko** |

Status-Werte: *offen* (noch nicht geprüft), *geschützt*, *nicht geschützt*, *akzeptiertes Risiko*.

## Neue Bedrohung eintragen

Nächste freie ID nehmen (B-6, B-7, …) und eine Zeile ergänzen. In der Anforderung (`REQ-xxx.md`, Abschnitt «Bedrohungen») auf die ID verweisen.
