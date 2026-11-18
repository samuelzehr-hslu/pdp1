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
| B-6 | **Falsche Einmessung oder Selbstkorrektur** | Einmess-Bewegung zu klein, Scharnier-Achse falsch bestimmt; Drift-Korrektur «lernt» eine gehaltene Überstreckung als normal; nach IMU-Ausfall ist die Lage unbekannt | Winkel stimmt nicht, Warnung fällt aus oder kommt ohne Grund | Einmessen bei zu kleiner Bewegung abbrechen, Referenz nie automatisch ändern, nach Ausfall keine Warnung bis die Lage wieder bekannt ist (REQ-002) | offen |
| B-7 | **Einmessen über USB auslösen oder blockieren** | Jemand am Laptop (oder ein Programm) schickt Leerzeichen/Enter: startet ein Einmessen oder lässt es in einem Warte-Zustand hängen (REQ-004) | In dieser Zeit keine Überstreckungs-Warnung; falsche Referenz, wenn die Person nicht bereit ist | Nur `' '`, `'\n'`, `'\r'` zählen, Bestätigungen während einer Messung werden ignoriert, Wartezeit höchstens 30 s, jede Phase wird geprüft, bei Abbruch bleibt die alte Referenz (REQ-004). Mit REQ-005: nur noch ein Warte-Zustand, Still, Warten und freies Bewegen je höchstens 30 s (zusammen höchstens ca. 90 s ohne Warnung) | offen |
| B-8 | **Flex-Sensor fällt aus** (Prototyp 2) | Streifen gebrochen, Kabel ab, Kurzschluss am Spannungsteiler, Sensor «kriecht» nach starkem Beugen | Keine Warnung oder Warnung ohne Grund | ADC-Wert ausserhalb 100–3995 gilt als ungültig, nach 3 ungültigen Werten in Folge Motor aus + LED 5 Hz + Meldung (REQ-008 AK-3, AK-4) | offen |
| B-9 | **Haptik-Treiber DRV2605L hängt oder antwortet nicht** (Prototyp 2) | Programm stürzt ab, während der Treiber vibriert (er vibriert weiter, bis er einen neuen Befehl bekommt); I²C gestört | Motor vibriert ohne Ende oder warnt nie | Watchdog 500 ms, nach jedem Start Motor aus; 3 Fehlschläge in Folge = Motor-Ausfall melden (REQ-008 AK-11, AK-12) | offen |
| B-10 | **LiPo-Akku** (Prototyp 2) | Plus/Minus am JST-Stecker vertauscht, Tiefentladung, Akku gequetscht oder durchstochen | Feather kaputt, Akku bläht sich auf, im schlimmsten Fall Brand | Nicht im Code lösbar: Polarität vor dem ersten Anschliessen mit Multimeter prüfen, nur Akku mit Schutzschaltung (der gewählte 403040 von Accuswiss hat eine), Akku geschützt im Gehäuse, nicht unbeaufsichtigt laden (REQ-008 Hardware-Check) | offen |
| B-11 | **Neue Firmware per BOOT-Taste** (Prototyp 2) | Wer BOOT gedrückt hält und USB ansteckt, sieht den Feather als USB-Laufwerk und kann eine eigene Firmware (UF2) hineinkopieren | Gerät macht etwas anderes | Wie B-5 technisch nicht verhinderbar | **akzeptiertes Risiko** |

Status-Werte: *offen* (noch nicht geprüft), *geschützt*, *nicht geschützt*, *akzeptiertes Risiko*.

## Neue Bedrohung eintragen

Nächste freie ID nehmen (zurzeit B-12) und eine Zeile ergänzen. In der Anforderung (`REQ-xxx.md`, Abschnitt «Bedrohungen») auf die ID verweisen.
