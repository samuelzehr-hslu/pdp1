# gui_flex_statisch.py  (REQ-008 AK-15, nur Tests)
# Statische Prüfung des Live-GUIs von Prototyp 2: tools/gui_flex/jointsense_flex_live.html
#   - eine einzige Datei, läuft ohne Internet: keine Skripte, Schriften, Stylesheets oder Daten aus dem Netz
#   - benutzt Web Serial (navigator.serial), 115200 Baud
#   - tools/gui_flex/README.md gibt es und nennt Chrome oder Edge
#   - das GUI von Prototyp 1 (tools/gui/) ist unverändert (AK-18, nur Hinweis: das prüft flex_statisch.py mit git)
# Aufruf aus dem Hauptordner:  python docs/tests/gui_flex_host/gui_flex_statisch.py   (Mac/Linux: python3)
# Exit-Code 0 = ok, 1 = Fehler, 2 = nicht geprüft (Datei fehlt).
import os
import re
import sys

HAUPT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HTML = os.path.join(HAUPT, "tools", "gui_flex", "jointsense_flex_live.html")
README = os.path.join(HAUPT, "tools", "gui_flex", "README.md")

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
    if not os.path.exists(HTML):
        print("nicht geprüft: " + HTML + " fehlt")
        sys.exit(2)
    with open(HTML, encoding="utf-8", errors="replace") as f:
        t = f.read()

    print("AK-15: läuft ohne Internet (keine Einbindungen aus dem Netz)")
    pruefe(re.search(r"<script[^>]*\ssrc\s*=", t, re.I) is None, "kein <script src=...>")
    pruefe(re.search(r"<link\b", t, re.I) is None, "kein <link> (keine Stylesheets oder Schriften von aussen)")
    pruefe(re.search(r"@import", t, re.I) is None, "kein @import")
    pruefe(re.search(r"\bfetch\s*\(|XMLHttpRequest|\bWebSocket\b|EventSource|sendBeacon", t) is None,
           "kein fetch / XMLHttpRequest / WebSocket / sendBeacon (keine Netz-Zugriffe)")
    pruefe(re.search(r"\bimport\s*\(|^\s*import\s+[\w{*]", t, re.M) is None, "kein import (keine Module aus dem Netz)")
    pruefe(re.search(r"<(iframe|embed|object)\b", t, re.I) is None, "kein iframe / embed / object")
    pruefe(re.search(r"\b(src|href|data)\s*=\s*[\"']?\s*(https?:)?//", t, re.I) is None, "keine src=/href= Verweise auf Internet-Adressen")
    pruefe(re.search(r"url\(\s*[\"']?\s*(https?:)?//", t, re.I) is None, "kein url(http...) in den Stilen (keine Web-Schriften)")
    pruefe(re.search(r"<script\b[^>]*>", t, re.I) is not None, "es gibt ein eingebautes <script>")
    pruefe(re.search(r"\bcdn\.|googleapis|gstatic|cloudflare|jsdelivr|unpkg", t, re.I) is None, "keine bekannten CDN-Namen")
    adressen = sorted(set(a for a in re.findall(r"https?://[^\s\"'<>)]+", t) if not a.startswith("http://www.w3.org/")))
    if adressen:
        print("  hinweis Internet-Adressen im Text (nur Links, kein Laden?): " + ", ".join(adressen[:5]))

    print("AK-15 / AK-14: Web Serial mit 115200 Baud")
    pruefe(re.search(r"navigator\s*\.\s*serial", t) is not None, "benutzt navigator.serial (Web Serial)")
    pruefe(re.search(r"\b115200\b", t) is not None, "Baudrate 115200 steht im GUI")
    pruefe(re.search(r"\b9600\b", t) is None, "keine andere Baudrate (9600)")
    pruefe(re.search(r"Einmessen", t) is not None, "ein Knopf oder Text 'Einmessen' ist vorhanden")

    print("AK-15: Anleitung für Anfänger")
    if os.path.exists(README):
        with open(README, encoding="utf-8", errors="replace") as f:
            r = f.read()
        pruefe(len(r.strip()) >= 300, "tools/gui_flex/README.md hat Inhalt (mindestens 300 Zeichen)")
        pruefe(re.search(r"Chrome|Edge", r) is not None, "README nennt Chrome oder Edge")
        pruefe(re.search(r"115200", r) is not None, "README nennt die Baudrate 115200")
    else:
        pruefe(False, "tools/gui_flex/README.md fehlt")

    print("\n%d von %d Prüfungen bestanden" % (anzahl - fehler, anzahl))
    sys.exit(1 if fehler else 0)


main()
