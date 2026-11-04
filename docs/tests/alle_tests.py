# alle_tests.py
# Startet alle Logik-Tests in docs/tests/logik/.
#
# Aufruf (aus dem Hauptordner des Repos):
#   python docs/tests/alle_tests.py     (Mac/Linux: python3 docs/tests/alle_tests.py)
#
# Was passiert:
#   1. Jede Datei docs/tests/logik/test_*.cpp wird mit g++ (oder clang++) übersetzt.
#      Alle Warnungen zählen als Fehler (-Wall -Wextra -Werror).
#      Der Ordner jointsense/ wird eingebunden, damit "#include \"logik.h\"" klappt,
#      und jointsense/logik.cpp wird mit übersetzt (dort steckt der Inhalt der Funktionen).
#      Vorher werden die Python-Szenarien (docs/tests/szenarien/*.py) gestartet. Sie legen
#      CSV-Dateien in scratch/szenarien/ ab, die manche Tests einlesen.
#   2. Das fertige Testprogramm landet in scratch/tests/ (nicht im Repo) und wird gestartet.
#   3. Am Schluss: Zusammenfassung. Exit-Code 0 = alles ok, 1 = mindestens ein Fehler.

import glob
import os
import shutil
import subprocess
import sys

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TEST_ORDNER = os.path.join(HAUPTORDNER, "docs", "tests", "logik")
CODE_ORDNER = os.path.join(HAUPTORDNER, "jointsense")
AUSGABE_ORDNER = os.path.join(HAUPTORDNER, "scratch", "tests")


def main():
    # Umlaute auch unter Windows richtig anzeigen
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    # C++-Compiler suchen. Je nach PC heisst er anders:
    #   Windows (Intel/AMD): g++     Windows ARM (MSYS2 clangarm64): clang++
    #   Mac: g++ oder clang++ (beides ist dort derselbe Compiler)     Linux: g++
    compiler = None
    for name in ("g++", "clang++", "c++"):
        if shutil.which(name):
            compiler = name
            break
    if compiler is None:
        print("Kein C++-Compiler gefunden (g++ oder clang++). Mit /einrichten prüfen, wie man ihn installiert.")
        sys.exit(1)
    print("Compiler: " + compiler)

    os.makedirs(AUSGABE_ORDNER, exist_ok=True)

    # Szenarien erzeugen (Python-Skripte in docs/tests/szenarien/ schreiben CSV-Dateien nach
    # scratch/szenarien/). Manche Logik-Tests lesen diese Dateien.
    for skript in sorted(glob.glob(os.path.join(HAUPTORDNER, "docs", "tests", "szenarien", "*.py"))):
        print("== Szenarien erzeugen: " + os.path.basename(skript))
        erzeugt = subprocess.run([sys.executable, skript], cwd=HAUPTORDNER, stdout=subprocess.DEVNULL)
        if erzeugt.returncode != 0:
            print("  FEHLER  Szenario-Skript fehlgeschlagen: " + os.path.basename(skript))
            sys.exit(1)

    testdateien = sorted(glob.glob(os.path.join(TEST_ORDNER, "test_*.cpp")))

    if not testdateien:
        print("Keine Testdateien gefunden (docs/tests/logik/test_*.cpp).")
        sys.exit(0)

    fehlgeschlagen = []

    for datei in testdateien:
        name = os.path.splitext(os.path.basename(datei))[0]
        # Auf Windows brauchen Programme die Endung .exe, auf Mac/Linux nicht
        endung = ".exe" if os.name == "nt" else ""
        programm = os.path.join(AUSGABE_ORDNER, name + endung)
        print("== " + name)

        # Schritt 1: Übersetzen
        befehl = [
            compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", CODE_ORDNER, "-I", TEST_ORDNER,
            datei,
        ] + glob.glob(os.path.join(CODE_ORDNER, "*.cpp")) + ["-o", programm]
        uebersetzt = subprocess.run(befehl)
        if uebersetzt.returncode != 0:
            print("  FEHLER  Übersetzen fehlgeschlagen")
            fehlgeschlagen.append(name)
            continue

        # Schritt 2: Starten (höchstens 60 Sekunden, falls etwas hängt)
        try:
            gelaufen = subprocess.run([programm], timeout=60, cwd=HAUPTORDNER)
        except subprocess.TimeoutExpired:
            print("  FEHLER  Test hängt (länger als 60 s)")
            fehlgeschlagen.append(name)
            continue
        if gelaufen.returncode != 0:
            fehlgeschlagen.append(name)

    print()
    print("Testdateien: %d, fehlgeschlagen: %d" % (len(testdateien), len(fehlgeschlagen)))
    for name in fehlgeschlagen:
        print("  - " + name)
    sys.exit(1 if fehlgeschlagen else 0)


main()
