# Prüft, ob alle Werkzeuge für JointSense installiert sind.
# Dieses Skript installiert NICHTS. Es schaut nur nach und schreibt das Ergebnis hin.
#
# Aufruf: python3 pruefen.py   (Windows: python pruefen.py oder py pruefen.py)
#
# Es erkennt auch das System (Windows, Mac, Linux) und den Prozessor (Intel/AMD oder ARM).
# Danach richtet sich, welche Installations-Anleitung in SKILL.md passt.

import json
import os
import platform
import shutil
import subprocess
import sys


def befehl_ausfuehren(befehl, eingabe=None):
    # Führt einen Befehl aus und gibt (Exit-Code, erste Zeile der Ausgabe) zurück.
    # Klappt es gar nicht, kommt (-1, "") zurück.
    try:
        ergebnis = subprocess.run(befehl, capture_output=True, text=True, timeout=30, input=eingabe)
        text = (ergebnis.stdout or ergebnis.stderr).strip()
        return ergebnis.returncode, (text.splitlines()[0] if text else "")
    except Exception:
        return -1, ""


def system_erkennen():
    # Gibt z. B. ("Windows", "ARM64") oder ("Mac", "ARM64 (Apple-Chip)") zurück.
    # Achtung: Auf Windows ARM und auf Macs mit Rosetta kann Python "emuliert" laufen
    # und meldet dann Intel/AMD, obwohl der Prozessor ARM ist. Darum prüfen wir extra.
    name = platform.system()
    maschine = platform.machine().lower()
    ist_arm = maschine in ("arm64", "aarch64") or maschine.startswith("arm")

    if name == "Windows":
        # PROCESSOR_IDENTIFIER zeigt den echten Prozessor, auch wenn Python emuliert läuft
        if "arm" in os.environ.get("PROCESSOR_IDENTIFIER", "").lower():
            ist_arm = True
        return "Windows", ("ARM64" if ist_arm else "x64 (Intel/AMD)")

    if name == "Darwin":
        # sysctl sagt, ob der Mac einen Apple-Chip hat (auch wenn Python unter Rosetta läuft)
        code, text = befehl_ausfuehren(["sysctl", "-n", "hw.optional.arm64"])
        if text.strip() == "1":
            ist_arm = True
        return "Mac", ("ARM64 (Apple-Chip)" if ist_arm else "x64 (Intel)")

    return name or "Linux", ("ARM64" if ist_arm else "x64 (Intel/AMD)")


def werkzeug_pruefen(name, befehl):
    # Schaut, ob ein Programm gefunden wird, und zeigt seine Version.
    if shutil.which(befehl[0]) is None:
        print(f"[FEHLT] {name}")
        return False
    code, version = befehl_ausfuehren(befehl)
    print(f"[OK]    {name}: {version}")
    return True


def python_name_finden():
    # Welcher Name startet Python auf diesem PC? (wichtig für alle Befehle in der Doku)
    for kandidat in ("python3", "python", "py"):
        if shutil.which(kandidat):
            code, text = befehl_ausfuehren([kandidat, "-c", "import sys; print(sys.version_info[0])"])
            if code == 0 and text.strip() == "3":
                return kandidat
    return None


def shell_finden():
    # Die Hooks laufen mit "sh". Auf Windows kommt sh von Git Bash.
    for kandidat in ("sh", "bash"):
        pfad = shutil.which(kandidat)
        if pfad:
            return pfad
    if os.name == "nt":
        for pfad in (os.environ.get("CLAUDE_CODE_GIT_BASH_PATH", ""),
                     r"C:\Program Files\Git\bin\bash.exe",
                     r"C:\Program Files\Git\bin\sh.exe"):
            if pfad and os.path.exists(pfad):
                return pfad
    return None


def sperren_pruefen(shell):
    # Testet die Sicherheits-Sperre wie Claude Code: Der Developer will einen Test lesen -> muss gesperrt sein.
    hooks = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "hooks"))
    starter = os.path.join(hooks, "python_starten.sh")
    skript = os.path.join(hooks, "developer_blind.py")
    beispiel = json.dumps({
        "agent_type": "developer",
        "tool_name": "Read",
        "tool_input": {"file_path": "docs/tests/logik/beispiel.cpp"},
    })
    code, text = befehl_ausfuehren([shell, starter, skript], eingabe=beispiel)
    return code == 2


print("JointSense - Werkzeuge pruefen")
print("-------------------------------")

system, prozessor = system_erkennen()
print(f"[INFO]  System: {system}, Prozessor: {prozessor}")

alles_ok = True

# Python selbst läuft schon, sonst würde dieses Skript nicht laufen.
print(f"[OK]    Python: {sys.version.split()[0]}")
python_name = python_name_finden()
if python_name:
    print(f"[INFO]  Python startet hier mit: {python_name}")
else:
    print("[HINWEIS] Python wurde unter keinem der Namen python3 / python / py gefunden (PATH pruefen)")

# Sicherheits-Sperren (Hooks) testen
shell = shell_finden()
if shell is None:
    print("[FEHLT] sh (Shell fuer die Sicherheits-Sperren; Windows: Git for Windows installieren)")
    alles_ok = False
elif sperren_pruefen(shell):
    print("[OK]    Sicherheits-Sperren (Hooks) funktionieren")
else:
    print("[FEHLT] Sicherheits-Sperren (Hooks) funktionieren NICHT")
    alles_ok = False

if not werkzeug_pruefen("arduino-cli", ["arduino-cli", "version"]):
    alles_ok = False
else:
    # Ist der Core für Uno/Nano (arduino:avr) installiert?
    alle_cores = ""
    try:
        alle_cores = subprocess.run(["arduino-cli", "core", "list"],
                                    capture_output=True, text=True, timeout=30).stdout
    except Exception:
        pass
    if "arduino:avr" in alle_cores:
        print("[OK]    arduino:avr (Uno/Nano)")
    else:
        print("[FEHLT] arduino:avr (Uno/Nano)")
        alles_ok = False

# Mac mit Apple-Chip: Das Board-Paket arduino:avr enthält Intel-Programme,
# die nur mit Rosetta 2 laufen.
if system == "Mac" and prozessor.startswith("ARM64"):
    code, text = befehl_ausfuehren(["arch", "-x86_64", "/usr/bin/true"])
    if code == 0:
        print("[OK]    Rosetta 2 (fuer arduino:avr auf Apple-Chip)")
    else:
        print("[FEHLT] Rosetta 2 (fuer arduino:avr auf Apple-Chip)")
        alles_ok = False

# C++-Compiler: g++ oder clang++ (Windows ARM hat meist nur clang++)
compiler_gefunden = False
for name in ("g++", "clang++"):
    if shutil.which(name):
        werkzeug_pruefen(f"C++-Compiler ({name})", [name, "--version"])
        compiler_gefunden = True
        break
if not compiler_gefunden:
    print("[FEHLT] C++-Compiler (g++ oder clang++)")
    alles_ok = False

if not werkzeug_pruefen("cppcheck (Code-Scanner)", ["cppcheck", "--version"]):
    alles_ok = False

if not werkzeug_pruefen("wokwi-cli (Simulation)", ["wokwi-cli", "--version"]):
    alles_ok = False

# Den Token NIE ausgeben, nur sagen, ob er gesetzt ist.
if os.environ.get("WOKWI_CLI_TOKEN"):
    print("[OK]    WOKWI_CLI_TOKEN ist gesetzt")
else:
    print("[FEHLT] WOKWI_CLI_TOKEN ist nicht gesetzt")
    alles_ok = False

print("-------------------------------")
if alles_ok:
    print("Alles bereit.")
else:
    print("Es fehlt noch etwas (siehe [FEHLT]).")

# Immer mit 0 beenden, damit ein "python3 ... || python ..." nicht doppelt startet
sys.exit(0)
