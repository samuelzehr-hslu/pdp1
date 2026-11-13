# refactoring_pruefen.py  (REQ-007, nur Tests)
# Prüft die mechanisch prüfbaren Punkte von REQ-007 gegen die Ausgangslage in scratch/REQ-007-ausgangslage/:
#   AK-2  logik.h: gleiche Inhalte (Kommentare und Leerräume werden ignoriert)
#   AK-4  Kompilieren in beiden Modi ohne Warnungen, cppcheck ohne Befunde
#   AK-5  Flash und RAM in beiden Modi nicht grösser als die Ausgangslage
#   AK-7  pins.h unverändert, keine Pin-Nummern / I2C-Adressen als Zahl im übrigen Code (Suchmuster)
#   AK-9  keine Funktion länger als 80 Zeilen (ohne Kommentare und Leerzeilen) in jointsense/
# Aufruf aus dem Hauptordner:  python docs/tests/refactoring_pruefen.py   (Mac/Linux: python3)
# Exit-Code: 0 = alles bestanden, 1 = mindestens ein Punkt nicht bestanden, 2 = Werkzeug fehlt (nicht geprüft)
# Ergebnisse und Kopien liegen unter scratch/REQ-007-nachher/ (nicht im Repo).

import glob
import os
import re
import shutil
import subprocess
import sys

HAUPT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CODE = os.path.join(HAUPT, "jointsense")
VOR = os.path.join(HAUPT, "scratch", "REQ-007-ausgangslage")
NACH = os.path.join(HAUPT, "scratch", "REQ-007-nachher")
GRENZE_FLASH = {"gui": 31836, "text": 32252}
GRENZE_RAM = {"gui": 1339, "text": 1322}
MAX_ZEILEN = 80

ergebnis = {}


def melde(ak, ok, text):
    ergebnis[ak] = ergebnis.get(ak, True) and ok
    print("  %-6s %s  %s" % ("ok" if ok else "FEHLER", ak, text))


def ohne_kommentare(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def lesen(pfad):
    with open(pfad, encoding="utf-8") as f:
        return f.read()


def ak2():
    print("AK-2 Schnittstelle logik.h")
    a = re.sub(r"\s+", " ", ohne_kommentare(lesen(os.path.join(VOR, "referenz", "logik.h")))).strip()
    b = re.sub(r"\s+", " ", ohne_kommentare(lesen(os.path.join(CODE, "logik.h")))).strip()
    melde("AK-2", a == b, "logik.h ohne Kommentare gleich wie in der Ausgangslage" if a == b else "logik.h hat sich (ausser Kommentaren) verändert")
    # Zusatz: alle öffentlichen Funktionen aus logik.cpp müssen mit Nicht-static-Definition noch da sein
    def oeffentlich(pfad):
        t = ohne_kommentare(lesen(pfad))
        funde = re.findall(r"^(?!\s)(?!#)([A-Za-z_][\w \*&:<>]*?[\s\*&])(\w+)\s*\([^;{}]*\)\s*\{", t, flags=re.M)
        return sorted(set(n for kopf, n in funde if not re.search(r"static", kopf)))
    fa = oeffentlich(os.path.join(VOR, "referenz", "logik.cpp"))
    fb = oeffentlich(os.path.join(CODE, "logik.cpp"))
    neu = [x for x in fb if x not in fa]
    weg = [x for x in fa if x not in fb]
    melde("AK-2", not neu and not weg, "Nicht-static-Funktionen in logik.cpp gleich" if not neu and not weg else "neu: %s, weg: %s" % (neu, weg))


def kompilieren(name, modus):
    arbeit = os.path.join(NACH, name, "jointsense")
    if os.path.exists(os.path.dirname(arbeit)):
        shutil.rmtree(os.path.dirname(arbeit))
    os.makedirs(arbeit)
    for d in glob.glob(os.path.join(CODE, "*")):
        if os.path.isfile(d):
            shutil.copy(d, arbeit)
    for d in glob.glob(os.path.join(arbeit, "*")):
        if d.endswith((".ino", ".h", ".cpp")):
            t = lesen(d)
            n = re.sub(r"^(\s*#define\s+GUI_MODUS\s+)\d+", r"\g<1>%d" % modus, t, flags=re.M)
            if n != t:
                with open(d, "w", encoding="utf-8", newline="\n") as f:
                    f.write(n)
    r = subprocess.run(["arduino-cli", "compile", "--fqbn", "arduino:avr:uno", "--warnings", "all", "--output-dir",
                        os.path.join(NACH, "build_" + name), arbeit], capture_output=True, text=True)
    with open(os.path.join(NACH, "compile_%s.txt" % name), "w", encoding="utf-8") as f:
        f.write(r.stdout + r.stderr)
    return r


def ak4_ak5():
    print("AK-4 / AK-5 Kompilieren, Warnungen, Speicher")
    if shutil.which("arduino-cli") is None:
        melde("AK-4", False, "arduino-cli fehlt: nicht geprüft (/einrichten)")
        ergebnis["NICHT_GEPRUEFT"] = True
        return
    for name, modus in (("gui", 1), ("text", 0)):
        r = kompilieren(name, modus)
        voll = r.stdout + r.stderr
        warnungen = len(re.findall(r"warning:", voll))
        melde("AK-4", r.returncode == 0 and warnungen == 0, "Modus %d: Übersetzen %s, Warnungen: %d" % (modus, "ok" if r.returncode == 0 else "FEHLGESCHLAGEN", warnungen))
        m1 = re.search(r"Sketch uses (\d+) bytes", voll)
        m2 = re.search(r"Global variables use (\d+) bytes", voll)
        if m1 and m2:
            flash, ram = int(m1.group(1)), int(m2.group(1))
            melde("AK-5", flash <= GRENZE_FLASH[name] and ram <= GRENZE_RAM[name],
                  "Modus %d: Flash %d (Grenze %d, %+d), RAM %d (Grenze %d, %+d)" % (modus, flash, GRENZE_FLASH[name], flash - GRENZE_FLASH[name], ram, GRENZE_RAM[name], ram - GRENZE_RAM[name]))
        else:
            melde("AK-5", False, "Modus %d: keine Speicherangaben in der Ausgabe" % modus)
    if shutil.which("cppcheck") is None:
        melde("AK-4", False, "cppcheck fehlt: nicht geprüft (/einrichten)")
        ergebnis["NICHT_GEPRUEFT"] = True
        return
    r = subprocess.run(["cppcheck", "--enable=warning,style,performance,portability", "--std=c++11", "--language=c++",
                        os.path.join("jointsense", "jointsense.ino"), os.path.join("jointsense", "logik.cpp")],
                       capture_output=True, text=True, cwd=HAUPT)
    voll = r.stdout + r.stderr
    with open(os.path.join(NACH, "cppcheck.txt"), "w", encoding="utf-8") as f:
        f.write(voll)
    befunde = [z for z in voll.splitlines() if re.search(r"\((error|warning|style|performance|portability)\)", z)]
    melde("AK-4", len(befunde) == 0, "cppcheck: %d Befunde (Ausgangslage: 0)" % len(befunde))
    for z in befunde[:10]:
        print("         " + z)


def ak7():
    print("AK-7 Verdrahtung")
    a = lesen(os.path.join(VOR, "referenz", "pins.h"))
    b = lesen(os.path.join(CODE, "pins.h"))
    melde("AK-7", a == b, "pins.h unverändert" if a == b else "pins.h hat sich verändert")
    d1 = lesen(os.path.join(VOR, "referenz", "diagram.json"))
    d2 = lesen(os.path.join(CODE, "diagram.json"))
    melde("AK-7", d1 == d2, "diagram.json unverändert" if d1 == d2 else "diagram.json hat sich verändert")
    verdaechtig = []
    for d in sorted(glob.glob(os.path.join(CODE, "*"))):
        if not d.endswith((".ino", ".cpp", ".h")) or os.path.basename(d) == "pins.h":
            continue
        t = ohne_kommentare(lesen(d))
        for nr, z in enumerate(t.splitlines(), 1):
            if re.search(r"\b(pinMode|digitalWrite|digitalRead|analogWrite|analogRead|attachInterrupt)\s*\(\s*A?\d", z) or \
               re.search(r"\b(beginTransmission|requestFrom)\s*\(\s*(\(uint8_t\)\s*)?0x", z) or re.search(r"\b0x6[89]\b", z):
                verdaechtig.append("%s:%d: %s" % (os.path.basename(d), nr, z.strip()))
    melde("AK-7", not verdaechtig, "keine Pin-Nummern / I2C-Adressen als Zahl gefunden (Suchmuster, Rest bei Code-Durchsicht)" if not verdaechtig else "verdächtig: %d Stellen" % len(verdaechtig))
    for z in verdaechtig[:10]:
        print("         " + z)


def funktionslaengen(pfad):
    # Zählt Zeilen (ohne Kommentare und Leerzeilen) von Funktionen, deren Kopf in Spalte 0 steht und mit '{' endet.
    t = ohne_kommentare(lesen(pfad))
    zeilen = t.split("\n")
    kopf = re.compile(r"^(?!\s)(?!else|if|for|while|switch|do\b|#)([A-Za-z_][\w \t:<>\*&]*?[\s\*&])(\w+)\s*\(([^;{}]*)\)\s*(const)?\s*\{")
    ergebnisse = []
    i = 0
    while i < len(zeilen):
        m = kopf.match(zeilen[i])
        if not m:
            i += 1
            continue
        tiefe = 0
        n = 0
        start = i
        while i < len(zeilen):
            z = re.sub(r"\"(\\.|[^\"\\])*\"|'(\\.|[^'\\])*'", "", zeilen[i])
            tiefe += z.count("{") - z.count("}")
            if z.strip():
                n += 1
            i += 1
            if tiefe <= 0:
                break
        ergebnisse.append((m.group(2), n, start + 1))
    return ergebnisse


def ak9():
    print("AK-9 Funktionslänge (höchstens %d Zeilen ohne Kommentare/Leerzeilen)" % MAX_ZEILEN)
    zu_lang = []
    laengste = ("", 0, "")
    for d in sorted(glob.glob(os.path.join(CODE, "*"))):
        if not d.endswith((".ino", ".cpp", ".h")):
            continue
        for name, n, start in funktionslaengen(d):
            if n > laengste[1]:
                laengste = (name, n, os.path.basename(d))
            if n > MAX_ZEILEN:
                zu_lang.append("%s (%s ab Zeile %d): %d Zeilen" % (name, os.path.basename(d), start, n))
    melde("AK-9", not zu_lang, "längste Funktion: %s (%s) mit %d Zeilen" % (laengste[0], laengste[2], laengste[1]))
    for z in zu_lang:
        print("         zu lang: " + z)
    print("         (Kommentare auf Deutsch und aktuell: nur per Code-Durchsicht prüfbar)")


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    os.makedirs(NACH, exist_ok=True)
    if not os.path.exists(os.path.join(VOR, "referenz", "logik.h")):
        print("Ausgangslage fehlt (scratch/REQ-007-ausgangslage/referenz/). Nicht geprüft.")
        sys.exit(2)
    ak2()
    ak4_ak5()
    ak7()
    ak9()
    print()
    schlecht = [k for k, v in ergebnis.items() if k.startswith("AK") and not v]
    print("Ergebnis: " + ("alles bestanden" if not schlecht else "NICHT bestanden: " + ", ".join(sorted(schlecht))))
    sys.exit(2 if ergebnis.get("NICHT_GEPRUEFT") else (1 if schlecht else 0))


main()
