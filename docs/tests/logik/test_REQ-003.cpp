// test_REQ-003.cpp
// Tests zu REQ-003: Motor-LED als Platzhalter für den Vibrationsmotor.
//
// Die REQ hat keine neue Funktion (keine Schnittstelle in logik.h), darum gibt es hier
// keine Fuzz-Tests. Geprüft wird die Verdrahtung: Die Tatsachen aus diagram.json und README
// liest das Python-Skript docs/tests/szenarien/verdrahtung_REQ-003.py und schreibt sie in
// scratch/szenarien/verdrahtung_REQ-003.csv. Hier wird verglichen, und zwar mit der ECHTEN pins.h.
//
// Was hier NICHT geprüft wird (geht nur in Wokwi oder am Board):
//   AK-1 (Motor aus -> LED aus), AK-2 (Motor an -> LED leuchtet) im Betrieb, AK-3 Zeit in Millisekunden.

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

#include "test_hilfe.h"
#include "pins.h"   // die echte Pin-Datei aus jointsense/

static std::map<std::string, std::string> fakten;

static std::string wert(const std::string& schluessel) {
  std::map<std::string, std::string>::const_iterator it = fakten.find(schluessel);
  return it == fakten.end() ? std::string("fehlt") : it->second;
}

static bool laden() {
  std::ifstream datei("scratch/szenarien/verdrahtung_REQ-003.csv");
  if (!datei) return false;
  std::string zeile;
  while (std::getline(datei, zeile)) {
    if (!zeile.empty() && zeile[zeile.size() - 1] == '\r') zeile.erase(zeile.size() - 1);
    std::string::size_type komma = zeile.find(',');
    if (komma == std::string::npos) continue;
    fakten[zeile.substr(0, komma)] = zeile.substr(komma + 1);
  }
  return !fakten.empty();
}

// Prüft die Motor-LED in einer Schaltung. vorne = "diagramm_" (jointsense/diagram.json)
// oder "kopie_" (Kopie für die REQ-001-Störfälle).
// Normale Prüfung, oder bei "zusatz" nur eine Meldung (zählt nie als Fehler, weil nicht in der REQ).
static void p(bool zusatz, bool bedingung, const char* text) {
  if (zusatz) {
    std::printf("  %s  %s\n", bedingung ? "ok     " : "ABWEICHUNG (kein Fehler)", text);
  } else {
    pruefe(bedingung, text);
  }
}

static void pruefeSchaltung(const std::string& vorne, const std::string& name, bool zusatz) {
  const std::string pin = std::to_string(PIN_MOTOR_PWM);
  const std::string z = zusatz ? "Zusatz: " : "";

  p(zusatz, wert(vorne + "datei") == "ok", (z + name + ": Schaltungsdatei lässt sich lesen").c_str());
  p(zusatz, wert(vorne + "led_anzahl") == "1", (z + name + ": genau eine LED mit «Motor» im Namen").c_str());

  // prüft AK-4: LED-Pluspol (Anode) hängt an D9 = PIN_MOTOR_PWM, und an keinem anderen Arduino-Pin
  p(zusatz, wert(vorne + "anode_arduino_pins") == pin,
         (z + "AK-4: " + name + ": LED-Anode hängt nur an D" + pin + " (= PIN_MOTOR_PWM)").c_str());
  // prüft AK-4: 220 Ω in der Leitung
  p(zusatz, wert(vorne + "anode_widerstaende_ohm") == "220",
         (z + "AK-4: " + name + ": genau ein Widerstand mit 220 Ohm zwischen D9 und LED").c_str());
  // prüft AK-4: Kathode an GND (und an sonst keinem Arduino-Pin)
  p(zusatz, wert(vorne + "kathode_arduino_pins") == "GND",
         (z + "AK-4: " + name + ": LED-Kathode hängt nur an GND").c_str());

  // prüft AK-3 (Teil Verdrahtung): zwischen D9 und LED sitzt nur der Widerstand, kein Bauteil, das
  // verzögern oder anders schalten könnte. Dann ändert die LED ihren Zustand im selben Moment wie D9.
  p(zusatz, wert(vorne + "anode_fremde_bauteile") == "keine",
         (z + "AK-3: " + name + ": zwischen D9 und LED-Anode kein weiteres Bauteil").c_str());
  p(zusatz, wert(vorne + "kathode_fremde_bauteile") == "keine" &&
         wert(vorne + "kathode_widerstaende_ohm") == "keine",
         (z + "AK-3: " + name + ": zwischen LED-Kathode und GND nichts (kein Schalter, kein Widerstand)").c_str());

  // prüft AK-1 und AK-2 nur zum Teil: Die Polung stimmt. D9 = HIGH lässt Strom von der Anode zur
  // Kathode fliessen (LED an), D9 = 0 nicht (LED aus). Das Verhalten im Betrieb prüft Wokwi/das Board.
  p(zusatz, wert(vorne + "anode_arduino_pins") == pin && wert(vorne + "kathode_arduino_pins") == "GND",
         (z + "AK-1/AK-2 (Polung): " + name + ": D9 hoch = LED an, D9 = 0 = LED aus").c_str());
}

int main() {
  std::printf("REQ-003: Verdrahtung der Motor-LED\n");

  bool geladen = laden();
  pruefe(geladen, "Verdrahtungsliste gefunden (scratch/szenarien/verdrahtung_REQ-003.csv, "
                  "erzeugt von verdrahtung_REQ-003.py)");
  if (!geladen) return testErgebnis();

  // prüft AK-4: Die Anforderung nennt D9 und PIN_MOTOR_PWM = 9
  pruefe(PIN_MOTOR_PWM == 9, "AK-4: pins.h: PIN_MOTOR_PWM = 9");

  // prüft AK-4: README (Zeile «Treiber IN1») stimmt mit pins.h überein
  pruefe(wert("readme_in1_pin") == std::to_string(PIN_MOTOR_PWM),
         "AK-4: README-Tabelle: D9 -> Treiber IN1 stimmt mit PIN_MOTOR_PWM in pins.h überein");

  // prüft AK-4 und AK-3: jointsense/diagram.json
  pruefeSchaltung("diagramm_", "diagram.json", false);

  // Zusatz: Die Wokwi-Störfälle von REQ-001 laufen auf der Kopie diagram_kabelbruch.json.
  // Hat sie eine andere Motor-Verdrahtung, prüfen jene Szenarien etwas anderes als das Gerät.
  pruefeSchaltung("kopie_", "diagram_kabelbruch.json", true);

  return testErgebnis();
}
