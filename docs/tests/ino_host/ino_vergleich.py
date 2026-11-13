# ino_vergleich.py  (REQ-007, nur Tests)
#
# Zweck: Beweisen, dass das Aufräumen die Ausgabe der Firmware NICHT verändert (AK-3).
# Das Skript übersetzt jointsense.ino mit einer Arduino-Attrappe auf dem PC (clang++ oder g++), spielt feste Szenarien ab
# (Tasten, Enter, Kabelbruch, Überstreckung, Einmessen ...) und schreibt ALLES, was die Firmware tut, mit Zeit in Dateien:
#   <name>_m<modus>.txt       Serielle Zeilen (S), Motor/Pin-Änderungen (AW/DW/PM), Baudrate (SB). Das ist der Massstab für AK-3.
#   <name>_m<modus>.wire.txt  alle I2C-Zugriffe (nur als Hinweis, nicht streng)
# Modus 1 = GUI_MODUS 1, Modus 0 = GUI_MODUS 0 (das Skript ändert den Schalter nur in einer Kopie unter scratch/).
#
# Aufrufe (aus dem Hauptordner, Python-Name wie bei /einrichten):
#   python docs/tests/ino_host/ino_vergleich.py lauf <sketch-ordner> <ausgabe-ordner>
#       Beispiel Ausgangslage: lauf scratch/REQ-007-ausgangslage/referenz scratch/REQ-007-ausgangslage/ino_host
#       Beispiel nachher:      lauf jointsense scratch/REQ-007-nachher/ino_host
#   python docs/tests/ino_host/ino_vergleich.py vergleich <ordner-vorher> <ordner-nachher>
#   python docs/tests/ino_host/ino_vergleich.py pruefen
#       = lauf mit jointsense/ nach scratch/REQ-007-nachher/ino_host und Vergleich mit der Ausgangslage
# Exit-Code: 0 = alle Dateien gleich, 1 = Unterschied, 2 = Werkzeug fehlt oder Übersetzen schlug fehl.

import glob
import os
import re
import shutil
import subprocess
import sys

HAUPT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HIER = os.path.dirname(os.path.abspath(__file__))
TESTS_LOGIK = os.path.join(HAUPT, "docs", "tests", "logik")
SZENARIEN = ["normal", "abbruch", "ohne_imu2"]
MODI = [1, 0]
AUSGANGSLAGE = os.path.join(HAUPT, "scratch", "REQ-007-ausgangslage", "ino_host")
NACHHER = os.path.join(HAUPT, "scratch", "REQ-007-nachher", "ino_host")


def compiler_suchen():
    for name in ("g++", "clang++", "c++"):
        if shutil.which(name):
            return name
    return None


def prototypen(text):
    # Wie Arduino: vor dem ersten Gebrauch muss jede Funktion bekannt sein. Wir lesen alle Funktionsköpfe (Spalte 0),
    # auch wenn der Kopf über mehrere Zeilen geht.
    muster = re.compile(r"^(?!\s)(?!else|if|for|while|switch|return|do)([A-Za-z_][\w 	:<>\*&]*?[\s\*&])(\w+)\s*\(([^;{}()]*)\)\s*\{", re.M)
    gesehen = []
    for m in muster.finditer(text):
        if m.group(2) == "main":
            continue
        args = re.sub(r"\s+", " ", m.group(3)).strip()
        kopf = m.group(1).strip() + " " + m.group(2) + "(" + args + ");"
        if kopf not in gesehen:
            gesehen.append(kopf)
    return gesehen


def vorbereiten(sketch, modus, arbeit):
    # Kopie des Sketch-Ordners, GUI_MODUS gesetzt, alle .ino zusammengefügt (wie Arduino), Prototypen ergänzt.
    if os.path.exists(arbeit):
        shutil.rmtree(arbeit)
    os.makedirs(arbeit)
    for datei in glob.glob(os.path.join(sketch, "*")):
        if os.path.isfile(datei):
            shutil.copy(datei, arbeit)
    ersetzt = 0
    for datei in glob.glob(os.path.join(arbeit, "*")):
        if not datei.endswith((".ino", ".h", ".cpp")):
            continue
        with open(datei, encoding="utf-8") as f:
            t = f.read()
        neu, n = re.subn(r"^(\s*#define\s+GUI_MODUS\s+)\d+", r"\g<1>" + str(modus), t, flags=re.M)
        if n:
            ersetzt += n
            with open(datei, "w", encoding="utf-8", newline="\n") as f:
                f.write(neu)
    if ersetzt == 0:
        print("FEHLER: '#define GUI_MODUS <zahl>' nicht gefunden")
        return False
    inos = sorted(glob.glob(os.path.join(arbeit, "*.ino")))
    if not inos:
        print("FEHLER: keine .ino-Datei im Sketch-Ordner " + sketch)
        return False
    # Arduino nimmt die Datei, die so heisst wie der Ordner, zuerst. Wir nehmen jointsense.ino zuerst, dann die anderen.
    erste = [p for p in inos if os.path.basename(p) == "jointsense.ino"]
    rest = [p for p in inos if p not in erste]
    text = ""
    for p in erste + rest:
        with open(p, encoding="utf-8") as f:
            text += f.read() + "\n"
    protos = prototypen(text)
    zeilen = text.split("\n")
    letzteInclude = -1
    for i, z in enumerate(zeilen):
        if re.match(r"\s*#\s*include", z):
            letzteInclude = i
    zeilen[letzteInclude + 1:letzteInclude + 1] = ["", "// --- automatisch erzeugte Prototypen (wie bei Arduino) ---"] + protos + [""]
    with open(os.path.join(arbeit, "ino_zusammen.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#include "arduino_attrappe.h"\n' + "\n".join(zeilen))
    return True


def lauf(sketch, aus):
    compiler = compiler_suchen()
    if compiler is None:
        print("Kein C++-Compiler (g++/clang++). Siehe /einrichten.  -> nicht geprüft")
        return 2
    sketch = os.path.abspath(sketch)
    aus = os.path.abspath(aus)
    os.makedirs(aus, exist_ok=True)
    for f in glob.glob(os.path.join(aus, "*.txt")):
        os.remove(f)
    endung = ".exe" if os.name == "nt" else ""
    gut = True
    for modus in MODI:
        arbeit = os.path.join(HAUPT, "scratch", "ino_host_arbeit", "modus%d" % modus)
        if not vorbereiten(sketch, modus, arbeit):
            return 2
        programm = os.path.join(arbeit, "ino_lauf" + endung)
        quellen = [os.path.join(HIER, "ino_lauf.cpp"), os.path.join(HIER, "arduino_attrappe.cpp"), os.path.join(HIER, "sensor_tabelle.cpp")]
        quellen += [p for p in glob.glob(os.path.join(arbeit, "*.cpp"))]
        befehl = [compiler, "-std=c++11", "-w", "-O0", "-I", arbeit, "-I", HIER, "-I", TESTS_LOGIK] + quellen + ["-o", programm]
        r = subprocess.run(befehl, capture_output=True, text=True)
        if r.returncode != 0:
            print("FEHLER beim Übersetzen (Modus %d):" % modus)
            print("\n".join(r.stderr.splitlines()[:15]))
            return 2
        for name in SZENARIEN:
            a = os.path.join(aus, "%s_m%d.txt" % (name, modus))
            w = os.path.join(aus, "%s_m%d.wire.txt" % (name, modus))
            r = subprocess.run([programm, name, a, w], capture_output=True, text=True, timeout=300)
            if r.returncode != 0:
                print("FEHLER beim Ausführen (%s, Modus %d): %s" % (name, modus, r.stderr.strip()[:200]))
                gut = False
            else:
                with open(a, encoding="utf-8") as f:
                    n = sum(1 for _ in f)
                print("ok  %-10s Modus %d: %d Ausgabezeilen -> %s" % (name, modus, n, a))
    return 0 if gut else 2


def lesen(pfad):
    with open(pfad, encoding="utf-8", errors="replace") as f:
        return f.read().splitlines()


def vergleich(vorher, nachher):
    unterschied = 0
    for modus in MODI:
        for name in SZENARIEN:
            for endung, streng in (("", True), (".wire", False)):
                datei = "%s_m%d%s.txt" % (name, modus, endung)
                pa = os.path.join(vorher, datei)
                pb = os.path.join(nachher, datei)
                if not os.path.exists(pa) or not os.path.exists(pb):
                    print("FEHLT  " + datei)
                    unterschied += 1 if streng else 0
                    continue
                a = lesen(pa)
                b = lesen(pb)
                if a == b:
                    print("gleich %-32s (%d Zeilen)%s" % (datei, len(a), "" if streng else "  [nur Hinweis]"))
                    continue
                erste = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
                print("%s %s: erster Unterschied in Zeile %d (vorher %d, nachher %d Zeilen)%s" % (
                    "UNTERSCHIED" if streng else "Hinweis    ", datei, erste + 1, len(a), len(b), "" if streng else "  [nur Hinweis]"))
                print("   vorher : " + (a[erste] if erste < len(a) else "(Ende)"))
                print("   nachher: " + (b[erste] if erste < len(b) else "(Ende)"))
                if streng:
                    unterschied += 1
    print()
    print("Ergebnis: " + ("ALLES GLEICH" if unterschied == 0 else "%d Datei(en) mit Unterschied" % unterschied))
    return 0 if unterschied == 0 else 1


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    if len(sys.argv) >= 4 and sys.argv[1] == "lauf":
        sys.exit(lauf(sys.argv[2], sys.argv[3]))
    if len(sys.argv) >= 4 and sys.argv[1] == "vergleich":
        sys.exit(vergleich(sys.argv[2], sys.argv[3]))
    if len(sys.argv) >= 2 and sys.argv[1] == "pruefen":
        rc = lauf(os.path.join(HAUPT, "jointsense"), NACHHER)
        if rc != 0:
            sys.exit(rc)
        sys.exit(vergleich(AUSGANGSLAGE, NACHHER))
    print(__doc__ if __doc__ else "Aufruf siehe Kopf der Datei")
    sys.exit(2)


main()
