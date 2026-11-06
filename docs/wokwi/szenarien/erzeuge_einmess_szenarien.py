# erzeuge_einmess_szenarien.py
# Schreibt die Wokwi-Szenarien, in denen ein ERFOLGREICHES Einmessen nachgestellt wird (REQ-002), nach docs/wokwi/szenarien/.
#
# Warum ein Skript? Wokwi kann die Sensorwerte nur in Stufen setzen (set-control). Eine Beuge-Bewegung mit Oberarm-Bewegung
# besteht aus vielen Stufen (alle 100 ms neue Werte fuer Beschleunigung UND Drehgeschwindigkeit beider IMUs).
# Das Skript rechnet die Werte aus einer einfachen Arm-Geometrie aus (wie docs/tests/logik/sim_ellbogen.h),
# es baut NICHT die Logik der Firmware nach. Die Firmware muss aus den Werten selbst Winkel und Achse finden.
#
# Wokwi-MPU6050 (gemessen, REQ-002 Runde 2): Regler accelX/Y/Z in g, rotationX/Y/Z in Grad/s
# (NICHT gyroX/gyroY: die gibt es nicht, Wokwi meldet dafuer auch keinen Fehler).
# Rohwert = Grad/s * 65,5 bei +-500 Grad/s, begrenzt auf +32767 / -32768.
#
# Aufruf (aus dem Hauptordner):  python docs/wokwi/szenarien/erzeuge_einmess_szenarien.py   (Mac/Linux: python3, Windows evtl. py)
# Die erzeugten .yaml-Dateien stehen im Repo. Das Skript nur neu starten, wenn sich der Ablauf aendert.
#
# Modell: Beide IMUs liegen im Ruhezustand gleich (Beschleunigung 0/0/1 g, "Arm gestreckt", Ellbogenwinkel 0).
# Scharnier-Achse = Sensor-Achse Y. Ueberstreckung +30 Grad = accelX 0,5 / accelZ 0,866 (wie in den REQ-001-Szenarien).
# Einmess-Bewegung (AK-6): Phase 2 = Unterarm beugt 2 Mal 70 Grad und streckt, der Oberarm macht dabei kleine Kreise
# (20 Grad um zwei waagrechte Achsen). Die Bewegung liegt zwischen t0+2100 und t0+5900 (je 100 ms Luft zum Stoss und zur Bestaetigung).

import math
import os

SCHRITT_MS = 100
HIER = os.path.dirname(os.path.abspath(__file__))

# ---------------- kleine Mathe-Helfer (3x3-Matrizen) ----------------


def mal(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def transp(a):
    return [[a[j][i] for j in range(3)] for i in range(3)]


def mal_vec(a, v):
    return [sum(a[i][k] * v[k] for k in range(3)) for i in range(3)]


def rx(g):
    c, s = math.cos(math.radians(g)), math.sin(math.radians(g))
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def ry(g):
    c, s = math.cos(math.radians(g)), math.sin(math.radians(g))
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


EINHEIT = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]


def drehvektor_grad(d):
    vx = (d[2][1] - d[1][2]) / 2.0
    vy = (d[0][2] - d[2][0]) / 2.0
    vz = (d[1][0] - d[0][1]) / 2.0
    s = math.sqrt(vx * vx + vy * vy + vz * vz)
    c = (d[0][0] + d[1][1] + d[2][2] - 1.0) / 2.0
    winkel = math.atan2(s, c)
    f = winkel / s if s > 1e-12 else 1.0
    return [math.degrees(vx * f), math.degrees(vy * f), math.degrees(vz * f)]


def weich(x):
    if x <= 0:
        return 0.0
    if x >= 1:
        return 1.0
    return x * x * (3 - 2 * x)


# ---------------- Zeitplan: Ereignisse mit Zeit in ms ----------------


class Plan:
    def __init__(self):
        self.ereignisse = []   # (zeit_ms, reihenfolge, text)
        self.zuletzt = {}      # letzter gesetzter Wert je (imu, regler), nur Aenderungen werden geschrieben
        self.maxrate = 0.0

    def roh(self, t, text):
        self.ereignisse.append((t, len(self.ereignisse), text))

    def setze(self, t, teil, regler, wert):
        if isinstance(wert, float):
            wert = round(wert, 4 if regler.startswith("accel") else 3)
            if wert == 0:
                wert = 0.0
        if self.zuletzt.get((teil, regler)) == wert:
            return
        self.zuletzt[(teil, regler)] = wert
        text = "%g" % wert if isinstance(wert, float) else str(wert)
        self.roh(t, "  - set-control: {part-id: %s, control: %s, value: %s}" % (teil, regler, text))

    def pin(self, t, wert):
        self.roh(t, '  - expect-pin: {part-id: uno, pin: "9", value: %d}' % wert)

    def kabel(self, t, nr, dran):
        self.setze(t, "kabel%d" % nr, "pressed", 1 if dran else 0)

    def taste(self, t):
        self.setze(t, "taste", "pressed", 1)
        self.setze(t + 150, "taste", "pressed", 0)

    def imu_werte(self, t, teil, acc, rate):
        self.setze(t, teil, "accelX", acc[0])
        self.setze(t, teil, "accelY", acc[1])
        self.setze(t, teil, "accelZ", acc[2])
        self.setze(t, teil, "rotationX", rate[0])
        self.setze(t, teil, "rotationY", rate[1])
        self.setze(t, teil, "rotationZ", rate[2])
        self.maxrate = max(self.maxrate, abs(rate[0]), abs(rate[1]), abs(rate[2]))

    def bewegung(self, t_start, dauer_ms, lage):
        # lage(s) liefert (R1, R2) = Drehmatrix Oberarm-IMU und Unterarm-IMU zur Zeit s (Sekunden seit t_start).
        # Je Stufe: Drehgeschwindigkeit = Mittelwert ueber die Stufe, Beschleunigung = Lage in der Mitte der Stufe.
        n = dauer_ms // SCHRITT_MS
        dt = SCHRITT_MS / 1000.0
        for k in range(n):
            ta, tm, tb = k * dt, (k + 0.5) * dt, (k + 1) * dt
            a1, a2 = lage(ta)
            m1, m2 = lage(tm)
            b1, b2 = lage(tb)
            t = t_start + k * SCHRITT_MS
            for teil, ra, rm, rb in (("imu1", a1, m1, b1), ("imu2", a2, m2, b2)):
                w = [x / dt for x in drehvektor_grad(mal(transp(ra), rb))]
                acc = mal_vec(transp(rm), [0.0, 0.0, 1.0])
                self.imu_werte(t, teil, acc, w)
        # Ende: genau die Endlage, Drehgeschwindigkeit 0
        e1, e2 = lage(n * dt)
        t = t_start + n * SCHRITT_MS
        self.imu_werte(t, "imu1", mal_vec(transp(e1), [0.0, 0.0, 1.0]), [0.0, 0.0, 0.0])
        self.imu_werte(t, "imu2", mal_vec(transp(e2), [0.0, 0.0, 1.0]), [0.0, 0.0, 0.0])

    def text(self, bis_ms=None):
        zeilen = []
        jetzt = 0
        for t, _, text in sorted(self.ereignisse):
            if t > jetzt:
                zeilen.append("  - delay: %dms" % (t - jetzt))
                jetzt = t
            zeilen.append(text)
        if bis_ms is not None and bis_ms > jetzt:
            zeilen.append("  - delay: %dms" % (bis_ms - jetzt))
        return "\n".join(zeilen) + "\n"


# ---------------- Bausteine ----------------


def beuge(s, amplitude=70.0, zyklen=2):
    return -0.5 * amplitude * (1.0 - math.cos(2 * math.pi * zyklen * s / 4.0))


def einmess_lage(s, oberarm_grad=20.0):
    # s = Sekunden seit t0+2100 (Dauer 3,8 s), auf die 4 s der Bewegungsphase gedehnt
    # oberarm_grad = Groesse der Oberarm-Kreise (0 = Oberarm haengt still, AK-7d)
    u = s / 3.8 * 4.0
    if u >= 4.0:
        u = 4.0
    w = 2 * math.pi * u / 4.0
    pitch = -oberarm_grad * 0.5 * (1.0 - math.cos(w))     # Oberarm: Pendeln nach vorn ...
    seit = oberarm_grad * 0.5 * math.sin(w)               # ... und zur Seite = kleine Kreise, beide um waagrechte Achsen
    r1 = mal(rx(seit), ry(pitch))
    r2 = mal(r1, ry(-beuge(u)))
    return r1, r2


def einmessen(plan, t0, oberarm_grad=20.0):
    # Taste, Phase 1 (2 s still), Stoss, Phase 2 (Beugen/Strecken mit Oberarm-Bewegung), Bestaetigung bei t0+6000.
    plan.taste(t0)
    plan.bewegung(t0 + 2100, 3800, lambda s: einmess_lage(s, oberarm_grad))


def ueberstrecken(plan, t, von, bis, dauer_ms=500, teil="imu2"):
    # Unterarm (IMU 2) weich von 'von' auf 'bis' Grad Ellbogenwinkel (positiv = Ueberstreckung), Oberarm in Ruhe.
    def lage(s):
        f = von + (bis - von) * weich(s * 1000.0 / dauer_ms)
        return EINHEIT, ry(-f)
    plan.bewegung(t, dauer_ms, lage)


def kopf(name, titel, gehoert):
    return ('# %s\n# %s\n# Kabel = Taster in der SDA-Leitung (pressed 1 = Kabel dran, 0 = Kabel ab). Pin 9 = Motor.\n'
            '# ERZEUGT mit docs/wokwi/szenarien/erzeuge_einmess_szenarien.py (nicht von Hand aendern, Skript anpassen).\n'
            'name: "%s"\nversion: 1\nsteps:\n') % (titel, gehoert, name)


def schreibe(dateiname, name, titel, gehoert, plan, bis_ms, rest=""):
    inhalt = kopf(name, titel, gehoert) + plan.text(bis_ms) + rest
    with open(os.path.join(HIER, dateiname), "w", encoding="utf-8", newline="\n") as f:
        f.write(inhalt)
    print("  %-48s %5d Zeilen, groesste Drehgeschwindigkeit %.0f Grad/s" % (dateiname, inhalt.count("\n"), plan.maxrate))


T0 = 1000            # Tastendruck (ms nach Start)
ENDE = T0 + 6400     # Ende des Vorspanns "Einmessen" (Bestaetigung ist dann vorbei)
REQ001 = "Gehoert zu REQ-001 (Vorspann: erfolgreiches Einmessen nach REQ-002). Erklaerung: docs/wokwi/szenarien/README.md"
REQ002 = "Gehoert zu REQ-002. Erklaerung: docs/wokwi/REQ-002-stoerfaelle.md und docs/wokwi/szenarien/README.md"


def vorspann():
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    einmessen(p, T0)
    return p


def main():
    # Selbstpruefung des Modells: +30 Grad Ueberstreckung = accelX 0,5 / accelZ 0,866
    r1, r2 = einmess_lage(0.0)
    a = mal_vec(transp(ry(-30.0)), [0, 0, 1])
    assert abs(a[0] - 0.5) < 1e-6 and abs(a[2] - 0.8660254) < 1e-6, a
    print("Schreibe Szenarien nach", HIER)

    # ---------------- REQ-001-Szenarien, jetzt mit echtem Einmessen im Vorspann ----------------
    # ak04: Ausfall bei Ueberstreckung. Nach der Wiederkehr gilt REQ-002 AK-13: erst keine Warnung, spaetestens nach 5 s wieder Alarm.
    p = vorspann()
    ueberstrecken(p, ENDE, 0.0, 30.0)
    p.pin(ENDE + 1100, 1)   # Warnung laeuft (Referenz stammt aus dem Einmessen)
    rest = """  # IMU 2 faellt aus. M = Zeitpunkt der ersten FEHLER-Zeile (AK-6: sofort)
  - set-control: {part-id: kabel2, control: pressed, value: 0}
  - wait-serial: "FEHLER"
  # Muster: an 0-100, aus 100-200, an 200-300, aus 300-400, an 400-500 (gemessen in der Mitte jedes Stosses)
  - delay: 50ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  - delay: 100ms
  # t=150: Pause. Der Arm ist weiter ueberstreckt, trotzdem aus (AK-4: Ausfall hat Vorrang)
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 100ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  - delay: 100ms
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 100ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  - delay: 100ms
  # t=550: Muster zu Ende, Motor aus (AK-5)
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 950ms
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 1500ms
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 1900ms
  # t=4900: noch Pause
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 150ms
  # t=5050: Muster beginnt nach 5000 ms von vorn (Start zu Start), Toleranz +-50 ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  - delay: 100ms
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 100ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  # Kabel wieder dran, mitten in einem Stoss (t=5250)
  - set-control: {part-id: kabel2, control: pressed, value: 1}
  - wait-serial: "wieder da"
  - delay: 45ms
  # AK-8: Motor sofort aus. AK-9 (REQ-001) und AK-13 (REQ-002): kein Sofort-Alarm, die Lage ist noch unbekannt (Entprellzeit 80 ms)
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 5500ms
  # AK-13 (REQ-002): spaetestens 5 s nach der Wiederkehr ist die Lage wieder bekannt. Der Arm ist weiter ueberstreckt, die alte
  # Referenz gilt: der Alarm kommt zurueck.
  - expect-pin: {part-id: uno, pin: "9", value: 1}
"""
    schreibe("ak04_ausfall_bei_ueberstreckung.yaml", "ak04_ausfall_bei_ueberstreckung",
             "AK-3 bis AK-9: IMU 2 faellt aus, waehrend der Arm ueberstreckt ist (Muster, Vorrang, Wiederkehr, alte Referenz, REQ-002 AK-13)",
             REQ001, p, ENDE + 1100, rest)

    # ak08: Wiederkehr mitten im Stoss
    p = vorspann()
    rest = """  # Arm gestreckt. IMU 1 faellt aus.
  - set-control: {part-id: kabel1, control: pressed, value: 0}
  - wait-serial: "FEHLER"
  - delay: 30ms
  # Mitten im 1. Stoss: Motor an
  - expect-pin: {part-id: uno, pin: "9", value: 1}
  - set-control: {part-id: kabel1, control: pressed, value: 1}
  - wait-serial: "wieder da"
  - delay: 40ms
  # Der Stoss waere noch nicht zu Ende, aber der Ausfall ist vorbei: Motor aus (AK-8)
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 1500ms
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  # Wiederholung (Wackelkontakt): neuer Ausfall -> Muster beginnt neu, sofort an
  - set-control: {part-id: kabel1, control: pressed, value: 0}
  - wait-serial: "FEHLER"
  - delay: 50ms
  - expect-pin: {part-id: uno, pin: "9", value: 1}
"""
    schreibe("ak08_wiederkehr_mitten_im_stoss.yaml", "ak08_wiederkehr_mitten_im_stoss",
             "AK-7 und AK-8: Wiederkehr mitten im Stoss: Motor sofort aus, Zeile 'wieder da' ohne 'FEHLER'", REQ001, p, ENDE, rest)

    # ak10_einmessen_abbruch: zweites Einmessen faellt kurz aus -> abgebrochen, alte Referenz gilt weiter
    p = vorspann()
    ueberstrecken(p, ENDE, 0.0, 30.0)
    p.pin(ENDE + 1100, 1)
    rest = """  # Einmessen starten, nach 100 ms faellt IMU 2 fuer 30 ms aus (1 bis 2 Fehlschlaege, KEIN Ausfall)
  - set-control: {part-id: taste, control: pressed, value: 1}
  - delay: 100ms
  - set-control: {part-id: kabel2, control: pressed, value: 0}
  - delay: 30ms
  - set-control: {part-id: kabel2, control: pressed, value: 1}
  - delay: 20ms
  - set-control: {part-id: taste, control: pressed, value: 0}
  # (kein wait-serial: Die Zeile kommt schon waehrend der Pausen davor. Der Text wird vom Skript pruefe_REQ-001.py im Log geprueft.)
  - delay: 300ms
  - delay: 1500ms
  # Die alte Referenz (gestreckt) gilt weiter: der ueberstreckte Arm loest weiter Alarm aus (Lage bleibt nach dem Abbruch bekannt, REQ-002 AK-7).
  # Waere neu eingemessen worden (im ueberstreckten Zustand), waere der Motor jetzt aus.
  - expect-pin: {part-id: uno, pin: "9", value: 1}
"""
    schreibe("ak10_einmessen_abbruch.yaml", "ak10_einmessen_abbruch",
             "AK-10: Fehlschlag beim Einmessen -> abgebrochen, Referenz unveraendert (Arm ueberstreckt: Alarm muss bleiben)",
             REQ001, p, ENDE + 1100, rest)

    # ak10_einmessen_bei_ausfall: Einmessen waehrend eines Ausfalls -> abgebrochen; danach Wiederkehr (REQ-002 AK-13)
    p = vorspann()
    p.kabel(ENDE, 2, False)
    rest1 = """  - wait-serial: "FEHLER"
  - delay: 300ms
  # Taste druecken, obwohl IMU 2 fehlt
  - set-control: {part-id: taste, control: pressed, value: 1}
  - delay: 150ms
  - set-control: {part-id: taste, control: pressed, value: 0}
  # (kein wait-serial: Die Zeile kommt schon waehrend der Pausen davor. Der Text wird vom Skript pruefe_REQ-001.py im Log geprueft.)
  - delay: 300ms
  # Waehrend IMU 2 weg ist, ueberstreckt der Arm (weiche Bewegung, 500 ms). Die Lage ist danach unbekannt (REQ-002 AK-13).
"""
    q = Plan()
    ueberstrecken(q, 0, 0.0, 30.0)
    rest2 = q.text(500)
    rest3 = """  # IMU 2 kommt zurueck, der Arm ist ueberstreckt
  - set-control: {part-id: kabel2, control: pressed, value: 1}
  - wait-serial: "wieder da"
  - delay: 45ms
  # Noch keine Warnung: Lage unbekannt (Entprellzeit 80 ms)
  - expect-pin: {part-id: uno, pin: "9", value: 0}
  - delay: 5500ms
  # Spaetestens 5 s nach der Wiederkehr gilt wieder die alte Referenz (gestreckt) -> ueberstreckter Arm loest Alarm aus (REQ-002 AK-13)
  - expect-pin: {part-id: uno, pin: "9", value: 1}
"""
    schreibe("ak10_einmessen_bei_ausfall.yaml", "ak10_einmessen_bei_ausfall",
             "AK-3 und AK-10: Einmessen WAEHREND eines Ausfalls -> abgebrochen, keine Muellwerte als Referenz",
             REQ001, p, ENDE, rest1 + rest2 + rest3)

    # ak11: Start ohne IMU 2, spaeter Einmessen per Taste (erfolgreich), danach Warnung.
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(10000, 2, True)
    ueberstrecken(p, 12000, 0.0, 30.0)
    p.pin(12600, 0)        # Es gibt keine Referenz -> KEINE Warnung
    p.pin(17000, 0)        # weiter keine Warnung (kein automatisches Einmessen)
    ueberstrecken(p, 17000, 30.0, 0.0)   # Arm strecken
    einmessen(p, 18000)    # per Taste einmessen: Referenz bei t = 24000, Bestaetigung bis 24150
    ueberstrecken(p, 26500, 0.0, 30.0)   # jetzt mit Referenz
    p.pin(27600, 1)
    schreibe("ak11_ohne_referenz.yaml", "ak11_ohne_referenz",
             "AK-11: Start ohne IMU 2 -> keine Referenz, keine Warnung, 'Keine Referenz' alle 1000 ms, Einmessen nur per Taste",
             REQ001, p, 27700)

    # ---------------- REQ-002-Stoerfaelle mit Gyroskop ----------------
    # W1: Einschalten ohne Einmessen, Ueberstreckung ohne Referenz, schnelles Drehen und Uebersteuerung (AK-9, AK-14, AK-15)
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    p.setze(1000, "imu2", "accelX", 0.5)
    p.setze(1000, "imu2", "accelZ", 0.866)
    for t in (2000, 3000, 4000):
        p.pin(t, 0)
    p.setze(4000, "imu2", "rotationX", 250)       # schnelles Drehen, gueltiger Wert (Rohwert 16375): kein Alarm, kein Ausfall
    p.pin(4500, 0)
    p.setze(4500, "imu2", "rotationX", -1000)     # Uebersteuerung negativ (Rohwert -32768): Zyklus ueberspringen, kein Ausfall
    p.pin(4800, 0)
    p.setze(5100, "imu2", "rotationX", 1000)      # Uebersteuerung positiv (Rohwert +32767)
    p.pin(5400, 0)
    p.setze(5700, "imu2", "rotationX", 0)
    p.pin(7200, 0)
    schreibe("req002_w1_einschalten.yaml", "req002_w1_einschalten",
             "W1: Einschalten ohne Einmessen, Zykluszeit, schnelles Drehen und Uebersteuerung (AK-9, AK-14, AK-15)", REQ002, p, 7200)

    # W3: Phase 1 nicht ruhig (AK-7b). Nur IMU 1 (Oberarm) dreht sich, IMU 2 ist ruhig: 'mindestens eine IMU' genuegt.
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    einmessen(p, T0)
    p.setze(T0 + 50, "imu1", "rotationX", 60)     # 60 Grad/s waehrend Phase 1 (Mittelwert klar ueber 10 Grad/s)
    p.setze(T0 + 1950, "imu1", "rotationX", 0)
    # Phase 2 laeuft normal (Beugen/Strecken mit Oberarm-Bewegung): der Abbruch hat also den Grund 'ruhig'.
    p.pin(T0 + 6075, 0)    # keine Bestaetigungsvibration
    p.pin(T0 + 6175, 0)
    p.pin(T0 + 6400, 0)
    schreibe("req002_w3_nicht_ruhig.yaml", "req002_w3_nicht_ruhig",
             "W3: Arm in Phase 1 nicht ruhig -> Abbruch 'ruhig' (AK-7b)", REQ002, p, T0 + 6500)

    # W4: IMU 2 faellt in Phase 2 aus (AK-7c). Die Bewegung laeuft weiter (auch ohne IMU 2 sichtbar).
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    einmessen(p, T0)
    p.kabel(T0 + 3000, 2, False)
    p.pin(T0 + 6000, 0)
    p.pin(T0 + 6100, 0)
    p.kabel(T0 + 6200, 2, True)
    ueberstrecken(p, T0 + 6500, 0.0, 30.0)
    p.pin(T0 + 7700, 0)   # keine Referenz entstanden (Einmessen war abgebrochen): keine Warnung
    schreibe("req002_w4_imu_ausfall_in_phase2.yaml", "req002_w4_imu_ausfall_in_phase2",
             "W4: IMU 2 faellt in Phase 2 aus -> Abbruch IMU (AK-7c, REQ-001)", REQ002, p, T0 + 8000)

    # W5: erfolgreiches Einmessen, Bestaetigung, Warnung mit Referenz, Uebersteuerung (AK-6, AK-9, AK-14), unter 10 s.
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    einmessen(p, T0)
    p.pin(T0 + 1950, 0)
    p.pin(T0 + 2050, 1)     # Stoss 100 ms (gehoert zu Phase 2)
    p.pin(T0 + 2250, 0)
    p.pin(T0 + 3000, 0)     # waehrend der Bewegung keine Warnung
    p.pin(T0 + 4500, 0)
    p.pin(T0 + 6075, 1)     # Bestaetigungsvibration 150 ms bei t0 + 6000 (+-100)
    p.pin(T0 + 6350, 0)
    ueberstrecken(p, T0 + 6400, 0.0, 30.0)
    p.pin(T0 + 7500, 1)     # Referenz gesetzt: +30 Grad = Warnung
    p.setze(T0 + 7600, "imu2", "rotationX", -1000)   # Uebersteuerung negativ (-32768): der Zyklus wird uebersprungen
    p.pin(T0 + 7800, 1)     # Warnung bleibt, kein Ausfall
    p.setze(T0 + 8000, "imu2", "rotationX", 1000)    # Uebersteuerung positiv (+32767)
    p.pin(T0 + 8200, 1)
    p.setze(T0 + 8400, "imu2", "rotationX", 0)
    p.pin(T0 + 8700, 1)
    schreibe("req002_w5_einmessen_ok.yaml", "req002_w5_einmessen_ok",
             "W5: erfolgreiches Einmessen, Bestaetigung, Warnung, Uebersteuerung (AK-6, AK-9, AK-14)", REQ002, p, T0 + 8700)

    # W6: Unterarm beugt 70 Grad, Oberarm haengt still -> Abbruch 'Oberarm' (AK-7d). Beugung und Ruhe sind in Ordnung,
    # also kommt der Abbruch weder aus (a) noch aus (b).
    p = Plan()
    p.kabel(0, 1, True)
    p.kabel(0, 2, True)
    einmessen(p, T0, oberarm_grad=0.0)
    p.pin(T0 + 6075, 0)    # keine Bestaetigungsvibration
    p.pin(T0 + 6175, 0)
    p.pin(T0 + 6400, 0)
    schreibe("req002_w6_oberarm_still.yaml", "req002_w6_oberarm_still",
             "W6: Oberarm haengt in Phase 2 still -> Abbruch 'Oberarm' (AK-7d)", REQ002, p, T0 + 6500)


main()
