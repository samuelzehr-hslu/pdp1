# alle_tests.py
# Startet alle Logik-Tests: Prototyp 1 (docs/tests/logik/) und Prototyp 2 (docs/tests/logik_flex/).
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
#   3. Danach der zweite Durchlauf für Prototyp 2 (Flex, REQ-008): jede Datei
#      docs/tests/logik_flex/test_*.cpp wird zusammen mit jointsense_flex/*.cpp übersetzt
#      (Include-Ordner jointsense_flex/). Die Programme heissen dort flex_<name> in scratch/tests/,
#      damit nichts mit Prototyp 1 kollidiert. Fehlt der Ordner jointsense_flex/ noch, steht
#      "übersprungen, Ordner fehlt" da, und das Skript läuft normal weiter.
#   4. Am Schluss: Zusammenfassung. Exit-Code 0 = alles ok, 1 = mindestens ein Fehler.
#      (Ein übersprungener Flex-Durchlauf ist kein Fehler.)

import glob
import os
import shutil
import subprocess
import sys

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TEST_ORDNER = os.path.join(HAUPTORDNER, "docs", "tests", "logik")
CODE_ORDNER = os.path.join(HAUPTORDNER, "jointsense")
AUSGABE_ORDNER = os.path.join(HAUPTORDNER, "scratch", "tests")

# Prototyp 2 (Flex)
FLEX_TEST_ORDNER = os.path.join(HAUPTORDNER, "docs", "tests", "logik_flex")
FLEX_CODE_ORDNER = os.path.join(HAUPTORDNER, "jointsense_flex")


def durchlauf(compiler, test_ordner, code_ordner, vorsatz, test_hinweis):
    """Übersetzt und startet alle test_*.cpp aus test_ordner zusammen mit code_ordner/*.cpp.

    vorsatz: wird vor den Namen des Testprogramms gesetzt ("" bei Prototyp 1, "flex_" bei Prototyp 2).
    Gibt (anzahl Testdateien, Liste der fehlgeschlagenen Namen) zurück.
    """
    testdateien = sorted(glob.glob(os.path.join(test_ordner, "test_*.cpp")))

    if not testdateien:
        print("Keine Testdateien gefunden (" + test_hinweis + ").")
        return 0, []

    fehlgeschlagen = []

    for datei in testdateien:
        name = os.path.splitext(os.path.basename(datei))[0]
        # Auf Windows brauchen Programme die Endung .exe, auf Mac/Linux nicht
        endung = ".exe" if os.name == "nt" else ""
        programm = os.path.join(AUSGABE_ORDNER, vorsatz + name + endung)
        print("== " + name)

        # Schritt 1: Übersetzen
        befehl = [
            compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
            "-I", code_ordner, "-I", test_ordner,
            datei,
        ] + glob.glob(os.path.join(code_ordner, "*.cpp")) + ["-o", programm]
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
    return len(testdateien), fehlgeschlagen


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

    # Durchlauf 1: Prototyp 1 (unverändert wie bisher)
    anzahl1, fehler1 = durchlauf(compiler, TEST_ORDNER, CODE_ORDNER, "", "docs/tests/logik/test_*.cpp")

    # Durchlauf 2: Prototyp 2 (Flex, REQ-008)
    print()
    print("===== Prototyp 2 (Flex): docs/tests/logik_flex/ + jointsense_flex/ =====")
    anzahl2 = 0
    fehler2 = []
    if not os.path.isdir(FLEX_CODE_ORDNER):
        print("Flex-Durchlauf übersprungen, Ordner fehlt: jointsense_flex/ (noch nicht angelegt).")
    elif not glob.glob(os.path.join(FLEX_CODE_ORDNER, "*.cpp")):
        print("  FEHLER  jointsense_flex/ hat keine .cpp-Datei (logik.cpp fehlt)")
        fehler2 = ["jointsense_flex/logik.cpp fehlt"]
    else:
        anzahl2, fehler2 = durchlauf(compiler, FLEX_TEST_ORDNER, FLEX_CODE_ORDNER, "flex_",
                                     "docs/tests/logik_flex/test_*.cpp")

    fehlgeschlagen = fehler1 + fehler2
    if anzahl1 == 0 and anzahl2 == 0 and not fehlgeschlagen:
        sys.exit(0)
    sys.exit(1 if fehlgeschlagen else 0)


main()
