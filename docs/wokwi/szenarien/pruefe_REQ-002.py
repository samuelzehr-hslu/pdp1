# pruefe_REQ-002.py
# Startet die Wokwi-Stoerfaelle zu REQ-002 und wertet die Ausgabe des Seriellen Monitors aus.
# Voraussetzungen: wokwi-cli + WOKWI_CLI_TOKEN, Firmware kompiliert nach scratch/build (siehe docs/wokwi/README.md).
# Aufruf (aus dem Hauptordner): python docs/wokwi/szenarien/pruefe_REQ-002.py   (Mac/Linux: python3, Windows evtl. py)
# Logs liegen in scratch/wokwi-req002/. Exit-Code 0 = bestanden, 1 = Fehler, 2 = nicht geprueft.
#
# Wokwi HAT ein Gyroskop: die Regler heissen rotationX/rotationY/rotationZ (Grad/s), NICHT gyroX/gyroY (die gibt es nicht,
# Wokwi meldet dafuer keinen Fehler; so kam in Runde 1 der falsche Schluss "kein Gyroskop" zustande).
# Die Szenarien mit Bewegung (W3, W4, W5, W6 und die Vorspanne der REQ-001-Szenarien) erzeugt erzeuge_einmess_szenarien.py.
# Uebersteuerung (AK-14): Wokwi begrenzt den Rohwert auf +32767 / -32768; W1 und W5 pruefen, dass das kein IMU-Ausfall ist.
import os
import re
import shutil
import subprocess
import sys

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
LOG_ORDNER = os.path.join(HAUPTORDNER, "scratch", "wokwi-req002")
DIAGRAMM = "szenarien/diagram_kabelbruch.json"
T0 = 1000   # Zeit des Tastendrucks in den Szenarien (ms nach Start)
fehler = []


def pruefe(bedingung, text):
    print(("  ok      " if bedingung else "  FEHLER  ") + text)
    if not bedingung:
        fehler.append(text)


def lauf(name, label, timeout_ms):
    os.makedirs(LOG_ORDNER, exist_ok=True)
    rel = "scratch/wokwi-req002/" + label + ".log"
    voll = os.path.join(HAUPTORDNER, "scratch", "wokwi-req002", label + ".log")
    if os.path.exists(voll):
        os.remove(voll)
    befehl = [WOKWI, "docs/wokwi", "--diagram-file", DIAGRAMM, "--scenario", "szenarien/" + name + ".yaml",
              "--timeout", str(timeout_ms), "--serial-log-file", rel]
    try:
        r = subprocess.run(befehl, cwd=HAUPTORDNER, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=600)
        code, aus = r.returncode, (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        code, aus = 99, "wokwi-cli hing"
    zeilen = []
    if os.path.exists(voll):
        with open(voll, encoding="utf-8", errors="replace") as f:
            zeilen = f.read().replace("\r", "").split("\n")[:-1]
    return code, zeilen, aus


def zaehle(zeilen, *teile):
    return sum(1 for z in zeilen if all(t in z for t in teile))


def main():
    global WOKWI
    WOKWI = shutil.which("wokwi-cli")
    if WOKWI is None:
        print("NICHT GEPRUEFT: wokwi-cli fehlt (siehe /einrichten).")
        sys.exit(2)
    if not os.path.exists(os.path.join(HAUPTORDNER, "scratch", "build", "jointsense.ino.hex")):
        print("NICHT GEPRUEFT: Firmware fehlt. Zuerst kompilieren (siehe docs/wokwi/README.md).")
        sys.exit(2)

    print("== W1: Einschalten ohne Einmessen (AK-9), Zykluszeit (AK-15)")
    name = "req002_w1_einschalten"
    code, z, aus = lauf(name, name, 8000)
    pruefe(code == 0, "W1: Szenario komplett bestanden (Motor die ganze Zeit aus, auch bei ueberstrecktem Arm)")
    pruefe(zaehle(z, "FEHLER") == 0, "W1: keine Zeile FEHLER")
    pruefe(zaehle(z, "Jetzt Arm") == 0 and zaehle(z, "Referenz gesetzt") == 0, "W1: kein automatisches Einmessen")
    zeiten = [int(m.group(1)) for m in (re.search(r"Zyklus max: (\d+) us", x) for x in z) if m]
    pruefe(len(zeiten) >= 3 and max(zeiten) <= 15000, "W1: Zeilen 'Zyklus max' vorhanden, groesster Wert %s us (erlaubt: 15000)" % (max(zeiten) if zeiten else "-"))
    for k in range(1, 4):
        _, zu, _ = lauf(name, "w1_unten%d" % k, 1000 * k + 130)
        _, zo, _ = lauf(name, "w1_oben%d" % k, 1000 * k - 130)
        for text in ("Keine Referenz", "Zyklus max"):
            a, b = zaehle(zu, text), zaehle(zo, text)
            pruefe(a >= k, "W1: nach %d ms mindestens %d Zeile(n) '%s' (gefunden: %d)" % (1000 * k + 130, k, text, a))
            pruefe(b <= k - 1, "W1: vor %d ms hoechstens %d Zeile(n) '%s' (gefunden: %d)" % (1000 * k - 130, k - 1, text, b))

    print("== W2: Einmessen ohne Bewegung (AK-6 Ablauf, AK-7a)")
    name = "req002_w2_einmessen_ohne_bewegung"
    code, z, aus = lauf(name, name, 10000)
    pruefe(code == 0, "W2: Szenario komplett bestanden (Stoss 100 ms bei t0+2050, kein zweiter Stoss, keine Bestaetigung)")
    # Ohne Bewegung treffen (a) Beugung UND (d) Oberarm zu. AK-7 legt die Reihenfolge fest (IMU, ruhig, Beugung, Oberarm): es steht 'Beugung'.
    pruefe(zaehle(z, "Einmessen abgebrochen - Grund: Beugung") == 1 and zaehle(z, "Einmessen abgebrochen") == 1,
           "W2: genau eine Zeile 'Einmessen abgebrochen - Grund: Beugung' (Reihenfolge AK-7: Beugung vor Oberarm)")
    pruefe(zaehle(z, "Referenz gesetzt") == 0, "W2: kein 'Referenz gesetzt'")
    _, zu, _ = lauf(name, "w2_vor", T0 + 5900)
    _, zo, _ = lauf(name, "w2_nach", T0 + 6120)
    pruefe(zaehle(zu, "Einmessen abgebrochen") == 0, "W2: bis t0+5900 ms noch keine Abbruch-Zeile (gefunden: %d)" % zaehle(zu, "Einmessen abgebrochen"))
    pruefe(zaehle(zo, "Einmessen abgebrochen") == 1, "W2: bis t0+6120 ms ist die Abbruch-Zeile da (gefunden: %d)" % zaehle(zo, "Einmessen abgebrochen"))
    pruefe(zaehle(z, "Keine Referenz") >= 3, "W2: danach weiter 'Keine Referenz'")

    print("== Abbruch 'Grund: Oberarm' allein (AK-7d)")
    print("  nicht pruefbar: Wokwi liefert kein Gyroskop. Ohne Bewegung treffen immer (a) und (d) zugleich zu (siehe W2).")
    print("== W3: Arm in Phase 1 nicht ruhig (AK-7b)")
    name = "req002_w3_nicht_ruhig"
    code, z, aus = lauf(name, name, 9000)
    pruefe(code == 0, "W3: Szenario komplett bestanden (keine Bestaetigungsvibration, Motor aus)")
    pruefe(zaehle(z, "Einmessen abgebrochen - Grund: ruhig") == 1, "W3: genau eine Zeile 'Einmessen abgebrochen - Grund: ruhig'")
    pruefe(zaehle(z, "Referenz gesetzt") == 0, "W3: kein 'Referenz gesetzt'")
    pruefe(zaehle(z, "FEHLER") == 0, "W3: keine Zeile FEHLER (kein IMU-Ausfall)")
    _, zo, _ = lauf(name, "w3_nach", T0 + 6120)
    pruefe(zaehle(zo, "Einmessen abgebrochen") == 1, "W3: bis t0+6120 ms ist die Abbruch-Zeile da (gefunden: %d)" % zaehle(zo, "Einmessen abgebrochen"))

    print("== W6: Oberarm haengt in Phase 2 still (AK-7d)")
    name = "req002_w6_oberarm_still"
    code, z, aus = lauf(name, name, 9000)
    pruefe(code == 0, "W6: Szenario komplett bestanden (keine Bestaetigungsvibration, Motor aus)")
    pruefe(zaehle(z, "Einmessen abgebrochen - Grund: Oberarm") == 1, "W6: genau eine Zeile 'Einmessen abgebrochen - Grund: Oberarm'")
    pruefe(zaehle(z, "Einmessen abgebrochen") == 1, "W6: es gibt keine andere Abbruch-Zeile (Beugung 70 Grad und Ruhe in Phase 1 sind in Ordnung)")
    pruefe(zaehle(z, "Referenz gesetzt") == 0, "W6: kein 'Referenz gesetzt'")
    pruefe(zaehle(z, "FEHLER") == 0, "W6: keine Zeile FEHLER (kein IMU-Ausfall)")
    _, zo, _ = lauf(name, "w6_nach", T0 + 6120)
    pruefe(zaehle(zo, "Einmessen abgebrochen") == 1, "W6: bis t0+6120 ms ist die Abbruch-Zeile da (gefunden: %d)" % zaehle(zo, "Einmessen abgebrochen"))

    print("== W4: IMU 2 faellt in Phase 2 aus (AK-7c, REQ-001)")
    name = "req002_w4_imu_ausfall_in_phase2"
    code, z, aus = lauf(name, name, 10000)
    pruefe(code == 0, "W4: Szenario komplett bestanden (Motor nach Abbruch aus, keine Bestaetigung, kein Alarm nach Wiederkehr)")
    pruefe(zaehle(z, "Einmessen abgebrochen - Grund: IMU") == 1, "W4: genau eine Zeile 'Einmessen abgebrochen - Grund: IMU'")
    pruefe(zaehle(z, "FEHLER", "IMU 2 (Unterarm)") >= 1, "W4: FEHLER mit 'IMU 2 (Unterarm)' (REQ-001)")
    pruefe(zaehle(z, "wieder da") >= 1, "W4: 'wieder da' nach Wiederkehr")
    pruefe(zaehle(z, "Referenz gesetzt") == 0, "W4: kein 'Referenz gesetzt'")

    print("== W5: erfolgreiches Einmessen, Bestaetigung, Warnung, Uebersteuerung (AK-6, AK-9, AK-14)")
    name = "req002_w5_einmessen_ok"
    code, z, aus = lauf(name, name, 11000)
    pruefe(code == 0, "W5: Szenario komplett bestanden (Stoss bei t0+2050, Bestaetigung bei t0+6075, Warnung bei +30 Grad, Warnung bleibt bei Uebersteuerung)")
    pruefe(zaehle(z, "Einmessen abgebrochen") == 0, "W5: kein 'Einmessen abgebrochen' (das Einmessen hat geklappt)")
    pruefe(zaehle(z, "FEHLER") == 0 and zaehle(z, "wieder da") == 0, "W5: Uebersteuerung (+32767 / -32768) ist KEIN IMU-Ausfall (keine Zeile FEHLER, kein 'wieder da')")
    print("  AK-13 (Ausfall nach erfolgreichem Einmessen, Lage wieder bekannt in hoechstens 5 s): wird in ak04 und ak10_einmessen_bei_ausfall")
    print("  (pruefe_REQ-001.py) geprueft, weil dort die Wiederkehr schon eingebaut ist und W5 sonst laenger als 10 s dauern wuerde.")
    print("== Uebersteuerung ohne Referenz: siehe W1 (rotationX = -1000 und +1000)")

    print()
    print("Pruefungen mit Fehler: %d" % len(fehler))
    for f in fehler:
        print("  - " + f)
    sys.exit(1 if fehler else 0)


main()
