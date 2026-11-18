# JointSense

Tragbares Assistenzsystem, das die Überstreckung des Ellbogens erkennt und sofort per Feedback zurückmeldet.
Studentisches Team-Projekt im Modul **Engineering Product Development Project (PDP)**, HSLU, HS26/FS27.

---

## Problemstellung

Personen mit hyperlaxen bzw. überstreckbaren Gelenken nehmen eine Überstreckung im Bewegungsablauf oft nicht oder zu spät wahr. JointSense schliesst diese Lücke beim selbstständigen Training: Gelenkstellung erfassen, mit einem individuell eingemessenen Grenzwert vergleichen, Überschreitung unmittelbar zurückmelden.

**Systemabgrenzung:** keine medizinische Diagnose oder Therapie, kein mechanisches Verhindern der Überstreckung, kein Nachweis eines Langzeit-Trainingseffekts.

## Hardware

Es gibt **zwei Sensorik-Konzepte**, die parallel gebaut und verglichen werden:

| Prototyp | Sensor | Controller | Code |
|---|---|---|---|
| 1 | 2 IMUs (Oberarm + Unterarm) | Arduino Uno / Nano | `jointsense/` |
| 2 | Flex-Sensor über dem Ellbogen | Adafruit Feather RP2040 | `jointsense_flex/` (REQ-008) |

### Prototyp 1 (2 IMUs)

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

### Prototyp 2 (Flex-Sensor)

| Komponente | Typ | Funktion |
|---|---|---|
| Mikrocontroller | Adafruit Feather RP2040 (3,3 V, 12-Bit-ADC, LiPo-Lader eingebaut) | Messen, Grenzwertlogik |
| Sensor | Flex-Sensor 4,5" (Spectra Symbol, 10 kΩ gerade, 60–110 kΩ gebogen) | Biegung auf der Rückseite des Ellbogens |
| Spannungsteiler | Festwiderstand 22 kΩ | macht aus dem Widerstand eine Spannung für A0 |
| Motortreiber | Adafruit DRV2605L (Haptik-Treiber, I²C `0x5A`) | Ansteuerung Motor |
| Aktor | ERM-Coin-Vibrationsmotor | Haptisches Feedback |
| Versorgung | LiPo 3,7 V 500 mAh (403040, 40 × 30 × 4 mm, JST-PH 2,0 mm, mit Schutzschaltung, max. 250 mA dauernd), Laden über USB-C | Stromversorgung |

### Verdrahtung Prototyp 2 (Flex)

Einzige Wahrheit für `jointsense_flex/pins.h`.

| Feather RP2040 | Verbunden mit |
|---|---|
| 3V3 | Flex-Sensor Seite 1, DRV2605L VIN |
| A0 (GPIO 26) | Flex-Sensor Seite 2 und 22 kΩ nach GND (Spannungsteiler) |
| GND | 22 kΩ, DRV2605L GND, Taste |
| SDA (GPIO 2) | DRV2605L SDA (I²C, Adresse `0x5A`) |
| SCL (GPIO 3) | DRV2605L SCL |
| D5 (GPIO 7) | Taste (Einmessen), andere Seite an GND (interner Pullup) |
| D13 (GPIO 13) | rote LED auf dem Board = Status-LED (eingebaut, kein Kabel) |
| BAT (JST-Stecker) | LiPo-Akku. **Vor dem ersten Anstecken Polarität mit Multimeter prüfen** (Rot = Plus), viele Akkus sind vertauscht |
| – | DRV2605L Motor + / Motor − → Vibrationsmotor |

## Software

### Signalverarbeitung (Konzept)

- Abtastrate: 50–100 Hz (Anforderung: ≥ 25 Hz)
- Sensorfusion: Komplementärfilter bzw. integrierte Fusion (BNO085). Beschleunigung **und** Gyroskop, damit Drehungen (Unterarm, Schulter, Schwenken) den Ellbogenwinkel nicht verfälschen (REQ-002)
- Entprellung: 50–100 ms gegen Fehlalarme, plus kleine Hysterese gegen Flattern an der Grenze
- Einmessen (REQ-005): Referenz = Mittelwert über 2 s ruhige Zeit am Stück (Wackeln zählt neu, kein Abbruch). Danach Arm frei bewegen: beugen (Scharnier-Achse), Oberarm in zwei Richtungen bewegen (Achse der Oberarm-IMU), Hand drehen (Knochenachse des Unterarms), in beliebiger Reihenfolge. Das Gerät sammelt, bis alle Werte genügen und die Achsen eindeutig sind, und zeigt den Fortschritt im GUI bzw. im Seriellen Monitor. Jeder Teil höchstens 30 s, im Zweifel weiter sammeln oder Abbruch statt Raten
- Drift: Das Gerät korrigiert die Gyro-Drift im Betrieb selbst (Ruhe erkennen, Gelenkachse als Zusatzbedingung, Seel et al., 2014). Die Referenz (0°) wird dabei nie automatisch verändert. Ziel: 10 min ± 2° ohne Neu-Einmessen

### Prototyp 2: Signalverarbeitung (Flex, REQ-008)

- Abtastrate 50 Hz, 12 Bit. Widerstand aus dem Spannungsteiler: `R_flex = 22000 × (4095 / adc − 1)`
- Der Sensor ist auf der Rückseite **vorgebogen**: Überstreckung = weniger Biegung = kleinerer Widerstand
- Einmessen mit **einem Punkt**: Arm gestreckt, 2 s ruhig (höchstens 30 s). Gemessen wird die **Abweichung in Prozent** vom eingemessenen Widerstand, nicht in Grad (Team-Entscheid)
- Grenzwert 0–20 % (Vorgabe 5 %), Entprellung 80 ms, Hysterese 1 Prozentpunkt
- Sensor-Ausfall (ADC unter 100 oder über 3995): Motor aus, LED blinkt schnell
- Eigenes Live-GUI: `tools/gui_flex/jointsense_flex_live.html`

### Anzeige (REQ-006)

Ein Schalter oben in `jointsense/jointsense.ino` wählt, was über USB kommt:

- `#define GUI_MODUS 1` (Einstellung im Repo): Datenzeilen mit 115200 Baud für das Live-GUI im Browser, `tools/gui/jointsense_live.html` (Anleitung: `tools/gui/README.md`).
- `#define GUI_MODUS 0`: Texte für den Seriellen Monitor der Arduino IDE mit 9600 Baud.

### Setup

1. [Arduino IDE](https://www.arduino.cc/en/software) installieren.
2. Board wählen: *Arduino Uno* bzw. *Arduino Nano*.
3. Benötigte Bibliotheken über den Library Manager installieren (`Wire` ist integriert; IMU-Bibliothek: *im Team festlegen und hier eintragen*).
4. Sketch `jointsense/jointsense.ino` öffnen, Board per USB verbinden, hochladen.

**Prototyp 2 (Feather RP2040):** In der Arduino IDE unter *Einstellungen → Zusätzliche Boardverwalter-URLs* `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json` eintragen, im Boardverwalter *Raspberry Pi Pico/RP2040* (Earle Philhower) installieren (Version: siehe `docs/tech-stack.md`), Board *Adafruit Feather RP2040* wählen, Sketch `jointsense_flex/jointsense_flex.ino` öffnen. Klappt das Hochladen nicht: BOOT-Taste halten, RESET drücken, dann nochmals hochladen.

Die Pin-Nummern stehen nur in `jointsense/pins.h` (Prototyp 2: `jointsense_flex/pins.h`). Quelle ist die Verdrahtungstabelle oben: bei einer Änderung zuerst die README, dann `pins.h` anpassen.

## Bedienung

1. **Anlegen:** Sleeve über den Ellbogen ziehen, Markierung auf Gelenkmitte ausrichten.
2. **Einschalten:** Taste lang drücken, Status-LED zeigt «aktiv».
3. **Einmessen:** Im Stehen den Arm entspannt gestreckt locker nach unten hängen lassen, Taste kurz drücken (oder Enter im Seriellen Monitor bzw. Knopf «Einmessen» im GUI) und still halten, bis das Gerät 2 s Ruhe gemessen hat (Vibrationsstoss 300 ms). Nochmals bestätigen, dann den Arm frei bewegen, **eine Bewegung nach der anderen** (Reihenfolge egal): ein paar Mal beugen und strecken, den Oberarm in einem kleinen Kreis bewegen, die Hand hin und zurück drehen (dabei Oberarm ruhig halten). Alles gleichzeitig zählt nicht. Das GUI zeigt den Fortschritt (im Text-Modus der Serielle Monitor: `Beugung 41/30 OK  Oberarm 9/15  Hand 22/45  Achse 60%`). Ist alles OK: Bestätigungsvibration. Nach dem Einschalten muss einmal eingemessen werden (kein automatisches Einmessen).
4. **Training:** Solange der Winkel im zulässigen Bereich bleibt, passiert nichts.
5. **Überstreckung:** Motor vibriert, bis das Gelenk zurückgeführt wird.
6. **Beenden:** Taste lang drücken; Elektronikmodul zum Waschen entnehmen.

**Prototyp 2 (Flex):** Einmessen ist kürzer: Arm entspannt gestreckt halten, Taste kurz drücken (oder Enter bzw. Knopf «Einmessen» im GUI), 2 s still halten, fertig (Vibrationsstoss 300 ms). Kein freies Bewegen. Status-LED: dauernd an = bereit, langsam blinkend = keine Referenz / Einmessen, schnell blinkend = Fehler. Ein- und Ausschalten per langem Tastendruck gibt es noch nicht.

## Zentrale quantitative Anforderungen

| ID | Anforderung | Zielwert |
|---|---|---|
| QA-01 | Messbereich / Abtastrate / Auflösung | −100° (Beugung) bis +15° (Überstreckung) / ≥ 25 Hz / ≤ 1° |
| QA-02 | Einmessen der Referenz | 2 s ruhig (höchstens 30 s Zeit), dann frei bewegen (höchstens 30 s), dazu eine Wartezeit auf die Bestätigung (höchstens 30 s) (REQ-005), Wiederholgenauigkeit ± 1,5° |
| QA-03 | Grenzwert-Einstellbereich | 0° bis +10°, Schrittweite ≤ 2° |
| QA-04 | Erkennung Grenzwertüberschreitung | Toleranz ≤ ± 2°, Erkennungsrate ≥ 95 % |
| QA-05 | Feedback-Latenz | ≤ 100 ms, Zuverlässigkeit ≥ 98 % |
| QA-06 | Akkubetrieb | ≥ 4 h |
| QA-07 | Gesamtmasse | ≤ 250 g |
| QA-09 | Verrutschen nach 30 min | < 10 mm, Rotation < 3° |
| QA-13 | An-/Ablegen ohne Hilfe | je ≤ 60 s |

Vollständige Liste (QA-01 bis QA-17) auf dem Miro-Board.