# developer_blind.py
# Dieser Hook sorgt dafür, dass der Developer-Agent "blind" bleibt:
# Er darf die Testfälle und Testberichte NICHT sehen und NICHT ändern.
#
# So funktioniert es:
#   Claude Code ruft dieses Skript vor jedem Werkzeug-Aufruf auf (Lesen, Suchen, Schreiben, Befehle).
#   Es bekommt per "stdin" eine JSON-Nachricht mit:
#     - agent_type: welcher Agent gerade arbeitet (z. B. "developer")
#     - tool_name:  welches Werkzeug (z. B. "Read")
#     - tool_input: was genau (z. B. welche Datei)
#   Ist es der Developer und geht es um einen gesperrten Ordner -> Exit-Code 2 = verboten.
#   Alle anderen Agenten (Engineer, Tester) dürfen alles wie gewohnt.
#
# Ehrlicher Hinweis: Das ist ein Schutz gegen Versehen, kein perfekter Tresor.

import json
import sys

# Diese Ordner sind für den Developer gesperrt
GESPERRTE_ORDNER = ["docs/tests", "docs/testberichte"]


def ist_gesperrt(text):
    # Backslashes (Windows) in normale Schrägstriche umwandeln, damit der Vergleich überall klappt
    text = str(text).replace("\\", "/").lower()
    for ordner in GESPERRTE_ORDNER:
        if ordner in text:
            return True
    return False


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

    # Nur der Developer wird eingeschränkt
    if daten.get("agent_type") != "developer":
        sys.exit(0)

    eingabe = daten.get("tool_input") or {}
    werkzeug = daten.get("tool_name", "")

    # Suchen im ganzen Repo würde auch Treffer aus den Testordnern zeigen.
    # Darum muss der Developer beim Suchen einen Ordner angeben (z. B. jointsense/).
    if werkzeug in ("Grep", "Glob"):
        pfad = str(eingabe.get("path", "")).replace("\\", "/").rstrip("/")
        projekt = str(daten.get("cwd", "")).replace("\\", "/").rstrip("/")
        if pfad in ("", ".", projekt):
            print(
                "Gesperrt: Bitte nicht im ganzen Repo suchen, sondern einen Ordner angeben, "
                "z. B. path = jointsense/ oder docs/anforderungen/.",
                file=sys.stderr,
            )
            sys.exit(2)

    # Alle Felder prüfen, in denen ein Pfad oder Befehl stehen kann
    zu_pruefen = [
        eingabe.get("file_path", ""),  # Read, Edit, Write
        eingabe.get("path", ""),       # Grep, Glob
        eingabe.get("pattern", ""),    # Glob-Muster
        eingabe.get("glob", ""),       # Grep-Filter
        eingabe.get("command", ""),    # Bash, PowerShell
    ]

    for text in zu_pruefen:
        if text and ist_gesperrt(text):
            print(
                "Gesperrt: Der Developer arbeitet blind und darf docs/tests/ und "
                "docs/testberichte/ nicht lesen oder ändern. Programmiere nur nach der "
                "Anforderung (REQ-xxx.md). Hältst du etwas für unklar, melde es dem Engineer.",
                file=sys.stderr,
            )
            sys.exit(2)

    sys.exit(0)


main()
