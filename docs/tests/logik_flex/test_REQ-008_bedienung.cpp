// test_REQ-008_bedienung.cpp  (Prototyp 2 Flex)
// Prüft aus REQ-008: AK-13 (Taste 30 ms entprellt; Serial-Zeichen mit 64-Zeichen-Grenze über serialZeichenVerarbeiten),
// AK-10/AK-4/AK-11 (Blinkmuster der Status-LED) und Fuzz dafür. Bedrohung: B-1.
//
// Taste: die Funktion wird alle 5 ms mit dem rohen Tastenwert aufgerufen ("ms" = Zeit seit Start).
// LED: das Muster wird alle 10 ms abgefragt (Zustand wie in AK-14: 0 keine Referenz, 1 bereit, 2 einmessen,
// 3 Sensor-Ausfall, 4 Motor-Ausfall).

#include "test_hilfe.h"
#include "logik.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

typedef std::function<bool(uint32_t)> RohFkt;   // ist die Taste zur Zeit "ms" gedrückt?

static const uint32_t TASTE_ENTPRELL = 30;       // AK-13

// Spielt die Taste alle 5 ms und liefert die Zeiten (ms), an denen "neu gedrückt" kam.
static std::vector<int> tasteSpielen(uint32_t t0, uint32_t dauerMs, const RohFkt& roh, uint32_t entprell) {
  TasteZustand z{};
  std::vector<int> treffer;
  for (uint32_t ms = 0; ms <= dauerMs; ms += 5) {
    if (tasteNeuGedrueckt(z, roh(ms), t0 + ms, entprell)) treffer.push_back(static_cast<int>(ms));
  }
  return treffer;
}

// Liefert die Muster-Folge einer LED alle 10 ms.
static std::vector<int> ledFolge(uint8_t zustand, uint32_t t0, uint32_t dauerMs) {
  std::vector<int> v;
  for (uint32_t ms = 0; ms < dauerMs; ms += 10) v.push_back(static_cast<int>(ledMuster(zustand, t0 + ms)));
  return v;
}

// Schickt einen Text Zeichen für Zeichen in serialZeichenVerarbeiten (neuer Zustand) und zählt die Bestätigungen.
static int serialAnzahl(const std::string& text) {
  SerialZeile z{};
  int n = 0;
  for (size_t i = 0; i < text.size(); i++) {
    if (static_cast<int>(serialZeichenVerarbeiten(z, text[i])) == static_cast<int>(SERIAL_BESTAETIGUNG)) n++;
  }
  return n;
}

static int steigendeFlanken(const std::vector<int>& v) {
  int n = 0;
  for (size_t i = 1; i < v.size(); i++) {
    if (v[i] == 1 && v[i - 1] == 0) n++;
  }
  return n;
}

// Enthält jedes Fenster von "fensterProben" Werten sowohl eine 0 als auch eine 1?
static bool jedesFensterBlinkt(const std::vector<int>& v, size_t fensterProben) {
  for (size_t start = 0; start + fensterProben <= v.size(); start += fensterProben) {
    bool an = false;
    bool aus = false;
    for (size_t i = start; i < start + fensterProben; i++) {
      if (v[i] == 1) an = true;
      if (v[i] == 0) aus = true;
    }
    if (!(an && aus)) return false;
  }
  return true;
}

static bool nurNullOderEins(const std::vector<int>& v) {
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] != 0 && v[i] != 1) return false;
  }
  return true;
}

static double anteilAn(const std::vector<int>& v) {
  int an = 0;
  for (size_t i = 0; i < v.size(); i++) an += v[i];
  return v.empty() ? 0.0 : static_cast<double>(an) / static_cast<double>(v.size());
}

int main() {
  std::srand(42);

  // ---------------------------------------------------------------- AK-13: Taste
  std::printf("AK-13: Taste, 30 ms entprellt, genau ein Treffer pro Druck\n");
  {
    std::vector<int> t = tasteSpielen(1000, 1000, [](uint32_t) { return false; }, TASTE_ENTPRELL);
    pruefe(t.empty(), "AK-13: nie gedrückt: kein Treffer");
  }
  {
    std::vector<int> t = tasteSpielen(50000, 1000, [](uint32_t ms) { return ms >= 100 && ms < 600; }, TASTE_ENTPRELL);
    pruefe(t.size() == 1, "AK-13: ein sauberer Druck (500 ms) gibt genau einen Treffer");
    pruefe(!t.empty() && t[0] >= 100 && t[0] <= 200, "AK-13: der Treffer kommt höchstens 100 ms nach dem Drücken");
  }
  {
    // Prellen beim Drücken: 100 an, 105 aus, 110 an, 115 aus, 120 an, ab 125 stabil an
    RohFkt roh = [](uint32_t ms) -> bool {
      if (ms < 100 || ms >= 600) return false;
      if (ms < 125) return ((ms - 100) / 5) % 2 == 0;
      return true;
    };
    std::vector<int> t = tasteSpielen(2000, 1000, roh, TASTE_ENTPRELL);
    pruefe(t.size() == 1, "AK-13: Prellen beim Drücken (25 ms Flattern) gibt trotzdem nur einen Treffer");
    pruefe(!t.empty() && t[0] <= 250, "AK-13: der Treffer kommt kurz nach dem Prellen");
  }
  {
    std::vector<int> t = tasteSpielen(3000, 11000, [](uint32_t ms) { return ms >= 100 && ms < 10100; }, TASTE_ENTPRELL);
    pruefe(t.size() == 1, "AK-13: 10 s gedrückt halten gibt nur einen Treffer (kein Dauerfeuer)");
  }
  {
    std::vector<int> t = tasteSpielen(0, 2000,
                                      [](uint32_t ms) { return (ms >= 100 && ms < 600) || (ms >= 1000 && ms < 1500); },
                                      TASTE_ENTPRELL);
    pruefe(t.size() == 2, "AK-13: zwei getrennte Drücke geben zwei Treffer");
  }
  {
    // Prellen beim Loslassen: 600 aus, 605 an, 610 aus, 615 an, 620 aus, ab 625 stabil aus
    RohFkt roh = [](uint32_t ms) -> bool {
      if (ms < 100) return false;
      if (ms < 600) return true;
      if (ms < 625) return ((ms - 600) / 5) % 2 == 1;
      return false;
    };
    std::vector<int> t = tasteSpielen(7000, 1500, roh, TASTE_ENTPRELL);
    pruefe(t.size() == 1, "AK-13: Prellen beim Loslassen zählt nicht als neuer Druck (genau ein Treffer)");
  }
  {
    // Drücken mit Prellen, Loslassen mit Prellen, zweites Drücken mit Prellen
    RohFkt roh = [](uint32_t ms) -> bool {
      if (ms < 100) return false;
      if (ms < 120) return ((ms - 100) / 5) % 2 == 0;
      if (ms < 600) return true;
      if (ms < 620) return ((ms - 600) / 5) % 2 == 1;
      if (ms < 1000) return false;
      if (ms < 1020) return ((ms - 1000) / 5) % 2 == 0;
      if (ms < 1500) return true;
      return false;
    };
    std::vector<int> t = tasteSpielen(9000, 2000, roh, TASTE_ENTPRELL);
    pruefe(t.size() == 2, "AK-13: zwei Drücke, jeweils mit Prellen beim Drücken und Loslassen: genau zwei Treffer");
  }
  {
    // andere Entprellzeit (Parameter wird benutzt): 100 ms. Prellen über 60 ms darf nicht zu mehreren Treffern führen
    RohFkt roh = [](uint32_t ms) -> bool {
      if (ms < 100 || ms >= 800) return false;
      if (ms < 160) return ((ms - 100) / 10) % 2 == 0;
      return true;
    };
    std::vector<int> t = tasteSpielen(0, 1000, roh, 100);
    pruefe(t.size() == 1, "AK-13: Entprellzeit 100 ms (Parameter): Prellen über 60 ms gibt nur einen Treffer");
  }
  {
    std::vector<int> t = tasteSpielen(0, 1000, [](uint32_t ms) { return ms >= 100 && ms < 110; }, TASTE_ENTPRELL);
    zusatz(t.empty(), "Zusatz: ein Störimpuls von nur 10 ms (kürzer als 30 ms) wird nicht als Druck gezählt (REQ lässt offen)");
  }
  {
    std::vector<int> t = tasteSpielen(4294967100u, 1000, [](uint32_t ms) { return ms >= 100 && ms < 600; }, TASTE_ENTPRELL);
    zusatz(t.size() == 1, "Zusatz: Zeitüberlauf (millis läuft über) mitten im Druck: trotzdem genau ein Treffer");
  }

  // ---------------------------------------------------------------- AK-13: Serial-Zeichen (B-1)
  std::printf("AK-13/B-1: serialZeichenVerarbeiten, Bestätigung nur bei '\\n', '\\r' oder ' ' am Zeilenanfang\n");
  {
    const char start[] = {'\n', '\r', ' '};
    const char* namen[] = {"'\\n'", "'\\r'", "' '"};
    for (int i = 0; i < 3; i++) {
      SerialZeile z{};
      char text[100];
      std::snprintf(text, sizeof(text), "AK-13: ein einzelnes %s am Zeilenanfang ergibt genau eine Bestätigung", namen[i]);
      pruefe(static_cast<int>(serialZeichenVerarbeiten(z, start[i])) == static_cast<int>(SERIAL_BESTAETIGUNG), text);
    }
  }
  {
    const char andere[] = {'a', 'A', '1', 'x', '+', '\t', '\\', '\0', '\x7f', static_cast<char>(0xff), static_cast<char>(0x80)};
    bool nie = true;
    for (unsigned i = 0; i < sizeof(andere); i++) {
      SerialZeile z{};
      if (static_cast<int>(serialZeichenVerarbeiten(z, andere[i])) != static_cast<int>(SERIAL_NICHTS)) nie = false;
    }
    pruefe(nie, "AK-13: andere Zeichen (a, A, 1, x, +, Tab, \\0, 0x7f, 0x80, 0xff) ergeben nie eine Bestätigung");
  }
  pruefe(serialAnzahl("abc\n") == 0, "AK-13: 'abc' + Zeilenende: keine Bestätigung (nur am Zeilenanfang)");
  pruefe(serialAnzahl("abc\n\n") == 1, "AK-13: nach einer ganzen Zeile zählt das nächste '\\n' am Zeilenanfang wieder");
  pruefe(serialAnzahl("a b\n") == 0, "AK-13: ein Leerzeichen mitten in der Zeile ergibt keine Bestätigung");
  pruefe(serialAnzahl("x\r") == 0, "AK-13: 'x' + '\\r': keine Bestätigung");
  pruefe(serialAnzahl("a\n ") == 1, "AK-13: von zwei Zeilen ('a' und ' ') bestätigt nur die zweite");
  pruefe(serialAnzahl("\n\n") == 2, "AK-13: zwei leere Zeilen ergeben zwei Bestätigungen");
  pruefe(serialAnzahl("1+x\n") == 0 && serialAnzahl("a1x+") == 0, "AK-13: 'a', '1', 'x', '+' nacheinander: nichts passiert (wie im Board-Test von Prototyp 1)");
  {
    std::string x64(64, 'x');
    std::string x65(65, 'x');
    pruefe(serialAnzahl(x64 + "\n") == 0, "AK-13: Zeile mit genau 64 Zeichen ohne Bestätigungszeichen: keine Bestätigung");
    pruefe(serialAnzahl(x65 + "\n") == 0, "AK-13: Zeile mit 65 Zeichen wird verworfen: keine Bestätigung");
    pruefe(serialAnzahl(x65 + "\n ") == 1, "AK-13: nach der verworfenen Zeile ('\\n' beendet sie) bestätigt das nächste Leerzeichen am Zeilenanfang");
    pruefe(serialAnzahl(x65 + "\r ") == 1, "AK-13: eine verworfene Zeile endet auch mit '\\r' (danach bestätigt ein Leerzeichen am Zeilenanfang)");
    pruefe(serialAnzahl(x65 + "\n\n") == 1, "AK-13: ... und ein '\\n' am Zeilenanfang nach der verworfenen Zeile");
    pruefe(serialAnzahl(std::string(10000, 'x') + "\n") == 0, "AK-13: 10000 Zeichen lange Zeile: keine Bestätigung, kein Absturz");
    pruefe(serialAnzahl("x" + std::string(100, ' ') + "\n") == 0, "AK-13: lange Zeile voller Leerzeichen (nicht am Zeilenanfang): keine Bestätigung");
    pruefe(serialAnzahl("x" + std::string(100, ' ') + "\n\n") == 1, "AK-13: ... und das '\\n' danach bestätigt wieder (die Verwerfung ist beendet)");
    pruefe(serialAnzahl(x65 + " \n") == 0, "AK-13: in einer verworfenen Zeile bestätigt ein Leerzeichen nicht");
  }
  {
    // Fuzz: alle 256 Zeichenwerte, sehr viele Zeichen. Bestätigung nur bei '\n', '\r', ' '.
    SerialZeile z{};
    bool nurErlaubte = true;
    bool nurNullEins = true;
    for (int lauf = 0; lauf < 2000000; lauf++) {
      char c = static_cast<char>(zufallInt(0, 255));
      if (lauf % 7 == 0) c = "\n\r ax"[std::rand() % 5];
      int erg = static_cast<int>(serialZeichenVerarbeiten(z, c));
      if (erg != 0 && erg != 1) nurNullEins = false;
      if (erg == 1 && c != '\n' && c != '\r' && c != ' ') nurErlaubte = false;
    }
    pruefe(nurNullEins, "Fuzz (B-1): 2 Millionen zufällige Zeichen: Rückgabe nur 0 oder 1, kein Absturz");
    pruefe(nurErlaubte, "Fuzz (B-1): eine Bestätigung kommt nie bei einem anderen Zeichen als '\\n', '\\r', ' '");
    // danach: Zeile abschliessen, dann muss ein '\n' am Zeilenanfang wieder bestätigen (nichts bleibt hängen)
    serialZeichenVerarbeiten(z, 'x');
    serialZeichenVerarbeiten(z, '\n');
    pruefe(static_cast<int>(serialZeichenVerarbeiten(z, '\n')) == static_cast<int>(SERIAL_BESTAETIGUNG),
           "Fuzz (B-1): nach dem Zufallsstrom ('x' + '\\n' beenden jede Zeile) bestätigt ein '\\n' wieder");
    // sehr lange Zeile am Stück
    SerialZeile z2{};
    int summe = 0;
    for (int i = 0; i < 3000000; i++) summe += static_cast<int>(serialZeichenVerarbeiten(z2, 'x'));
    summe += static_cast<int>(serialZeichenVerarbeiten(z2, '\n'));
    pruefe(summe == 0, "Fuzz (B-1): 3 Millionen Zeichen ohne Zeilenende, dann '\\n': keine Bestätigung, kein Absturz");
    pruefe(static_cast<int>(serialZeichenVerarbeiten(z2, ' ')) == static_cast<int>(SERIAL_BESTAETIGUNG),
           "Fuzz (B-1): danach funktioniert die Bestätigung wieder");
  }
  zusatz(serialAnzahl("\r\n") >= 1 && serialAnzahl("\r\n") <= 2, "Zusatz: 'CR LF' (Enter im Seriellen Monitor mit 'Beides') ergibt 1 oder 2 Bestätigungen (die REQ lässt das offen; die .ino muss doppelte Auslösung abfangen)");

  // ---------------------------------------------------------------- Status-LED
  std::printf("AK-10, AK-4, AK-11: Blinkmuster der Status-LED\n");
  const uint32_t startzeiten[] = {0u, 12345u, 1000000u};
  for (unsigned s = 0; s < sizeof(startzeiten) / sizeof(startzeiten[0]); s++) {
    uint32_t t0 = startzeiten[s];
    {
      std::vector<int> v = ledFolge(1, t0, 10000);
      bool immerAn = true;
      for (size_t i = 0; i < v.size(); i++) if (v[i] != 1) immerAn = false;
      pruefe(immerAn, "AK-10: Zustand 1 (bereit, Referenz da, kein Fehler): LED leuchtet dauernd");
    }
    const uint8_t langsam[] = {0, 2};
    for (unsigned k = 0; k < 2; k++) {
      std::vector<int> v = ledFolge(langsam[k], t0, 10000);
      int flanken = steigendeFlanken(v);
      char text[120];
      std::snprintf(text, sizeof(text), "AK-10: Zustand %d (%s): blinkt langsam, 9 bis 11 Blinks in 10 s (1 Hz)", langsam[k],
                    langsam[k] == 0 ? "keine Referenz" : "Einmessen");
      pruefe(flanken >= 9 && flanken <= 11, text);
      pruefe(jedesFensterBlinkt(v, 100), "AK-10: bei langsamem Blinken ist in jeder Sekunde die LED sowohl an als auch aus");
    }
    const uint8_t schnell[] = {3, 4};
    for (unsigned k = 0; k < 2; k++) {
      std::vector<int> v = ledFolge(schnell[k], t0, 10000);
      int flanken = steigendeFlanken(v);
      char text[120];
      std::snprintf(text, sizeof(text), "AK-4/AK-11: Zustand %d (%s): blinkt schnell, 47 bis 53 Blinks in 10 s (5 Hz)", schnell[k],
                    schnell[k] == 3 ? "Sensor-Ausfall" : "Motor-Ausfall");
      pruefe(flanken >= 47 && flanken <= 53, text);
      pruefe(jedesFensterBlinkt(v, 20), "AK-4/AK-11: bei schnellem Blinken ist in jeder Fünftelsekunde die LED sowohl an als auch aus");
    }
  }
  {
    // Zusatz: Tastverhältnis nicht extrem (REQ nennt nur die Frequenz)
    std::vector<int> a = ledFolge(0, 0, 10000);
    std::vector<int> b = ledFolge(3, 0, 10000);
    zusatz(anteilAn(a) >= 0.1 && anteilAn(a) <= 0.9, "Zusatz: langsames Blinken: LED ist 10 bis 90 % der Zeit an (gut sichtbar)");
    zusatz(anteilAn(b) >= 0.1 && anteilAn(b) <= 0.9, "Zusatz: schnelles Blinken: LED ist 10 bis 90 % der Zeit an (gut sichtbar)");
    std::vector<int> c = ledFolge(3, 4294966000u, 4000);
    zusatz(steigendeFlanken(c) >= 18 && steigendeFlanken(c) <= 22, "Zusatz: Blinkmuster läuft auch über den millis()-Überlauf weiter");
  }

  // ---------------------------------------------------------------- Fuzz
  std::printf("Fuzz: Unsinn in Taste und LED, nichts stürzt ab\n");
  {
    TasteZustand z{};
    uint32_t t = 0;
    const uint32_t entprellWerte[] = {0u, 1u, 30u, 1000u, 4294967295u};
    for (int lauf = 0; lauf < 300000; lauf++) {
      t += static_cast<uint32_t>(zufallInt(0, 50));
      if (lauf % 1000 == 0) t = static_cast<uint32_t>(zufallInt(0, 2000000000)) * 2u;   // grosse Sprünge, auch rückwärts
      tasteNeuGedrueckt(z, (std::rand() % 2) == 0, t, entprellWerte[std::rand() % 5]);
    }
    pruefe(true, "Fuzz: 300000 zufällige Tastenwerte und Zeiten (auch rückwärts, Entprellung 0 bis 2^32-1): kein Absturz");
  }
  {
    bool alleOk = true;
    for (int lauf = 0; lauf < 200000; lauf++) {
      uint8_t zustand = static_cast<uint8_t>(zufallInt(0, 255));
      uint32_t t = static_cast<uint32_t>(zufallInt(0, 2000000000)) * 2u;
      uint8_t m = ledMuster(zustand, t);
      if (m != 0 && m != 1) alleOk = false;
    }
    pruefe(alleOk, "Fuzz: ledMuster liefert für jeden Zustand (0 bis 255) und jede Zeit nur 0 oder 1");
  }
  {
    std::vector<int> v5 = ledFolge(5, 0, 2000);
    std::vector<int> v255 = ledFolge(255, 0, 2000);
    pruefe(nurNullOderEins(v5) && nurNullOderEins(v255), "Fuzz: unbekannte Zustände 5 und 255 geben 0 oder 1");
  }

  return testErgebnis();
}
