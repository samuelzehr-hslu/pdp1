# Stufen-Test für Prototyp 2 (Flex)

Mit diesem Sketch prüft ihr die Hardware **Schritt für Schritt**. Jede Stufe bringt **ein** neues Teil dazu.
Klappt eine Stufe nicht, wisst ihr genau, welches Teil schuld ist.

| Stufe | Was neu dazukommt | Was ihr prüft |
|---|---|---|
| 1 Sensor | nur A0 lesen (ADC-Rohwert, Widerstand) | Ändert sich der Wert beim Biegen? Ist er ruhig? Liegt er zwischen 100 und 3995? |
| 2 Grenzwert | Referenz setzen, Abweichung in %, Grenzwert 5 % mit Hysterese | Wird «Zu weit» ab 5 % rot und erst unter 4 % wieder grau? Wie viele % sind 5°, 10°, 15°? |
| 3 Taste + LED | Taste D5 setzt die Referenz, **rote LED auf dem Feather** leuchtet bei «zu weit» | Reagiert die Taste? Leuchtet die LED zur gleichen Zeit wie «Zu weit» im GUI? |
| 4 Motor | DRV2605L, Motor vibriert bei «zu weit» | Wird der DRV2605L gefunden? Vibriert der Motor? Hört er wieder auf? |
| 5 Komplett | der richtige Code `jointsense_flex/jointsense_flex.ino` | Ganzer Ablauf mit Einmessen, Ausfall-Erkennung, Watchdog |

## So geht's

1. **Einmal hochladen** (macht ein Mensch, nicht Claude): Arduino IDE → `tools/flex_stufen/flex_stufen.ino` öffnen →
   Board «Adafruit Feather RP2040» → Hochladen.
2. Seriellen Monitor schliessen, dann `tools/gui_flex/jointsense_flex_live.html` in Chrome oder Edge öffnen → **Verbinden**.
3. Oben steht **«Aktuelle Stufe: 1 – Sensor»**. Nach dem Einschalten startet der Feather immer in Stufe 1.
4. Checkliste der Stufe abhaken. Ist alles ok → Knopf **«Stufe 2»** drücken. **Kein neues Hochladen nötig.**
5. Ab Stufe 2: Arm entspannt gestreckt halten → **«Referenz setzen»** (oder ab Stufe 3 die Taste) → 1 Sekunde ruhig halten.
   Die Referenz bleibt erhalten, wenn ihr die Stufe wechselt.
6. Nach Stufe 4: `jointsense_flex/jointsense_flex.ino` hochladen = Stufe 5. Das GUI zeigt dann «Stufe 5 – Komplett».
   Zurück zu den Stufen 1–4: wieder `flex_stufen.ino` hochladen.

Ohne GUI geht es auch im Seriellen Monitor (115200 Baud): `1` bis `4` tippen = Stufe wechseln, Enter = Referenz setzen.

## Was ist anders als im richtigen Code?

Absichtlich **weggelassen**, damit nichts stört:

- kein Watchdog (kein Neustart, wenn etwas hängt)
- keine Fehlermeldungen jede Sekunde: jeder Fehler kommt **nur einmal**
- der Motor-Treiber wird **erst in Stufe 4** gesucht. In Stufe 1–3 kommt also nie «Motor-Ausfall».
- Referenz: einfacher Mittelwert über 1 Sekunde (der richtige Code wartet auf 2 ruhige Sekunden)

## Probleme

| Problem | Was tun |
|---|---|
| GUI zeigt «Stufe 5», obwohl ihr Stufen testen wollt | Auf dem Feather ist noch der richtige Code. `flex_stufen.ino` hochladen. |
| Stufe 1: «ADC-Wert fast 0» | Kabel 3V3 – Flex – A0 prüfen (Flex nicht angeschlossen?). |
| Stufe 1: «ADC-Wert fast 4095» | 22-kΩ-Widerstand zwischen A0 und GND fehlt, oder Kurzschluss. |
| Stufe 1: Wert springt stark, obwohl der Arm ruhig ist | Wackelkontakt am Steckbrett oder am Flex-Anschluss. |
| Stufe 2: «Zu weit» kommt nie | Referenz gesetzt? Zustand muss «Bereit» zeigen. Vielleicht ändert sich der Widerstand beim Überstrecken zu wenig. |
| Stufe 4: «DRV2605L nicht gefunden» | SDA (GPIO 2), SCL (GPIO 3), VIN, GND zum DRV2605L prüfen, dann nochmals **«Stufe 4»** drücken (sucht neu). |

## Dateien

- `flex_stufen.ino`: Ablauf, Hardware, Stufen
- `stufen_logik.h`: reine Rechnungen (Widerstand, Abweichung, Grenzwert mit Hysterese), ohne Arduino-Befehle
- `pins.h`: **Kopie** von `jointsense_flex/pins.h`. Ändert sich ein Kabel: README → `jointsense_flex/pins.h` → diese Kopie.

Das ist ein **Test-Werkzeug** und kein Produkt-Code. Es lief nicht durch die Pipeline (Anforderung, Tests, Abnahme).
