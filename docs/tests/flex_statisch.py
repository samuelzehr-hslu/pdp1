# flex_statisch.py  (REQ-008, Prototyp 2 Flex, nur Tests)
# Statische Prüfung des Codes in jointsense_flex/ (der Code wird gelesen, nicht ausgeführt):
#   AK-1   12 Bit: analogReadResolution(12), analogRead(PIN_FLEX)
#   AK-7   GRENZWERT_PROZENT Vorgabe 5
#   AK-11  MOTOR_STAERKE Vorgabe 100 (0 bis 127)
#   AK-12  Watchdog 500 ms (wdt_begin(500) oder watchdog_enable(500, ...)) und regelmässiges Zurücksetzen
#   AK-14  GUI_MODUS 1 oben in der .ino, 115200 Baud, Zeilen mit "G,", "D,", "E,"
#   AK-17  Pins und I2C-Adresse nur in pins.h; TEILER_WIDERSTAND_OHM und ADC_MAX nur an einer Stelle
#   AK-18  Prototyp 1 unverändert (git: jointsense/ und tools/gui/)
#   Sicherheitsregeln (B-1, B-3): kein String, kein new/malloc
# Aufruf aus dem Hauptordner:  python docs/tests/flex_statisch.py   (Mac/Linux: python3)
# Exit-Code 0 = alles ok, 1 = mindestens ein Fehler, 2 = nicht geprüft (Ordner fehlt).
import glob
import os
import re
import subprocess
import sys

HAUPT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FLEX = os.path.join(HAUPT, "jointsense_flex")

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


def lesen(pfad):
    with open(pfad, encoding="utf-8", errors="replace") as f:
        return f.read()


def ohne_kommentare(text):
    # Strings zuerst schützen ist hier nicht nötig, URLs in Strings gibt es im Firmware-Code nicht.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def ohne_strings(text):
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def zahl_aus_definition(text, name):
    """Sucht '#define NAME 5' oder 'const float NAME = 5.0;' und gibt die erste Zahl danach zurück (oder None)."""
    for zeile in text.split("\n"):
        if re.search(r"\b" + name + r"\b", zeile) and ("#define" in zeile or "=" in zeile or "{" in zeile):
            m = re.search(r"\b" + name + r"\b\s*[=\{]?\s*\(?\s*(-?\d+(?:\.\d+)?)", zeile)
            if m:
                return float(m.group(1))
    return None


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

    if not os.path.isdir(FLEX):
        print("nicht geprüft: Ordner jointsense_flex/ fehlt")
        sys.exit(2)

    ino_pfad = os.path.join(FLEX, "jointsense_flex.ino")
    pins_pfad = os.path.join(FLEX, "pins.h")
    dateien = sorted(glob.glob(os.path.join(FLEX, "*.ino")) + glob.glob(os.path.join(FLEX, "*.cpp")) + glob.glob(os.path.join(FLEX, "*.h")))

    print("Dateien vorhanden")
    pruefe(os.path.exists(ino_pfad), "jointsense_flex/jointsense_flex.ino gibt es (Ordnername = Dateiname)")
    pruefe(os.path.exists(pins_pfad), "jointsense_flex/pins.h gibt es")
    pruefe(os.path.exists(os.path.join(FLEX, "logik.h")) and os.path.exists(os.path.join(FLEX, "logik.cpp")), "jointsense_flex/logik.h und logik.cpp gibt es")
    if not (os.path.exists(ino_pfad) and os.path.exists(pins_pfad)):
        print("\nAbbruch: Hauptdateien fehlen.")
        sys.exit(1)

    ino_roh = lesen(ino_pfad)
    ino = ohne_kommentare(ino_roh)
    ino_ohne_str = ohne_strings(ino)
    pins = ohne_kommentare(lesen(pins_pfad))
    code = {}   # Dateiname -> Text ohne Kommentare (ohne pins.h)
    for d in dateien:
        if os.path.basename(d) != "pins.h":
            code[os.path.basename(d)] = ohne_kommentare(lesen(d))

    # ---------------------------------------------------------------- AK-17
    print("AK-17: Pins und I2C-Adresse nur in pins.h")

    def pin_wert(name):
        m = re.search(r"\b" + name + r"\b\s*[=\s]\s*\(?\s*([A-Za-z0-9_]+)", pins)
        if not m:
            return None
        return re.sub(r"u$", "", m.group(1))

    flex_pin = pin_wert("PIN_FLEX")
    taste_pin = pin_wert("PIN_TASTE")
    pruefe(flex_pin in ("26", "A0", "PIN_A0"), "AK-17: PIN_FLEX in pins.h ist A0 = GPIO 26 (README), gefunden: %s" % flex_pin)
    pruefe(taste_pin in ("7", "D5", "PIN_D5"), "AK-17: PIN_TASTE in pins.h ist D5 = GPIO 7 (README), gefunden: %s" % taste_pin)
    pruefe(re.search(r"\b13\b|LED_BUILTIN|PIN_LED", pins) is not None, "AK-17: Status-LED (D13 = GPIO 13) steht in pins.h")
    pruefe(re.search(r"\b2\b", pins) is not None and re.search(r"\b3\b", pins) is not None, "AK-17: SDA (GPIO 2) und SCL (GPIO 3) stehen in pins.h")
    pruefe(re.search(r"0x5[aA]", pins) is not None, "AK-17: I2C-Adresse 0x5A steht in pins.h")
    pruefe(not any(re.search(r"0x5[aA]", t) for t in code.values()), "AK-17: 0x5A steht sonst nirgends im Code (nur in pins.h)")
    schlecht = []
    for name, text in code.items():
        for m in re.finditer(r"\b(pinMode|digitalWrite|digitalRead|analogRead|attachInterrupt)\s*\(\s*(\d+|A\d|D\d+)\s*[,)]", text):
            schlecht.append(name + ": " + m.group(0))
        for m in re.finditer(r"\bWire\d?\.set(SDA|SCL)\s*\(\s*\d+", text):
            schlecht.append(name + ": " + m.group(0))
        for m in re.finditer(r"#\s*define\s+PIN_\w+", text):
            schlecht.append(name + ": " + m.group(0))
    pruefe(not schlecht, "AK-17: keine Pin-Nummern im Code ausser in pins.h %s" % (schlecht if schlecht else ""))
    pruefe('#include "pins.h"' in ino_roh, 'AK-17: die .ino bindet "pins.h" ein')

    for name, wert in (("TEILER_WIDERSTAND_OHM", "22000"), ("ADC_MAX", "4095")):
        definitionen = []
        for datei, text in list(code.items()) + [("pins.h", pins)]:
            for m in re.finditer(r"(#\s*define\s+" + name + r"\b[^\n]*|\b" + name + r"\b\s*(?:=|\{)[^;\n]*)", text):
                definitionen.append(datei)
        pruefe(len(definitionen) == 1, "AK-17: %s wird genau einmal festgelegt (gefunden: %d, in %s)" % (name, len(definitionen), definitionen))
        vorkommen = 0
        for datei, text in list(code.items()) + [("pins.h", pins)]:
            vorkommen += len(re.findall(r"\b" + wert + r"\b", text))
        pruefe(vorkommen == 1, "AK-17: die Zahl %s steht im ganzen Code nur einmal (gefunden: %d)" % (wert, vorkommen))
        gefunden = None
        for text in list(code.values()) + [pins]:
            z = zahl_aus_definition(text, name)
            if z is not None:
                gefunden = z
        pruefe(gefunden == float(wert), "AK-17: %s ist %s (gefunden: %s)" % (name, wert, gefunden))

    # ---------------------------------------------------------------- AK-1
    print("AK-1: 12 Bit am Flex-Pin")
    pruefe(re.search(r"analogReadResolution\s*\(\s*12\s*\)", ino) is not None, "AK-1: analogReadResolution(12)")
    pruefe(re.search(r"analogRead\s*\(\s*PIN_FLEX\s*\)", ino) is not None, "AK-1: analogRead(PIN_FLEX)")

    # ---------------------------------------------------------------- AK-7, AK-11, AK-14
    print("AK-7, AK-11, AK-14: Konstanten und GUI_MODUS")
    pruefe(zahl_aus_definition(ino, "GRENZWERT_PROZENT") == 5.0, "AK-7: GRENZWERT_PROZENT = 5 (Vorgabe) steht in der .ino")
    ms = zahl_aus_definition(ino, "MOTOR_STAERKE")
    pruefe(ms == 100.0, "AK-11: MOTOR_STAERKE = 100 (Vorgabe) steht in der .ino (gefunden: %s)" % ms)
    zeilen = ino_roh.split("\n")
    obere = "\n".join(zeilen[:80])
    pruefe(re.search(r"#\s*define\s+GUI_MODUS\s+1\b", obere) is not None, "AK-14: '#define GUI_MODUS 1' steht oben in der .ino (erste 80 Zeilen)")
    # Serial.begin(115200) oder Serial.begin(KONSTANTE), wenn die KONSTANTE im Sketch auf 115200 gesetzt ist
    baud_arg = re.search(r"Serial\.begin\s*\(\s*(\w+)\s*\)", ino)
    baud_wert = None
    if baud_arg is not None:
        baud_wert = 115200.0 if baud_arg.group(1) == "115200" else zahl_aus_definition(ino, baud_arg.group(1))
    pruefe(baud_wert == 115200.0, "AK-14: Serial.begin(115200) (Zahl oder Konstante mit Wert 115200; gefunden: %s)" % baud_wert)
    pruefe(re.search(r"\b9600\b", ino) is None, "AK-14: keine andere Baudrate (9600) in der .ino (Serial immer 115200)")
    for buchstabe in "GDE":
        pruefe(('"%s,' % buchstabe) in ino or ("'%s'" % buchstabe) in ino, "AK-14: die .ino erzeugt Zeilen mit '%s,' am Anfang" % buchstabe)

    # ---------------------------------------------------------------- AK-12
    print("AK-12: Watchdog 500 ms")

    def zahl_oder_name(arg):
        arg = arg.strip()
        if re.fullmatch(r"\d+", arg):
            return int(arg)
        w = zahl_aus_definition(ino, arg)
        return int(w) if w is not None else None

    m = re.search(r"\bwdt_begin\s*\(\s*([A-Za-z0-9_]+)\s*\)", ino) or re.search(r"\bwatchdog_enable\s*\(\s*([A-Za-z0-9_]+)\s*,", ino)
    wert = zahl_oder_name(m.group(1)) if m else None
    pruefe(wert == 500, "AK-12: Watchdog wird mit 500 ms gestartet (gefunden: %s)" % wert)
    pruefe(re.search(r"\bwdt_reset\s*\(|\bwatchdog_update\s*\(", ino) is not None, "AK-12: der Watchdog wird im Programm zurückgesetzt (wdt_reset bzw. watchdog_update)")

    # ---------------------------------------------------------------- AK-13: I2C-Zeitlimit
    print("AK-13: I2C-Zeitlimit höchstens 10 ms pro Befehl (B-3)")
    m = re.search(r"\bWire\d?\.set(?:Wire)?Timeout\s*\(\s*([A-Za-z0-9_]+)", ino)
    wert = zahl_oder_name(m.group(1)) if m else None
    pruefe(wert is not None and 0 < wert <= 10, "AK-13: Wire.setTimeout(...) mit höchstens 10 ms (gefunden: %s)" % wert)
    pruefe(re.search(r"\bserialZeichenVerarbeiten\s*\(", ino) is not None, "AK-13: die .ino benutzt serialZeichenVerarbeiten für die Serial-Eingabe")

    # ---------------------------------------------------------------- Sicherheitsregeln
    print("Sicherheitsregeln (B-1, B-3): kein String, kein new/malloc")
    for name, text in code.items():
        t = ohne_strings(text)
        pruefe(re.search(r"\bString\b", t) is None, "%s: kein Arduino-'String' (Speicher, B-1)" % name)
        pruefe(re.search(r"\bnew\b|\bmalloc\s*\(|\bcalloc\s*\(|\brealloc\s*\(", t) is None, "%s: kein new/malloc (Speicher, B-3)" % name)

    # ---------------------------------------------------------------- AK-18
    print("AK-18: Prototyp 1 unverändert")
    try:
        erg = subprocess.run(["git", "status", "--porcelain", "--", "jointsense", "tools/gui"], cwd=HAUPT,
                             capture_output=True, text=True, timeout=30)
        if erg.returncode != 0:
            print("  nicht geprüft  git status schlug fehl: " + erg.stderr.strip()[:200])
        else:
            pruefe(erg.stdout.strip() == "", "AK-18: git sieht keine Änderung in jointsense/ und tools/gui/ %s" % erg.stdout.strip()[:200])
    except Exception as e:
        print("  nicht geprüft  git nicht verfügbar: " + str(e)[:100])

    print("\n%d von %d Prüfungen bestanden" % (anzahl - fehler, anzahl))
    sys.exit(1 if fehler else 0)


main()
