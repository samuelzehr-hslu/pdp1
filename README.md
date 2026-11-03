# JointSense

Tragbares Assistenzsystem, das die Überstreckung des Ellbogens erkennt und sofort per Feedback zurückmeldet.
Studentisches Team-Projekt im Modul **Engineering Product Development Project (PDP)**, HSLU, HS26/FS27.

---

## Problemstellung

Personen mit hyperlaxen bzw. überstreckbaren Gelenken nehmen eine Überstreckung im Bewegungsablauf oft nicht oder zu spät wahr. JointSense schliesst diese Lücke beim selbstständigen Training: Gelenkstellung erfassen, mit einem individuell eingemessenen Grenzwert vergleichen, Überschreitung unmittelbar zurückmelden.

**Systemabgrenzung:** keine medizinische Diagnose oder Therapie, kein mechanisches Verhindern der Überstreckung, kein Nachweis eines Langzeit-Trainingseffekts.

## Hardware

### Prototyp

| Komponente | Typ | Funktion |
|---|---|---|
| Mikrocontroller | Arduino Uno / Nano | Winkelberechnung, Grenzwertlogik |
| IMU 1 (Oberarm) | --- | Lage Oberarm |
| IMU 2 (Unterarm) | --- | Lage Unterarm |
| Schalter | N-Kanal-MOSFET (Logic-Level, Typ ---) | Schaltet den Motor, Arduino liefert nur das Steuersignal (Motor braucht bis 80 mA) |
| Gate-Widerstand | 220 Ω | Begrenzt Stromspitzen beim Umladen des Gates, schützt den Arduino-Ausgang |
| Pull-down-Widerstand | 100 kΩ | Hält das Gate auf 0 V, solange der Arduino kein Signal gibt (Motor bleibt beim Start aus) |
| Freilaufdiode | --- | Begrenzt die Spannungsspitze des Motors beim Ausschalten, schützt den MOSFET |
| Spannungsregler | 3 V (Typ ---) | Macht aus 5 V stabile 3 V, Motor ist für 2,5–3,5 V ausgelegt |
| Aktor | ERM-Coin-Vibrationsmotor| Haptisches Feedback |
| Versorgung | USB / Powerbank | Stromversorgung Prototyp |

### Verdrahtung (Schaltplan Rev. B)

| Arduino | Verbunden mit |
|---|---|
| 5V | IMU 1 VCC, IMU 2 VCC, Spannungsregler IN |
| GND | alle GND, IMU 1 AD0 (→ Adresse `0x68`) |
| 3V3 | IMU 2 AD0 (→ Adresse `0x69`) |
| A4 (SDA) | SDA beider IMUs (gemeinsamer I²C-Bus) |
| A5 (SCL) | SCL beider IMUs |
| D9 (PWM) | 220 Ω → MOSFET Gate |
| D2 | Taste (Einmessen), andere Seite an GND. *Vorläufig, Team muss bestätigen* |
| – | MOSFET Gate → 100 kΩ → GND |
| – | Spannungsregler OUT (3 V) → Motor + |
| – | Motor − → MOSFET Drain; MOSFET Source → GND |
| – | Freilaufdiode parallel zum Motor, Kathode (Strich) an 3 V, Anode an Drain |

## Software

### Signalverarbeitung (Konzept)

- Abtastrate: 50–100 Hz (Anforderung: ≥ 25 Hz)
- Sensorfusion: Komplementärfilter bzw. integrierte Fusion (BNO085)
- Entprellung: 50–100 ms gegen Fehlalarme, plus kleine Hysterese gegen Flattern an der Grenze
- Einmessen: Mittelwert über eine kurze Haltezeit, Plausibilitätsprüfung
- Drift: Bei horizontalen Bewegungen Gelenkachse als Zusatzbedingung (Seel et al., 2014) oder Neu-Einmessen

### Setup

1. [Arduino IDE](https://www.arduino.cc/en/software) installieren.
2. Board wählen: *Arduino Uno* bzw. *Arduino Nano*.
3. Benötigte Bibliotheken über den Library Manager installieren (`Wire` ist integriert; IMU-Bibliothek: *im Team festlegen und hier eintragen*).
4. Sketch `jointsense/jointsense.ino` öffnen, Board per USB verbinden, hochladen.

Die Pin-Nummern stehen nur in `jointsense/pins.h`. Quelle ist die Verdrahtungstabelle oben: bei einer Änderung zuerst die README, dann `pins.h` anpassen.

## Bedienung

1. **Anlegen:** Sleeve über den Ellbogen ziehen, Markierung auf Gelenkmitte ausrichten.
2. **Einschalten:** Taste lang drücken, Status-LED zeigt «aktiv».
3. **Einmessen:** Arm entspannt strecken, Taste kurz drücken, kurze Bestätigungsvibration.
4. **Training:** Solange der Winkel im zulässigen Bereich bleibt, passiert nichts.
5. **Überstreckung:** Motor vibriert, bis das Gelenk zurückgeführt wird.
6. **Beenden:** Taste lang drücken; Elektronikmodul zum Waschen entnehmen.

## Zentrale quantitative Anforderungen

| ID | Anforderung | Zielwert |
|---|---|---|
| QA-01 | Messbereich / Abtastrate / Auflösung | −15° bis +100° / ≥ 25 Hz / ≤ 1° |
| QA-02 | Einmessen der Referenz | ≤ 3 s, Wiederholgenauigkeit ± 1,5° |
| QA-03 | Grenzwert-Einstellbereich | 0° bis +10°, Schrittweite ≤ 2° |
| QA-04 | Erkennung Grenzwertüberschreitung | Toleranz ≤ ± 2°, Erkennungsrate ≥ 95 % |
| QA-05 | Feedback-Latenz | ≤ 100 ms, Zuverlässigkeit ≥ 98 % |
| QA-06 | Akkubetrieb | ≥ 4 h |
| QA-07 | Gesamtmasse | ≤ 250 g |
| QA-09 | Verrutschen nach 30 min | < 10 mm, Rotation < 3° |
| QA-13 | An-/Ablegen ohne Hilfe | je ≤ 60 s |

Vollständige Liste (QA-01 bis QA-17) auf dem Miro-Board.