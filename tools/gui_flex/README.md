# Live-GUI für Prototyp 2 (Flex-Sensor)

Mit `jointsense_flex_live.html` siehst du im Browser live, was der Feather RP2040 misst (REQ-008).
Das GUI ist **eine einzige Datei** und braucht **kein Internet**.

Für Prototyp 1 (2 IMUs) gibt es ein eigenes GUI in `tools/gui/`. Die beiden passen nicht zueinander.

## Was du brauchst

- **Chrome** oder **Edge** auf dem PC (nur diese Browser können «Web Serial», also mit einem USB-Gerät reden).
- Den Feather RP2040 mit der Firmware aus `jointsense_flex/`, und zwar mit `#define GUI_MODUS 1`
  oben in `jointsense_flex.ino` (so ist es im Repo eingestellt).
- Ein USB-Kabel, das Daten überträgt (manche Kabel können nur laden).

## So geht's (Schritt für Schritt)

1. Feather per USB anstecken.
2. **Arduino IDE: Seriellen Monitor schliessen.** Es kann immer nur ein Programm den Port benutzen.
3. Die Datei `jointsense_flex_live.html` doppelklicken. Sie öffnet sich im Browser.
   (Falls ein anderer Browser aufgeht: Rechtsklick → «Öffnen mit» → Chrome oder Edge.)
4. Knopf **«Verbinden»** drücken und im Fenster den Feather auswählen (oft «USB Serial Device» oder «Feather RP2040»).
5. Jetzt kommen die Werte. Die **Datenrate** sollte bei etwa **50** Zeilen pro Sekunde liegen.
6. Zum Einmessen: Arm entspannt gestreckt halten, Knopf **«Einmessen»** drücken (oder die Taste am Gerät)
   und den Arm 2 Sekunden ruhig lassen. Fertig ist es, wenn der Motor kurz vibriert und der Zustand «Bereit» zeigt.

## Was du siehst

| Anzeige | Bedeutung |
|---|---|
| Zustand | «Keine Referenz», «Bereit», «Einmessen», «Sensor-Ausfall» oder «Motor-Ausfall» |
| Motor | an oder aus |
| Abweichung | Prozent Abweichung vom eingemessenen Widerstand. **Positiv = Richtung Überstreckung**, negativ = Beugen |
| Widerstand | Widerstand des Flex-Sensors in kΩ (z. B. `21.5 kΩ`). `0.0 kΩ` = ungültiger Messwert |
| ADC-Rohwert | Rohwert von A0 (0 bis 4095). Gültig ist 100 bis 3995 |
| Datenrate | Datenzeilen pro Sekunde (soll etwa 50 sein) |
| Diagramm | Abweichung der letzten 10 Sekunden, die **rote gestrichelte Linie** ist der Grenzwert |
| Letztes Ereignis | z. B. «Einmessen fertig», «Sensor-Ausfall», «Keine Referenz» |

## Probleme

| Problem | Lösung |
|---|---|
| Knopf «Verbinden» tut nichts / Fehlermeldung | Chrome oder Edge benutzen. Seriellen Monitor der Arduino IDE schliessen. |
| Feather taucht nicht in der Liste auf | Anderes USB-Kabel probieren (Datenkabel). |
| Werte kommen nicht, Datenrate 0 | In `jointsense_flex.ino` muss `GUI_MODUS 1` stehen. Neu hochladen (das macht ein Mensch). |
| Zustand «Sensor-Ausfall» | Flex-Sensor oder Kabel prüfen (ADC unter 100 = Kabel ab, über 3995 = Kurzschluss). |
| Zustand «Motor-Ausfall» | Verkabelung zum DRV2605L prüfen (SDA, SCL, VIN, GND). |

## Technik (für Neugierige)

Der Feather schickt Textzeilen mit 115200 Baud:

- `G,<grenzwert x10>,<hysterese x10>,<motorstärke>`: Einstellungen, nach dem Start und beim Verbinden
- `D,<zeit ms>,<adc>,<widerstand Ohm>,<abweichung x10>,<motor 0/1>,<zustand 0-4>`: 50 Mal pro Sekunde
- `E,<text>`: Ereignisse

Kaputte oder unbekannte Zeilen ignoriert das GUI einfach.
