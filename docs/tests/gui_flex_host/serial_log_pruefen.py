# serial_log_pruefen.py  (REQ-008 AK-14 und AK-1, nur Tests, für die Board-Prüfung)
# Prüft eine Aufzeichnung der seriellen Ausgabe des Feather (GUI_MODUS 1, 115200 Baud) auf das Zeilenformat aus AK-14:
#   G,<grenzwertZehntelProzent>,<hystereseZehntelProzent>,<motorStaerke>
#   D,<zeitMs>,<adc>,<widerstandOhm>,<abweichungZehntelProzent>,<motor 0/1>,<zustand 0..4>
#   E,<text>                   (Text ohne Komma, höchstens 60 Zeichen)
# und auf die Datenrate (im Mittel 50 +- 2 D-Zeilen pro Sekunde, AK-1/AK-14).
#
# Woher kommt die Aufzeichnung? Ein Mensch schliesst das Board an, startet die Firmware (GUI_MODUS 1) und speichert die
# Ausgabe in eine Textdatei (z. B. mit der Aufnahme im GUI oder einem Terminalprogramm). Dieses Skript öffnet den
# seriellen Port NICHT selbst.
#
# Aufruf aus dem Hauptordner:  python docs/tests/gui_flex_host/serial_log_pruefen.py <datei.txt>   (Mac/Linux: python3)
# Exit-Code 0 = ok, 1 = Fehler, 2 = nicht geprüft (keine Datei).
import os
import re
import sys

G_MUSTER = re.compile(r"^G,(\d+),(\d+),(\d+)$")
D_MUSTER = re.compile(r"^D,(\d+),(\d+),(-?\d+),(-?\d+),([01]),([0-4])$")
E_MUSTER = re.compile(r"^E,([^,]{1,60})$")

fehler = 0
anzahl = 0


def pruefe(bedingung, text):
    global fehler, anzahl
    anzahl += 1
    if bedingung:
        print("  ok      " + text)
    else:
        fehler += 1
        print("  FEHLER  " + text)


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    if len(sys.argv) < 2 or not os.path.exists(sys.argv[1]):
        print("nicht geprüft: keine Aufzeichnung angegeben. Aufruf: python serial_log_pruefen.py <datei.txt>")
        sys.exit(2)
    with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
        zeilen = [z.rstrip("\r\n") for z in f.read().split("\n")]
    # ganz leere Zeilen am Dateiende ignorieren; die erste Zeile kann nach dem Verbinden angeschnitten sein
    while zeilen and zeilen[-1] == "":
        zeilen.pop()
    if len(zeilen) > 1:
        zeilen = zeilen[1:]

    schlecht_form = []
    d_zeilen = []
    g_zahl = 0
    for nr, z in enumerate(zeilen, start=2):
        if z.startswith("G,"):
            g_zahl += 1
            if not G_MUSTER.match(z):
                schlecht_form.append("Zeile %d: %r" % (nr, z))
        elif z.startswith("D,"):
            m = D_MUSTER.match(z)
            if m:
                d_zeilen.append(tuple(int(x) for x in m.groups()))
            else:
                schlecht_form.append("Zeile %d: %r" % (nr, z))
        elif z.startswith("E,"):
            if not E_MUSTER.match(z):
                schlecht_form.append("Zeile %d: %r" % (nr, z))
        else:
            schlecht_form.append("Zeile %d: %r (beginnt nicht mit G, D oder E)" % (nr, z))

    print("AK-14: Zeilenformat (%d Zeilen, davon %d D-Zeilen, %d G-Zeilen)" % (len(zeilen), len(d_zeilen), g_zahl))
    pruefe(not schlecht_form, "alle Zeilen beginnen mit G, / D, / E, und haben das richtige Format %s" % schlecht_form[:3])
    pruefe(g_zahl >= 1, "mindestens eine G-Zeile (einmal nach dem Start)")
    pruefe(len(d_zeilen) >= 100, "genug D-Zeilen zum Auswerten (mindestens 100)")
    if len(d_zeilen) < 2:
        print("\nAbbruch: zu wenige D-Zeilen.")
        sys.exit(1)

    unguelt = [d for d in d_zeilen if d[1] < 100 or d[1] > 3995]
    pruefe(all(d[2] == 0 and d[3] == 0 for d in unguelt), "AK-14: bei ungültigem ADC (unter 100 oder über 3995) sind Widerstand und Abweichung 0")
    pruefe(all(d[3] == 0 for d in d_zeilen if d[5] == 0), "AK-14: ohne Referenz (Zustand 0) ist die Abweichung 0")
    pruefe(all(d[1] <= 4095 for d in d_zeilen), "AK-1: ADC-Wert höchstens 4095 (12 Bit)")
    pruefe(all(d[4] == 0 for d in d_zeilen if d[5] in (0, 2, 3)), "AK-4/AK-5/AK-10: bei keine Referenz, Einmessen und Sensor-Ausfall ist der Motor aus")

    zeiten = [d[0] for d in d_zeilen]
    steigend = all(zeiten[i] < zeiten[i + 1] for i in range(len(zeiten) - 1) if zeiten[i + 1] - zeiten[i] < 2 ** 31)
    pruefe(steigend, "AK-14: die Zeit (zeitMs) steigt von Zeile zu Zeile")
    dauer_s = (zeiten[-1] - zeiten[0]) / 1000.0
    if dauer_s > 0:
        rate = (len(d_zeilen) - 1) / dauer_s
        pruefe(48.0 <= rate <= 52.0, "AK-1/AK-14: Datenrate im Mittel 50 +- 2 pro Sekunde (gemessen: %.1f pro Sekunde über %.1f s)" % (rate, dauer_s))
        abstaende = [zeiten[i + 1] - zeiten[i] for i in range(len(zeiten) - 1)]
        grosse = [a for a in abstaende if a > 40]
        pruefe(len(grosse) == 0, "AK-1: kein Messzyklus dauert länger als 40 ms (Zeitlücken gefunden: %d, grösste %d ms)" % (len(grosse), max(abstaende)))
    else:
        pruefe(False, "die Zeit läuft nicht (alle zeitMs gleich)")

    print("\n%d von %d Prüfungen bestanden" % (anzahl - fehler, anzahl))
    sys.exit(1 if fehler else 0)


main()
