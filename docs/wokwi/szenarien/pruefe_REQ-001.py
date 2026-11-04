# pruefe_REQ-001.py
# Startet alle Wokwi-Szenarien zu REQ-001 (Sensorausfall) und wertet die Ausgabe des Seriellen Monitors aus.
#
# Voraussetzungen (siehe docs/wokwi/README.md und /einrichten):
#   - wokwi-cli ist installiert und WOKWI_CLI_TOKEN ist gesetzt
#   - die Firmware wurde kompiliert:
#       arduino-cli compile --fqbn arduino:avr:uno --warnings all --output-dir scratch/build jointsense
#
# Aufruf (aus dem Hauptordner des Repos):
#   python3 docs/wokwi/szenarien/pruefe_REQ-001.py        (Windows: python oder py)
#
# Was passiert:
#   - Jedes Szenario (*.yaml) laeuft komplett durch. Darin stehen die Pin-Pruefungen (Motor an/aus).
#   - Zusaetzlich werden Texte im Seriellen Monitor geprueft ("FEHLER", "wieder da", ...).
#   - Zeitabstaende der Meldungen (AK-6, AK-11) werden gemessen, indem dasselbe Szenario zu
#     verschiedenen Zeitpunkten abgebrochen wird und man die Zeilen im Log zaehlt.
#   - Logs liegen in scratch/wokwi-req001/ (nicht im Repo).
#
# Ergebnis: Exit-Code 0 = alles bestanden, 1 = mindestens ein Fehler,
#           2 = nicht geprueft (wokwi-cli oder Firmware fehlt). "nicht geprueft" ist NICHT "bestanden".

import os
import shutil
import subprocess
import sys

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
LOG_ORDNER = os.path.join(HAUPTORDNER, "scratch", "wokwi-req001")
DIAGRAMM = "szenarien/diagram_kabelbruch.json"   # relativ zu docs/wokwi/
VORSPANN_MS = 5150                               # Dauer des Vorspanns in den Szenarien (Einmessen)
MUSTER_TEXT_IMU1 = "IMU 1 (Oberarm)"
MUSTER_TEXT_IMU2 = "IMU 2 (Unterarm)"

fehler = []     # Liste der gefundenen Fehler (Text)
anzahl = [0]    # Anzahl der Pruefungen


def pruefe(bedingung, text):
    anzahl[0] += 1
    if bedingung:
        print("  ok      " + text)
    else:
        print("  FEHLER  " + text)
        fehler.append(text)


def lauf(name, label, timeout_ms, fail_text=None):
    # Startet wokwi-cli. Gibt (exit_code, zeilen_im_log, ausgabe) zurueck.
    os.makedirs(LOG_ORDNER, exist_ok=True)
    logdatei_rel = "scratch/wokwi-req001/" + label + ".log"
    logdatei = os.path.join(HAUPTORDNER, "scratch", "wokwi-req001", label + ".log")
    if os.path.exists(logdatei):
        os.remove(logdatei)
    befehl = [WOKWI, "docs/wokwi", "--diagram-file", DIAGRAMM,
              "--scenario", "szenarien/" + name + ".yaml",
              "--timeout", str(timeout_ms), "--serial-log-file", logdatei_rel]
    if fail_text:
        befehl += ["--fail-text", fail_text]
    try:
        r = subprocess.run(befehl, cwd=HAUPTORDNER, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=600)
        code, ausgabe = r.returncode, (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        code, ausgabe = 99, "wokwi-cli hing (Zeitlimit 600 s)"
    zeilen = []
    if os.path.exists(logdatei):
        with open(logdatei, encoding="utf-8", errors="replace") as f:
            text = f.read().replace("\r", "")
        zeilen = text.split("\n")
        zeilen = zeilen[:-1]    # letztes Stueck ist leer oder eine halbe Zeile -> weglassen
    return code, zeilen, ausgabe


def zeige_ausgabe(ausgabe):
    # Bei Fehlern: die letzten paar interessanten Zeilen der wokwi-cli-Ausgabe zeigen
    interessant = [z for z in ausgabe.split("\n") if z.strip() and not z.startswith("Abweichung")]
    for z in interessant[-4:]:
        print("          " + z.strip())


def zaehle(zeilen, *teile):
    # Zaehlt Zeilen, die ALLE Textteile enthalten
    return sum(1 for z in zeilen if all(t in z for t in teile))


def wieder_da_ohne_fehler(zeilen):
    # AK-8: 'wieder da' darf nie zusammen mit dem Wort FEHLER in einer Zeile stehen
    return all("FEHLER" not in z for z in zeilen if "wieder da" in z)


def volllauf(name, fail_text=None, timeout_ms=45000):
    # Szenario ganz durchlaufen lassen. Erfolg = alle erwarteten Texte und Pin-Pruefungen stimmen.
    code, zeilen, ausgabe = lauf(name, name, timeout_ms, fail_text)
    pruefe(code == 0, name + ": Szenario komplett bestanden (alle Pin- und Text-Pruefungen)")
    if code != 0:
        zeige_ausgabe(ausgabe)
    return zeilen


def fehlerzeilen(zeilen, imu_text):
    return zaehle(zeilen, "FEHLER", imu_text)


def main():
    global WOKWI
    WOKWI = shutil.which("wokwi-cli")
    if WOKWI is None:
        print("NICHT GEPRUEFT: wokwi-cli fehlt (siehe /einrichten).")
        sys.exit(2)
    if not os.path.exists(os.path.join(HAUPTORDNER, "scratch", "build", "jointsense.ino.hex")):
        print("NICHT GEPRUEFT: Firmware fehlt. Zuerst kompilieren:")
        print("  arduino-cli compile --fqbn arduino:avr:uno --warnings all --output-dir scratch/build jointsense")
        sys.exit(2)

    print("== Normalbetrieb ohne Stoerung")
    z = volllauf("normal_ohne_stoerung", fail_text="FEHLER")
    pruefe(zaehle(z, "wieder da") == 0, "Normalbetrieb: keine Zeile 'wieder da' (kein falscher Ausfall)")
    pruefe(zaehle(z, "Keine Referenz") == 0, "Normalbetrieb: keine Zeile 'Keine Referenz'")
    pruefe(zaehle(z, "Einmessen abgebrochen") == 0, "Normalbetrieb: kein abgebrochenes Einmessen")

    print("== AK-2: kurze Aussetzer (1-2 Fehlschlaege) sind kein Ausfall")
    z = volllauf("ak02_kurze_aussetzer", fail_text="FEHLER")
    pruefe(zaehle(z, "wieder da") == 0, "AK-2: kein Ausfall, also auch kein 'wieder da'")

    print("== AK-2 / AK-5 / QA-05: Erkennung nach 3 Fehlschlaegen, Muster startet sofort")
    z = volllauf("ak02_erkennung_zeit")
    pruefe(fehlerzeilen(z, MUSTER_TEXT_IMU2) >= 1, "AK-6: Zeile mit FEHLER und 'IMU 2 (Unterarm)' kommt")

    print("== AK-3 bis AK-9: Ausfall bei Ueberstreckung, Muster, Wiederkehr, alte Referenz")
    z = volllauf("ak04_ausfall_bei_ueberstreckung")
    pruefe(fehlerzeilen(z, MUSTER_TEXT_IMU2) >= 5, "AK-6: IMU 2 wird laufend gemeldet (mindestens 5 Zeilen in ca. 5 s)")
    pruefe(zaehle(z, "FEHLER", MUSTER_TEXT_IMU1) == 0, "AK-6: IMU 1 laeuft und wird NICHT als ausgefallen gemeldet")
    pruefe(zaehle(z, "wieder da") >= 1, "AK-8: Zeile 'wieder da' erscheint")
    pruefe(wieder_da_ohne_fehler(z), "AK-8: Zeile 'wieder da' enthaelt nicht das Wort FEHLER")

    print("== AK-6: Takt der Meldungen (1000 ms +-100 ms)")
    p = VORSPANN_MS
    name = "ak06_meldungen_takt"
    for n in range(1, 5):
        # Spaetestens bei p+160+1100*(n-1) muessen n Zeilen da sein (erste Zeile hoechstens 100 ms + Sendezeit nach p).
        unten = p + 160 + 1100 * (n - 1)
        # Vor p+30+900*n kann die (n+1)-te Zeile noch nicht da sein (erste Zeile fruehestens 40 ms nach p).
        oben = p + 30 + 900 * n
        _, z_unten, _ = lauf(name, "%s_unten%d" % (name, n), unten)
        _, z_oben, _ = lauf(name, "%s_oben%d" % (name, n), oben)
        a = fehlerzeilen(z_unten, MUSTER_TEXT_IMU2)
        b = fehlerzeilen(z_oben, MUSTER_TEXT_IMU2)
        pruefe(a >= n, "AK-6: nach %d ms (Ausfall seit %d ms) sind mindestens %d Zeilen da (gefunden: %d)" % (unten, unten - p, n, a))
        pruefe(b <= n, "AK-6: nach %d ms (Ausfall seit %d ms) sind hoechstens %d Zeilen da (gefunden: %d)" % (oben, oben - p, n, b))

    print("== AK-6 / AK-8: beide IMUs fallen aus")
    z = volllauf("ak06_beide_ausfall")
    pruefe(fehlerzeilen(z, MUSTER_TEXT_IMU1) >= 2, "AK-6: IMU 1 wird gemeldet (mehrfach)")
    pruefe(fehlerzeilen(z, MUSTER_TEXT_IMU2) >= 2, "AK-6: IMU 2 wird auch gemeldet (mehrfach)")
    pruefe(zaehle(z, "wieder da") == 1 and wieder_da_ohne_fehler(z), "AK-8: genau eine Zeile 'wieder da', ohne FEHLER")
    # IMU 2 kommt bei 10150 ms zurueck, IMU 1 erst bei 13150 ms.
    _, z_a, _ = lauf("ak06_beide_ausfall", "ak06_beide_schnitt1", 10400)
    _, z_b, _ = lauf("ak06_beide_ausfall", "ak06_beide_schnitt2", 13100)
    pruefe(zaehle(z_b, "wieder da") == 0, "AK-8: solange IMU 1 noch fehlt, gibt es noch kein 'wieder da'")
    pruefe(fehlerzeilen(z_b, MUSTER_TEXT_IMU2) == fehlerzeilen(z_a, MUSTER_TEXT_IMU2),
           "AK-6: IMU 2 ist zurueck und wird nicht mehr als ausgefallen gemeldet")
    pruefe(fehlerzeilen(z_b, MUSTER_TEXT_IMU1) - fehlerzeilen(z_a, MUSTER_TEXT_IMU1) >= 2,
           "AK-6: IMU 1 (noch weg) wird weiter alle ca. 1000 ms gemeldet")

    print("== AK-7 / AK-8: Wiederkehr mitten im Stoss")
    z = volllauf("ak08_wiederkehr_mitten_im_stoss")
    pruefe(zaehle(z, "wieder da") >= 1 and wieder_da_ohne_fehler(z), "AK-8: 'wieder da' ohne das Wort FEHLER")

    print("== AK-10: Einmessen abgebrochen")
    z = volllauf("ak10_einmessen_abbruch", fail_text="FEHLER")
    pruefe(zaehle(z, "Einmessen abgebrochen") >= 1, "AK-10: Zeile 'Einmessen abgebrochen'")
    z = volllauf("ak10_keine_bestaetigung", fail_text="FEHLER")
    pruefe(zaehle(z, "Einmessen abgebrochen") >= 1, "AK-10: Zeile 'Einmessen abgebrochen'")
    z = volllauf("ak10_einmessen_bei_ausfall")
    pruefe(zaehle(z, "Einmessen abgebrochen") >= 1, "AK-10: Einmessen waehrend des Ausfalls wird abgebrochen")

    print("== AK-11: keine Referenz")
    z = volllauf("ak11_ohne_referenz")
    pruefe(zaehle(z, "Keine Referenz") >= 5, "AK-11: Zeile 'Keine Referenz' kommt mehrfach")
    for (von, bis, text, kleinste, groesste) in [
        (3000, 8000, "waehrend IMU 2 fehlt", 4, 6),
        (12000, 17000, "IMU 2 ist wieder da, aber noch nicht eingemessen", 4, 6),
        (21000, 24000, "nach dem Einmessen per Taste", 0, 0),
    ]:
        _, z_von, _ = lauf("ak11_ohne_referenz", "ak11_von%d" % von, von)
        _, z_bis, _ = lauf("ak11_ohne_referenz", "ak11_bis%d" % bis, bis)
        n = zaehle(z_bis, "Keine Referenz") - zaehle(z_von, "Keine Referenz")
        pruefe(kleinste <= n <= groesste,
               "AK-11: zwischen %d ms und %d ms (%s): %d Zeilen 'Keine Referenz' (erlaubt: %d bis %d)"
               % (von, bis, text, n, kleinste, groesste))

    print()
    print("Pruefungen: %d, fehlgeschlagen: %d" % (anzahl[0], len(fehler)))
    for f in fehler:
        print("  - " + f)
    sys.exit(1 if fehler else 0)


main()
