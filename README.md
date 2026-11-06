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
| Motortreiber | --- | Ansteuerung Motor |
| Aktor | ERM-Coin-Vibrationsmotor| Haptisches Feedback |
| Versorgung | USB / Powerbank | Stromversorgung Prototyp |

### Verdrahtung (Schaltplan Rev. B)

| Arduino | Verbunden mit |
|---|---|
| 5V | Treiber VCC |
| GND | alle GND, IMU 1 AD0 (→ Adresse `0x68`) |
| 3V3 | IMU 1 VCC, IMU 2 VCC, IMU 2 AD0 (→ Adresse `0x69`) |
| A4 (SDA) | SDA beider IMUs (gemeinsamer I²C-Bus) |
| A5 (SCL) | SCL beider IMUs |
| D9 (PWM) | Treiber IN1 |
| D8 | Treiber IN2 (im Code dauerhaft `LOW`) |
| D2 | Taste (Einmessen), andere Seite an GND. *Vorläufig, Team muss bestätigen* |
| – | Treiber OUT A / OUT B → Vibrationsmotor |

## Software

### Signalverarbeitung (Konzept)

- Abtastrate: 50–100 Hz (Anforderung: ≥ 25 Hz)
- Sensorfusion: Komplementärfilter bzw. integrierte Fusion (BNO085). Beschleunigung **und** Gyroskop, damit Drehungen (Unterarm, Schulter, Schwenken) den Ellbogenwinkel nicht verfälschen (REQ-002)
- Entprellung: 50–100 ms gegen Fehlalarme, plus kleine Hysterese gegen Flattern an der Grenze
- Einmessen: Referenz = Mittelwert über eine kurze Haltezeit (2 s still), danach Arm genau 2 Mal beugen und strecken und dabei den Oberarm in kleinen Kreisen mitbewegen (4 s), damit das Gerät die Scharnier-Achse des Ellbogens findet, danach Hand einmal drehen (1,5 s), damit es die Knochenachse des Unterarms misst. Plausibilitätsprüfung (Arm ruhig? genug Bewegung? Achsen eindeutig?), im Zweifel Abbruch statt Raten
- Drift: Das Gerät korrigiert die Gyro-Drift im Betrieb selbst (Ruhe erkennen, Gelenkachse als Zusatzbedingung, Seel et al., 2014). Die Referenz (0°) wird dabei nie automatisch verändert. Ziel: 10 min ± 2° ohne Neu-Einmessen

### Setup

1. [Arduino IDE](https://www.arduino.cc/en/software) installieren.
2. Board wählen: *Arduino Uno* bzw. *Arduino Nano*.
3. Benötigte Bibliotheken über den Library Manager installieren (`Wire` ist integriert; IMU-Bibliothek: *im Team festlegen und hier eintragen*).
4. Sketch `jointsense/jointsense.ino` öffnen, Board per USB verbinden, hochladen.

Die Pin-Nummern stehen nur in `jointsense/pins.h`. Quelle ist die Verdrahtungstabelle oben: bei einer Änderung zuerst die README, dann `pins.h` anpassen.

## Bedienung

1. **Anlegen:** Sleeve über den Ellbogen ziehen, Markierung auf Gelenkmitte ausrichten.
2. **Einschalten:** Taste lang drücken, Status-LED zeigt «aktiv».
3. **Einmessen:** Im Stehen den Arm entspannt gestreckt locker nach unten hängen lassen, Taste kurz drücken und ca. 2 s still halten. Ein kurzer Vibrationsstoss meldet «jetzt bewegen»: Arm genau 2 Mal beugen und strecken (ca. 4 s) und dabei den Oberarm in kleinen Kreisen mitbewegen. Zwei kurze Stösse melden «jetzt Hand drehen»: Hand einmal hin und zurück drehen (ca. 1,5 s, Arm sonst ruhig). Danach Bestätigungsvibration. Nach dem Einschalten muss einmal eingemessen werden (kein automatisches Einmessen).
4. **Training:** Solange der Winkel im zulässigen Bereich bleibt, passiert nichts.
5. **Überstreckung:** Motor vibriert, bis das Gelenk zurückgeführt wird.
6. **Beenden:** Taste lang drücken; Elektronikmodul zum Waschen entnehmen.

## Zentrale quantitative Anforderungen

| ID | Anforderung | Zielwert |
|---|---|---|
| QA-01 | Messbereich / Abtastrate / Auflösung | −100° (Beugung) bis +15° (Überstreckung) / ≥ 25 Hz / ≤ 1° |
| QA-02 | Einmessen der Referenz | ≤ 8 s (2 s still, 4 s Beuge-Bewegung, 1,5 s Hand drehen), Wiederholgenauigkeit ± 1,5° |
| QA-03 | Grenzwert-Einstellbereich | 0° bis +10°, Schrittweite ≤ 2° |
| QA-04 | Erkennung Grenzwertüberschreitung | Toleranz ≤ ± 2°, Erkennungsrate ≥ 95 % |
| QA-05 | Feedback-Latenz | ≤ 100 ms, Zuverlässigkeit ≥ 98 % |
| QA-06 | Akkubetrieb | ≥ 4 h |
| QA-07 | Gesamtmasse | ≤ 250 g |
| QA-09 | Verrutschen nach 30 min | < 10 mm, Rotation < 3° |
| QA-13 | An-/Ablegen ohne Hilfe | je ≤ 60 s |

Vollständige Liste (QA-01 bis QA-17) auf dem Miro-Board.