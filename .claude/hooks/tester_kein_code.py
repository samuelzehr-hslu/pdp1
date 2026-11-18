# tester_kein_code.py
# Dieser Hook sorgt dafür, dass der Tester-Agent keinen Produkt-Code ändert.
# Er darf in jointsense/ lesen, aber nichts schreiben oder bearbeiten.
#
# So funktioniert es:
#   Claude Code ruft dieses Skript vor jedem Schreiben/Bearbeiten auf.
#   Es bekommt per "stdin" eine JSON-Nachricht mit agent_type, tool_name und tool_input.
#   Ist es der Tester und liegt die Datei in jointsense/ -> Exit-Code 2 = verboten.
#   Alle anderen Agenten dürfen wie gewohnt.
#
# Gestartet wird es über python_starten.sh (sucht das passende Python auf Mac/Windows/Linux).

import json
import os
import sys

# Firmware-Ordner, in denen der Tester nichts schreiben darf:
#   jointsense       = Prototyp 1 (2 IMUs)
#   jointsense_flex  = Prototyp 2 (Flex-Sensor)
GESPERRTE_ORDNER = ["jointsense", "jointsense_flex"]


def main():
    # Umlaute in Meldungen auch unter Windows richtig ausgeben
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

    try:
        daten = json.loads(sys.stdin.buffer.read().decode("utf-8"))
    except Exception:
        # Kann die Nachricht nicht gelesen werden, nichts blockieren
        sys.exit(0)

    # Nur der Tester wird eingeschränkt
    if daten.get("agent_type") != "tester":
        sys.exit(0)

    eingabe = daten.get("tool_input") or {}
    pfad = str(eingabe.get("file_path") or eingabe.get("notebook_path") or "")
    if not pfad:
        sys.exit(0)

    # Pfad relativ zum Projektordner ausrechnen, z. B. "jointsense/logik.cpp"
    projekt = str(daten.get("cwd") or os.getcwd())
    try:
        relativ = os.path.relpath(os.path.join(projekt, pfad), projekt)
    except ValueError:
        # Anderes Laufwerk (Windows): kann nicht im Projekt liegen
        sys.exit(0)
    relativ = relativ.replace("\\", "/").lower()

    # Liegt die Datei in einem der Firmware-Ordner? Dann verboten.
    # Der Schrägstrich ("ordner/") ist wichtig: Sonst würde z. B. auch eine Datei
    # "jointsense_notizen.md" im Hauptordner gesperrt.
    for ordner in GESPERRTE_ORDNER:
        if relativ == ordner or relativ.startswith(ordner + "/"):
            print(
                "Gesperrt: Der Tester ändert keinen Produkt-Code in jointsense/ "
                "oder jointsense_flex/. Fehler bitte dem Engineer melden.",
                file=sys.stderr,
            )
            sys.exit(2)

    sys.exit(0)


main()
