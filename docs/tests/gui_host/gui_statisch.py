# gui_statisch.py  (REQ-007 AK-8, nur Tests)
# Statische Prüfung der GUI-Datei gegen die Kopie vor dem Aufräumen:
#   - keine neuen Internet-Adressen, keine <script src>, <link>, @import, fetch/XMLHttpRequest
#   - dieselben Element-Namen (id="...") im Aufbau und dieselben getElementById-Namen
#   - dieselben Farben (#rrggbb) und dieselben Protokoll-Buchstaben/Ereignis-Codes
# Aufruf aus dem Hauptordner:  python docs/tests/gui_host/gui_statisch.py   (Mac/Linux: python3)
# Exit-Code 0 = ok, 1 = Unterschied.
import os
import re
import sys

HAUPT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
VORHER = os.path.join(HAUPT, "scratch", "REQ-007-ausgangslage", "referenz", "jointsense_live_VORHER.html")
NACHHER = os.path.join(HAUPT, "tools", "gui", "jointsense_live.html")


def kennzahlen(pfad):
    with open(pfad, encoding="utf-8") as f:
        t = f.read()
    return {
        "Internet-Adressen": sorted(set(re.findall(r"https?://[^\s\"'<>)]+", t))),
        "externe Einbindungen (script src / link / @import / fetch / XMLHttpRequest)": sorted(set(
            re.findall(r"<script[^>]*\ssrc=|<link\b|@import|\bfetch\s*\(|XMLHttpRequest|\bimport\s+[\w{]", t))),
        "id im Aufbau": sorted(set(re.findall(r"\bid=\"([^\"]+)\"", t))),
        "getElementById-Namen": sorted(set(re.findall(r"getElementById\(\s*[\"']([^\"']+)[\"']", t))),
        "Farben": sorted(set(c.upper() for c in re.findall(r"#[0-9a-fA-F]{6}\b", t))),
        "Ereignis-Codes (GROSSBUCHSTABEN_MIT_UNTERSTRICH in Anführungszeichen oder als Schlüssel)": sorted(set(re.findall(r"\b([A-Z][A-Z0-9]*_[A-Z0-9_]+)\b", t))),
        "Zeilenarten (case 'D' usw.)": sorted(set(re.findall(r"[\"']([GEDPZ])[\"']", t))),
    }


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    if not os.path.exists(VORHER):
        print("Referenz fehlt: " + VORHER + "  -> nicht geprüft")
        sys.exit(2)
    a = kennzahlen(VORHER)
    b = kennzahlen(NACHHER)
    schlecht = 0
    for name in a:
        neu = [x for x in b[name] if x not in a[name]]
        weg = [x for x in a[name] if x not in b[name]]
        if neu or weg:
            schlecht += 1
            print("UNTERSCHIED  %s\n   neu: %s\n   weg: %s" % (name, neu, weg))
        else:
            print("gleich       %s (%d Einträge)" % (name, len(a[name])))
    print("\nErgebnis: " + ("alles gleich" if schlecht == 0 else "%d Gruppe(n) mit Unterschied (Hinweis: Konstanten-Namen im Skript dürfen sich ändern, dann von Hand beurteilen)" % schlecht))
    sys.exit(1 if schlecht else 0)


main()
