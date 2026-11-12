# verdrahtung_REQ-003.py
# Liest die Verdrahtung der Motor-LED aus den Dateien und schreibt sie als einfache Liste auf.
# Gehört zu REQ-003 (Motor-LED als Platzhalter), Kriterien AK-3 und AK-4.
#
# Wichtig: Hier wird nichts bewertet. Python liest nur die Tatsachen aus
#   - jointsense/diagram.json                      (Schaltung der Simulation)
#   - docs/wokwi/szenarien/diagram_kabelbruch.json (Kopie der Schaltung für die REQ-001-Störfälle)
#   - README.md                                    (Verdrahtungstabelle, Zeile "Treiber IN1")
# Bewertet wird im C++-Test docs/tests/logik/test_REQ-003.cpp (dort wird auch die echte pins.h benutzt).
#
# Aufruf (aus dem Hauptordner des Repos):
#   python3 docs/tests/szenarien/verdrahtung_REQ-003.py     (Windows: python oder py)
# (docs/tests/alle_tests.py startet dieses Skript automatisch vor den C++-Tests.)
#
# Ergebnis: scratch/szenarien/verdrahtung_REQ-003.csv, jede Zeile "schluessel,wert".
# Das Skript endet immer mit Exit-Code 0, damit alle_tests.py nicht abbricht.
# Kann etwas nicht gelesen werden, steht "fehlt" als Wert, und der C++-Test meldet dann einen Fehler.

import json
import os
import re

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
AUSGABE = os.path.join(HAUPTORDNER, "scratch", "szenarien", "verdrahtung_REQ-003.csv")


def ohm(text):
    # "220" -> "220", "1k" -> "1000", "220Ω" -> "220". Unlesbar -> "fehlt".
    m = re.match(r"^\s*([0-9.]+)\s*([kKmM]?)\s*", str(text))
    if not m:
        return "fehlt"
    faktor = {"": 1, "k": 1000, "K": 1000, "m": 1000000, "M": 1000000}[m.group(2)]
    return str(int(round(float(m.group(1)) * faktor)))


def lese_schaltung(pfad, vorne, fakten):
    # Schreibt die Tatsachen zur Motor-LED mit dem Vorsatz "vorne" in die Liste "fakten".
    try:
        with open(pfad, encoding="utf-8") as f:
            schaltung = json.load(f)
        teile = {t["id"]: t for t in schaltung["parts"]}
        kabel = schaltung["connections"]
    except Exception as e:
        fakten[vorne + "datei"] = "fehlt (" + type(e).__name__ + ")"
        return
    fakten[vorne + "datei"] = "ok"

    # Nachbarn: jedes Kabel verbindet zwei Anschlüsse ("teil:pin")
    nachbarn = {}
    for k in kabel:
        a, b = k[0], k[1]
        nachbarn.setdefault(a, set()).add(b)
        nachbarn.setdefault(b, set()).add(a)

    def teil_von(anschluss):
        return anschluss.split(":")[0]

    def typ_von(anschluss):
        t = teile.get(teil_von(anschluss))
        return t["type"] if t else "unbekannt"

    def verfolge(start):
        # Folgt den Kabeln vom Start aus. Durch einen Widerstand geht es hindurch (Pin 1 <-> Pin 2).
        # Der Arduino und alle anderen Bauteile sind Endpunkte (dahinter wird nicht weiter gesucht).
        arduino, fremde, widerstaende = set(), set(), []
        gesehen = {start}
        offen = [start]
        while offen:
            jetzt = offen.pop()
            for n in nachbarn.get(jetzt, ()):
                if n in gesehen:
                    continue
                gesehen.add(n)
                t = typ_von(n)
                if t == "wokwi-arduino-uno":
                    pin = n.split(":", 1)[1]
                    arduino.add("GND" if pin.startswith("GND") else pin.lstrip("D"))
                elif t == "wokwi-resistor":
                    widerstaende.append(ohm(teile[teil_von(n)].get("attrs", {}).get("value", "")))
                    anderer = teil_von(n) + (":2" if n.endswith(":1") else ":1")
                    if anderer not in gesehen:
                        gesehen.add(anderer)
                        offen.append(anderer)
                else:
                    fremde.add(t + "@" + n)
        return arduino, fremde, widerstaende

    # Die Motor-LED(s): LED-Bauteile mit "Motor" im Namen
    leds = [i for i, t in teile.items()
            if t["type"] == "wokwi-led" and "motor" in str(t.get("attrs", {}).get("label", "")).lower()]
    fakten[vorne + "led_anzahl"] = str(len(leds))
    if len(leds) != 1:
        return
    led = leds[0]
    fakten[vorne + "led_farbe"] = str(teile[led].get("attrs", {}).get("color", "fehlt"))

    for seite, name in (("A", "anode_"), ("C", "kathode_")):
        arduino, fremde, widerstaende = verfolge(led + ":" + seite)
        fakten[vorne + name + "arduino_pins"] = ";".join(sorted(arduino)) if arduino else "keine"
        fakten[vorne + name + "fremde_bauteile"] = ";".join(sorted(fremde)) if fremde else "keine"
        fakten[vorne + name + "widerstaende_ohm"] = ";".join(widerstaende) if widerstaende else "keine"


def lese_readme(fakten):
    # In der Verdrahtungstabelle: in welcher Zeile steht "IN1", und welcher Pin (D...) steht davor?
    fakten["readme_in1_pin"] = "fehlt"
    try:
        with open(os.path.join(HAUPTORDNER, "README.md"), encoding="utf-8") as f:
            for zeile in f:
                if not zeile.lstrip().startswith("|"):
                    continue
                zellen = [z.strip() for z in zeile.strip().strip("|").split("|")]
                if len(zellen) >= 2 and "IN1" in zellen[1]:
                    m = re.match(r"^D(\d+)", zellen[0])
                    if m:
                        fakten["readme_in1_pin"] = m.group(1)
    except Exception:
        pass


def main():
    fakten = {}
    lese_schaltung(os.path.join(HAUPTORDNER, "jointsense", "diagram.json"), "diagramm_", fakten)
    lese_schaltung(os.path.join(HAUPTORDNER, "docs", "wokwi", "szenarien", "diagram_kabelbruch.json"),
                   "kopie_", fakten)
    lese_readme(fakten)
    os.makedirs(os.path.dirname(AUSGABE), exist_ok=True)
    with open(AUSGABE, "w", encoding="utf-8", newline="\n") as f:
        for schluessel in sorted(fakten):
            f.write(schluessel + "," + fakten[schluessel] + "\n")
    print("Verdrahtung REQ-003 geschrieben: " + AUSGABE)


main()
