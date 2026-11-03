#!/bin/sh
# python_starten.sh
# Startet ein Hook-Skript mit dem Python, das es auf diesem PC gibt.
#
# Warum gibt es diese Datei?
#   Python heisst nicht überall gleich:
#     - Mac und Linux:  meistens "python3"
#     - Windows:        meistens "python" oder "py"
#   Dieses Skript probiert die Namen der Reihe nach aus und nimmt den ersten, der funktioniert.
#   Es läuft mit "sh". Das gibt es auf Mac und Linux immer, und auf Windows
#   bringt Git Bash es mit (Claude Code braucht Git Bash auf Windows sowieso).
#   Python selbst läuft auf allen Prozessoren (Intel/AMD und ARM) gleich.
#
# Aufruf (steht so in .claude/settings.json):
#   sh python_starten.sh <hook-skript.py>
#
# Wichtig: Findet das Skript KEIN Python, sperrt es Developer und Tester,
# damit die Sicherheits-Sperren nicht still ausfallen. Alle anderen arbeiten normal weiter.

skript="$1"

# Die Nachricht von Claude Code (JSON) einmal einlesen, damit wir sie weitergeben können
eingabe=$(cat)

for kandidat in python3 python py; do
  # Gibt es diesen Namen UND ist es wirklich Python 3?
  # (Auf Windows gibt es manchmal einen "python3"-Platzhalter, der nur den Store öffnet.
  #  Der fällt hier durch, weil er den Test nicht besteht.)
  if "$kandidat" -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)" >/dev/null 2>&1; then
    printf '%s' "$eingabe" | "$kandidat" "$skript"
    exit $?
  fi
done

# Hier kommen wir nur an, wenn kein Python gefunden wurde.
if printf '%s' "$eingabe" | grep -Eq '"agent_type"[[:space:]]*:[[:space:]]*"(developer|tester)"'; then
  echo "Gesperrt: Python fehlt auf diesem PC, darum koennen die Sicherheits-Sperren nicht pruefen. Bitte zuerst /einrichten ausfuehren." >&2
  exit 2
fi
exit 0
