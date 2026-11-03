# winkelverlauf.py
# Erzeugt typische Winkelverläufe des Ellbogens als CSV-Dateien.
# Damit kann der Tester die echte C++-Logik mit realistischen Bewegungen füttern.
#
# Wichtig: Hier wird die Logik NICHT nachgebaut. Python erzeugt nur die Eingabe-Werte.
#
# Aufruf (aus dem Hauptordner des Repos):
#   python docs/tests/szenarien/winkelverlauf.py
#
# Ergebnis: CSV-Dateien in scratch/szenarien/ mit den Spalten
#   zeit_ms, winkel_grad
# Winkel = Abweichung von der eingemessenen Referenz (0 Grad = gestreckt, plus = überstreckt).

import math
import os
import random

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
AUSGABE_ORDNER = os.path.join(HAUPTORDNER, "scratch", "szenarien")

MESS_PAUSE_MS = 20  # 50 Hz, wie im Sketch (README: 50-100 Hz)


def speichern(name, werte):
    # werte = Liste von (zeit_ms, winkel_grad)
    os.makedirs(AUSGABE_ORDNER, exist_ok=True)
    pfad = os.path.join(AUSGABE_ORDNER, name + ".csv")
    with open(pfad, "w", encoding="utf-8") as datei:
        datei.write("zeit_ms,winkel_grad\n")
        for zeit, winkel in werte:
            datei.write("%d,%.3f\n" % (zeit, winkel))
    print("geschrieben: " + pfad)


def langsam_ueberstrecken():
    # Arm geht in 3 Sekunden von -20 Grad auf +10 Grad und wieder zurück
    werte = []
    for schritt in range(0, 6000 // MESS_PAUSE_MS):
        zeit = schritt * MESS_PAUSE_MS
        anteil = zeit / 3000.0
        if anteil <= 1.0:
            winkel = -20 + 30 * anteil
        else:
            winkel = 10 - 30 * (anteil - 1.0)
        werte.append((zeit, winkel))
    return werte


def zittern_an_grenze(grenze_grad=5.0):
    # Arm bleibt nahe an der Grenze und zittert leicht (+/- 1 Grad)
    random.seed(42)  # fester Startwert: jedes Mal die gleichen Werte
    werte = []
    for schritt in range(0, 3000 // MESS_PAUSE_MS):
        zeit = schritt * MESS_PAUSE_MS
        winkel = grenze_grad + math.sin(zeit / 50.0) * 1.0 + random.uniform(-0.3, 0.3)
        werte.append((zeit, winkel))
    return werte


def kurzer_ausreisser():
    # Arm ist ruhig bei 0 Grad, nur eine einzelne Messung springt auf +15 Grad
    # (z. B. Stoss). Gute Entprellung sollte hier NICHT warnen.
    werte = []
    for schritt in range(0, 2000 // MESS_PAUSE_MS):
        zeit = schritt * MESS_PAUSE_MS
        winkel = 15.0 if schritt == 50 else 0.0
        werte.append((zeit, winkel))
    return werte


def main():
    speichern("langsam_ueberstrecken", langsam_ueberstrecken())
    speichern("zittern_an_grenze", zittern_an_grenze())
    speichern("kurzer_ausreisser", kurzer_ausreisser())


main()
