# Wokwi-Simulation (wokwi-cli)

Der Developer darf diesen Ordner nicht lesen (wie alles unter `docs/tests/`).

Wokwi simuliert den ganzen Arduino mit Sensoren, Taste und Motor (in der Simulation eine LED als Platzhalter).
Die Schaltung steht in `jointsense/diagram.json`. Diese Datei hier **nicht** kopieren, sie wird nur verwendet.

## Einmalig einrichten

1. `wokwi-cli` installieren (Anleitung: <https://docs.wokwi.com/wokwi-ci/getting-started>).
2. Kostenlosen Token holen: <https://wokwi.com/dashboard/ci>.
3. Token als Umgebungsvariable `WOKWI_CLI_TOKEN` setzen. **Den Token nie ins Repo schreiben!**

`/einrichten` prüft, ob das alles da ist.

## Simulation starten

Zuerst kompilieren (lädt nichts aufs Board):

```bash
arduino-cli compile --fqbn arduino:avr:uno --output-dir scratch/build jointsense
```

Dann simulieren (aus dem Hauptordner des Repos):

```bash
wokwi-cli docs/wokwi --diagram-file ../../jointsense/diagram.json --timeout 10000 --serial-log-file scratch/wokwi-serial.log
```

Was die Optionen bedeuten:

| Option | Bedeutung |
|---|---|
| `docs/wokwi` | Ordner mit `wokwi.toml` |
| `--diagram-file` | Pfad zur Schaltung, relativ zu `docs/wokwi/` |
| `--timeout 10000` | Simulation nach 10 000 ms (simulierte Zeit) beenden |
| `--serial-log-file` | Ausgabe des Seriellen Monitors speichern. Der Pfad zählt vom **Hauptordner** aus (nicht von `docs/wokwi/`), also `scratch/...` und nicht `../../scratch/...` (sonst stürzt `wokwi-cli` ab) |
| `--expect-text "IMU 1 (Oberarm): OK"` | Test bestanden nur, wenn dieser Text erscheint |
| `--fail-text "FEHLER"` | Test fehlgeschlagen, sobald dieser Text erscheint |
| `--scenario szenarien/xyz.yaml` | Automatischer Ablauf (Taste drücken, Sensorwerte ändern) |

Gut zu wissen: Sobald alle `--expect-text` gefunden sind, beendet `wokwi-cli` die Simulation sofort, auch vor Ablauf des Timeouts.

## Was der simulierte Sensor kann (MPU6050)

Mit `set-control` in einem Szenario lassen sich diese Regler stellen (Quelle: <https://docs.wokwi.com/parts/wokwi-mpu6050>):

| Regler | Bedeutung |
|---|---|
| `accelX`, `accelY`, `accelZ` | Beschleunigung in g |
| `rotationX`, `rotationY`, `rotationZ` | Gyroskop in Grad pro Sekunde |
| `temperature` | Temperatur in Grad Celsius |

**Achtung:** Die Gyro-Regler heissen `rotation…`, nicht `gyro…`. Ein falscher Name gibt **keine** Fehlermeldung, er bewirkt einfach nichts. Am 2026-11-06 ausprobiert: `rotationX = 60` ergibt den Rohwert 3930, also 60 °/s im Bereich ±500 °/s.

Vor der Aussage «in Wokwi nicht prüfbar» immer zuerst diese Tabelle anschauen.

## Störfälle (für den Tester)

Szenario-Dateien kommen in `docs/wokwi/szenarien/` (Anleitung dort in `README.md`; für REQ-001 gibt es eine Schaltung mit «Kabeln» zum Abstecken der IMUs und das Skript `pruefe_REQ-001.py`). Typische Störfälle aus `docs/bedrohungen.md`:

- IMU antwortet nicht (z. B. eigene Kopie der Schaltung ohne IMU 2 im Ordner `scratch/`) → Gerät meldet den Fehler, Motor bleibt aus.
- Sensorwerte springen plötzlich → kein Fehlalarm, kein Absturz.
