# JointSense Live-GUI

Zeigt beide IMUs live als Graphen und die Überstreckung grün/rot an. Ersetzt den Seriellen Monitor (REQ-006).

## Was ihr braucht

- **Chrome oder Edge** (Computer, nicht Handy). Safari und Firefox gehen **nicht**, sie können kein «Web Serial».
- Firmware mit `#define GUI_MODUS 1` (oben in `jointsense/jointsense.ino`) auf dem Arduino. Das ist die Einstellung im Repo.
- Der **Serielle Monitor der Arduino IDE muss zu sein**. Sonst ist der Port belegt und das GUI kann nicht verbinden.

## So geht es

1. Arduino per USB anschliessen.
2. `jointsense_live.html` doppelklicken (öffnet sich im Browser, falls nicht: rechte Maustaste → Öffnen mit → Chrome/Edge).
3. Oben rechts **Verbinden** klicken und den Arduino auswählen. Beim nächsten Mal verbindet das GUI von selbst.
4. Der Uno startet beim Verbinden neu (ca. 2 s). Danach erscheinen «Arduino gestartet» und die IMU-Meldungen in der Ereignisliste.
5. **Einmessen starten** drückt «Enter» für euch (wie im Seriellen Monitor). Nach Schritt 1 nochmals drücken für Schritt 2.

Ohne Board ausprobieren: unten **Demo-Modus** ankreuzen.

## Knöpfe

| Knopf | Was er macht |
|---|---|
| Verbinden / Trennen | USB-Verbindung auf- und abbauen. Kabel gezogen und wieder eingesteckt: verbindet von selbst neu. |
| Anhalten / Fortsetzen | Graphen einfrieren, um etwas anzuschauen. Statuskarte läuft weiter. |
| Aufnahme (CSV) | Zeichnet alle Datenzeilen auf. Nochmals drücken: Datei wird heruntergeladen (für Excel/Python). |
| Einmessen / Bestätigen | Schickt `\n` an den Arduino. |

## Farben der Statuskarte

Die Farbe kommt aus den Status-Bits der Firmware, das GUI rechnet nichts selbst nach.

| Farbe | Bedeutung |
|---|---|
| Grün «OK» | Arm im sicheren Bereich |
| Rot «ÜBERSTRECKT» | Warnung aktiv, Motor läuft |
| Blau «EINMESSEN» | Einmessen läuft, keine Warnung |
| Orange «SENSORFEHLER» | IMU ausgefallen, Warnung aus |
| Grau | nicht eingemessen oder keine Daten |

## Zurück zum Text im Seriellen Monitor

In `jointsense/jointsense.ino` `#define GUI_MODUS 0` setzen und neu hochladen. Dann wieder 9600 Baud und die bekannten Texte.

## Probleme

| Problem | Lösung |
|---|---|
| «Verbinden fehlgeschlagen» | Serieller Monitor der Arduino IDE schliessen, oder Hochladen läuft gerade. |
| «Keine Datenzeilen – ist GUI_MODUS 1 hochgeladen?» | Auf dem Arduino läuft noch die Text-Firmware. Mit `GUI_MODUS 1` hochladen. |
| Knopf «Verbinden» grau | Falscher Browser. Chrome oder Edge nehmen. |

## Protokoll (für Neugierige)

Steht genau in `docs/anforderungen/REQ-006.md`. Kurz: `D,…` 50× pro Sekunde mit Rohwerten, Abweichung und Status-Bits, `E,…` Ereignisse, `P,…` Einmess-Fortschritt, `G,…` Grenzwert, `Z,…` Zykluszeit.
