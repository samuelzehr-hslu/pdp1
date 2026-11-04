# ausfall_verlaeufe.py
# Erzeugt Messverläufe für REQ-001 (Sensorausfall): Wann antwortet welche IMU, und ist der Arm "zu weit"?
#
# Wichtig: Hier wird die Logik NICHT nachgebaut. Python erzeugt nur die Eingabe-Werte.
# Den Rest macht die echte C++-Logik im Test docs/tests/logik/test_REQ-001.cpp.
#
# Aufruf (aus dem Hauptordner des Repos):
#   python3 docs/tests/szenarien/ausfall_verlaeufe.py     (Windows: python oder py)
# (docs/tests/alle_tests.py startet dieses Skript automatisch vor den C++-Tests.)
#
# Ergebnis: CSV-Dateien in scratch/szenarien/ mit den Spalten
#   zeit_ms, imu1_ok, imu2_ok, zu_weit
#   zeit_ms  = Zeitpunkt der Messung (wie millis(), springt nach 4294967295 wieder auf 0)
#   imu1_ok  = 1 wenn IMU 1 (Oberarm) geantwortet hat, 0 wenn die Messung fehlgeschlagen ist
#   imu2_ok  = dasselbe für IMU 2 (Unterarm)
#   zu_weit  = 1 wenn die Grenzwert-Prüfung "Überstreckung" melden würde (hier oft absichtlich 1,
#              damit man sieht, dass bei einem Ausfall die Warnung trotzdem aus ist)

import os
import random

HAUPTORDNER = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
AUSGABE_ORDNER = os.path.join(HAUPTORDNER, "scratch", "szenarien")

TAKT_MS = 20  # 50 Hz
ZEIT_MODUL = 2 ** 32  # millis() läuft nach 4294967295 wieder bei 0 los


def speichern(name, zeilen):
    # zeilen = Liste von (zeit_ms, imu1_ok, imu2_ok, zu_weit)
    os.makedirs(AUSGABE_ORDNER, exist_ok=True)
    pfad = os.path.join(AUSGABE_ORDNER, name + ".csv")
    with open(pfad, "w", encoding="utf-8") as datei:
        datei.write("zeit_ms,imu1_ok,imu2_ok,zu_weit\n")
        for zeit, ok1, ok2, zu_weit in zeilen:
            datei.write("%d,%d,%d,%d\n" % (zeit % ZEIT_MODUL, ok1, ok2, zu_weit))
    print("geschrieben: " + pfad)


def verlauf(dauer_ms, regel, start_ms=0, takte=None):
    # regel(t) bekommt die Zeit seit Beginn in ms und gibt (ok1, ok2, zu_weit) zurück.
    # takte: optionale Liste von Abständen (für unregelmässige Takte), sonst immer TAKT_MS.
    zeilen = []
    t = 0
    i = 0
    while t < dauer_ms:
        ok1, ok2, zu_weit = regel(t)
        zeilen.append((start_ms + t, ok1, ok2, zu_weit))
        t += takte[i % len(takte)] if takte else TAKT_MS
        i += 1
    return zeilen


def kurze_aussetzer():
    # Immer nur 1 oder 2 Fehlschläge in Folge, dazwischen ok. Nie ein Ausfall (AK-2).
    # Fehlschlag-Zeitpunkte (in Messzyklen): IMU 2: 1 mal, dann 2 mal, dann 1 mal ...
    fehl1 = set()
    fehl2 = set()
    for start, laenge in [(10, 1), (30, 2), (50, 1), (70, 2)]:
        for i in range(laenge):
            fehl2.add(start + i)
    for start, laenge in [(20, 2), (40, 1), (60, 2), (80, 1)]:
        for i in range(laenge):
            fehl1.add(start + i)
    # beide gleichzeitig kurz weg (je 2 Zyklen)
    for i in (100, 101):
        fehl1.add(i)
        fehl2.add(i)

    def regel(t):
        zyklus = t // TAKT_MS
        return (0 if zyklus in fehl1 else 1, 0 if zyklus in fehl2 else 1, 1)

    return verlauf(3000, regel)


def imu2_dauerausfall():
    # 2 s alles gut, dann fällt IMU 2 für 12 s aus (mehr als 2 Muster), dann 3 s alles gut.
    # Der Arm ist ab 1 s durchgehend "zu weit": die Warnung muss bei Ausfall trotzdem aus sein.
    def regel(t):
        ok2 = 0 if 2000 <= t < 14000 else 1
        zu_weit = 1 if t >= 1000 else 0
        return (1, ok2, zu_weit)

    return verlauf(17000, regel, start_ms=5000)


def imu1_dauerausfall():
    def regel(t):
        ok1 = 0 if 2000 <= t < 14000 else 1
        zu_weit = 1 if t >= 1000 else 0
        return (ok1, 1, zu_weit)

    return verlauf(17000, regel, start_ms=123456)


def beide_ausfall():
    # IMU 1 fällt bei 2 s aus, IMU 2 bei 3 s. IMU 1 kommt bei 8 s zurück, IMU 2 bei 9 s.
    # Zwischen 2 s und 9 s gilt der Ausfall, ab 9 s nicht mehr.
    def regel(t):
        ok1 = 0 if 2000 <= t < 8000 else 1
        ok2 = 0 if 3000 <= t < 9000 else 1
        return (ok1, ok2, 1)

    return verlauf(12000, regel, start_ms=777)


def wackelkontakt():
    # IMU 2 hat einen Wackelkontakt: mal geht es, mal nicht (zufällig, fester Startwert).
    # Immer wieder gibt es 3 oder mehr Fehlschläge in Folge -> viele Ausfälle und Wiederkehr.
    random.seed(42)
    zustand = {"ok": True, "rest": 0}

    def regel(t):
        if zustand["rest"] <= 0:
            zustand["ok"] = not zustand["ok"]
            # Fehler-Phasen 1..8 Zyklen, ok-Phasen 1..10 Zyklen
            zustand["rest"] = random.randint(1, 8) if not zustand["ok"] else random.randint(1, 10)
        zustand["rest"] -= 1
        return (1, 1 if zustand["ok"] else 0, random.choice([0, 1, 1]))

    return verlauf(60000, regel, start_ms=2024)


def zeitueberlauf():
    # millis() läuft nach 4294967295 über. Der Ausfall beginnt 2 s VOR dem Überlauf und
    # dauert 8 s, geht also über den Überlauf hinweg. Danach noch eine Wiederkehr.
    start = ZEIT_MODUL - 3000

    def regel(t):
        ok2 = 0 if 1000 <= t < 9000 else 1
        return (1, ok2, 1)

    return verlauf(12000, regel, start_ms=start)


def lang_minuten():
    # 5 Minuten: IMU 1 fällt nach 10 s aus und bleibt weg (rund 58 Wiederholungen des Musters).
    # Der Arm ist durchgehend "zu weit". Der Start liegt 100 s vor dem Zeitüberlauf von millis().
    def regel(t):
        return (0 if t >= 10000 else 1, 1, 1)

    return verlauf(300000, regel, start_ms=ZEIT_MODUL - 100000)


def unregelmaessige_takte():
    # Die Schleife ist nicht immer genau 20 ms: Abstände zwischen 15 und 25 ms (zufällig, fester Startwert).
    random.seed(42)
    takte = [random.randint(15, 25) for _ in range(997)]

    def regel(t):
        # Ausfall IMU 2 von 1 s bis 3 s, von 5 s bis 5,4 s und von 7 s bis 7,1 s (3 Ausfaelle)
        ok2 = 1
        if 1000 <= t < 3000 or 5000 <= t < 5400 or 7000 <= t < 7100:
            ok2 = 0
        return (1, ok2, 1)

    return verlauf(9000, regel, start_ms=ZEIT_MODUL - 5000, takte=takte)


def main():
    speichern("ausfall_kurze_aussetzer", kurze_aussetzer())
    speichern("ausfall_imu2_dauer", imu2_dauerausfall())
    speichern("ausfall_imu1_dauer", imu1_dauerausfall())
    speichern("ausfall_beide", beide_ausfall())
    speichern("ausfall_wackelkontakt", wackelkontakt())
    speichern("ausfall_zeitueberlauf", zeitueberlauf())
    speichern("ausfall_lang_minuten", lang_minuten())
    speichern("ausfall_unregelmaessige_takte", unregelmaessige_takte())


main()
