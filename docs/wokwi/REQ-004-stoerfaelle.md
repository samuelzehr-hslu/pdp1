# Wokwi-Plan zu REQ-004 (Einmess-Schritte bestätigen)

**Stand (Auftrag 1, 2026-11-13): Wokwi-Kontingent ist diesen Monat aufgebraucht (Team-Entscheid 2026-11-12). Es wurde nichts ausgeführt, keine Probe gemacht, keine Szenarien-Datei angepasst oder neu geschrieben.** Alles, was Wokwi prüfen würde, steht stattdessen in `docs/tests/board-test-REQ-004.md`. Diese Seite sagt, was zu tun ist, sobald Wokwi wieder geht.

## Kann `wokwi-cli` Zeichen an den Seriellen Monitor schicken?

**Ja, laut Doku** (<https://docs.wokwi.com/wokwi-ci/automation-scenarios>): Der Szenario-Schritt `write-serial` schickt Text oder eine Liste von Zahlen (Zeichencodes) an den Seriellen Eingang:

```yaml
- write-serial: 'x'
- write-serial: [32]        # Leerzeichen
- write-serial: [13, 10]    # CR + NL
```

**Nicht in der Doku geklärt (beim nächsten Wokwi-Lauf kurz mit einem Mini-Programm in `scratch/` ausprobieren, nicht mit der Firmware):**

1. Ob in einem Text in Anführungszeichen `\n` als Zeilenumbruch ankommt. Sicher geht die Zahlenliste (`[10]`, `[13]`, `[32]`).
2. Ob `write-serial` das **Zeichen einzeln** liefert (wichtig für AK-2: `[32, 13, 10]` in einem Schritt = ein `loop()`-Durchlauf ist am Board nicht sicher nachstellbar, in Wokwi ebenfalls nicht garantiert).

Darum gilt: AK-1 und AK-2 bleiben **zusätzlich** am Board (B-1, B-1b, B-2) und in der Logik (`istBestaetigungsZeichen`, alle 256 Zeichen). Wokwi kann sie nur ergänzen.

## Geplante Störfälle (höchstens 5, je höchstens 10 s simulierte Zeit)

Alle mit `diagram_kabelbruch.json`. Zeiten ab Tastendruck t0 (1000 ms nach Start). Motor = Pin 9. Ein Mensch drückt die Taste, darum: Taste in 150-ms-Impulsen (`taste`).

| Nr | Was | Erwartung |
|---|---|---|
| R4-W1 | Nur Taste: Start, Phase 1, Warte-Zustand 1, Taste, Phase 2, Warte-Zustand 2, Taste, Phase 3, Abschluss (Arm macht die Einmess-Bewegung, Wartezeit je 1 s statt 29 s, damit es unter 10 s bleibt) | Pin 9: aus bis t0+1950; an bei t0+2150 (Warte-Stoss 300 ms: t0+2000 bis t0+2300); ein Stoss 100 ms zu Beginn von Phase 2 direkt nach der Taste; Text `Schritt 1 fertig. Enter oder Taste fuer Schritt 2 (Bewegung).` und `Schritt 2 fertig. Enter oder Taste fuer Schritt 3 (Hand drehen).` je genau einmal; zwei Stösse zu Beginn von Phase 3; Bestätigungsvibration 150 ms am Ende; keine Zeile `Einmessen abgebrochen`. |
| R4-W2 | Taste mitten in einer Phase (AK-3): zweimal drücken in Phase 2 | Phase 2 läuft normal weiter, am Ende wartet das Gerät (kein automatisches Weiter). |
| R4-W3 | Warten ohne Taste, Höchstwartezeit (AK-9, 30 s) | Nicht in 10 s prüfbar (30 s simulierte Zeit). Nur am Board (B-8), ausser das Team erlaubt für diesen Fall ein längeres Zeitlimit. |
| R4-W4 | IMU 2 fällt im Warte-Zustand aus (AK-12) | `Einmessen abgebrochen - Grund: IMU`, Fehlermuster, Motor sonst aus. |
| R4-W5 | Überstreckung in Phase und Warte-Zustand (AK-10) | Pin 9 nur Stösse des Einmessens, keine Warnung. |

Pin-Proben wie bisher in die **Mitte** der Fenster setzen (± 50 ms). Der Stoss-Hinweis «Motorstärke 200 von 255» aus `docs/offene-hinweise.md` gilt weiter.

## Welche bestehenden Wokwi-Dateien den alten Ablauf annehmen

Alle diese Dateien gehen vom **alten Ablauf** aus (Phase 2 startet automatisch nach 2000 ms, Phase 3 nach 6000 ms, Bestätigung nach 7500 ms bzw. in den älteren Dateien nach 6000 ms). Sie sind schon seit REQ-002 «Neustart» veraltet (siehe `REQ-002-stoerfaelle.md`) und mit REQ-004 **zusätzlich** falsch (der Ablauf braucht jetzt Taste/Enter nach Phase 1 und Phase 2). **Nicht angepasst und nicht ausgeführt**, weil Wokwi nicht verfügbar ist und eine Anpassung ohne Lauf nicht überprüfbar wäre. Sie sind **«nicht geprüft»**, nie «bestanden».

| Datei | Warum veraltet |
|---|---|
| `szenarien/erzeuge_einmess_szenarien.py` | Funktion `einmessen()` stellt Taste + Bewegung bei t0+2100 bis t0+5900 ohne Bestätigung nach Phase 1 und 2 nach. Muss neu: Taste (t0), Phase 1, Taste nach dem Warte-Stoss, Phase 2, Taste, Phase 3. Danach die erzeugten `.yaml` neu schreiben. |
| `szenarien/req002_w3_nicht_ruhig.yaml`, `req002_w4_imu_ausfall_in_phase2.yaml`, `req002_w5_einmessen_ok.yaml`, `req002_w6_oberarm_still.yaml` (erzeugt) | wie oben (Zeiten der Stösse, Bestätigung, Abbruch). |
| `szenarien/req002_w2_einmessen_ohne_bewegung.yaml` (von Hand) | Erwartet Abbruch-Zeile bei t0+6000 ms. Neu: Abbruch kommt nach Phase 2 erst nach der Bestätigung für Phase 2, also später. |
| `szenarien/ak04_ausfall_bei_ueberstreckung.yaml`, `ak08_wiederkehr_mitten_im_stoss.yaml`, `ak10_einmessen_abbruch.yaml`, `ak10_einmessen_bei_ausfall.yaml`, Schluss von `ak11_ohne_referenz.yaml` (erzeugt) | Der Vorspann «erfolgreiches Einmessen» (7400 ms) klappt nicht mehr: Das Gerät wartet nach Phase 1 auf eine Bestätigung. |
| `szenarien/pruefe_REQ-001.py` (`VORSPANN_MS`, Prüfung «Vorspann hat geklappt») und `szenarien/pruefe_REQ-002.py` (Zeiten t0+6075 usw.) | Zeiten und Erwartungen gelten für den alten Ablauf. |
| `szenarien/README.md`, `REQ-002-stoerfaelle.md` | Beschreiben den alten Ablauf. Die zweite Datei trägt schon den Hinweis «ÜBERHOLT». |

**Noch gültig** (unverändert, weil sie kein erfolgreiches Einmessen brauchen oder nur in Phase 1 abbrechen): `normal_ohne_stoerung`, `ak02_*`, `ak06_*`, `ak10_keine_bestaetigung` (Taste startet, IMU fällt in Phase 1 kurz aus: Abbruch, wie vorher), `req002_w1_einschalten`. Auch sie sind erst «bestanden», wenn Wokwi wieder läuft und sie laufen.

## Was zu tun ist, wenn Wokwi wieder verfügbar ist

1. `erzeuge_einmess_szenarien.py` auf den neuen Ablauf umbauen (Taste nach jeder Phase, Wartezeit im Szenario nur 1 s), alle erzeugten `.yaml` neu schreiben.
2. `pruefe_REQ-001.py` und `pruefe_REQ-002.py` anpassen (Vorspann-Dauer, Zeiten der Stösse und Abbruch-Zeilen), die Szenarien R4-W1, W2, W4, W5 ergänzen.
3. `write-serial` mit einem Mini-Programm ausprobieren (siehe oben).
4. Alles mit einem Befehl im Vordergrund starten (siehe Tester-Regeln).
