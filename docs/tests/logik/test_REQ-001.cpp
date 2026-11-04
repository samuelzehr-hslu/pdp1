// test_REQ-001.cpp
// Tests zu REQ-001: Sensorausfall erkennen und sicher melden.
//
// Geprüft werden NUR die drei Funktionen aus der Schnittstelle der Anforderung:
//   imuStatusAktualisieren(...)   -> zählt Fehlschläge, erkennt "ausgefallen"
//   fehlerMusterMotorAn(...)      -> Vibrationsmuster "3 Stösse alle 5 s"
//   motorSollAn(...)              -> Ausfall hat Vorrang vor Überstreckung
//
// Aufbau dieser Datei:
//   Teil A  imuStatusAktualisieren  (AK-1, AK-2)
//   Teil B  fehlerMusterMotorAn     (AK-5, AK-8) inkl. Zeitüberlauf von millis()
//   Teil C  motorSollAn             (AK-4)
//   Teil D  Zusammenspiel aller drei Funktionen (AK-2, AK-4, AK-5, AK-8)
//   Teil E  Szenarien aus Python (CSV-Dateien in scratch/szenarien/)
//   Teil F  Fuzz-Tests (zufällige und extreme Werte, fester Startwert)
//
// Nicht hier prüfbar (braucht das ganze Gerät, siehe docs/wokwi/szenarien/):
//   AK-1 (Bytes zählen), AK-3, AK-6, AK-7, AK-9, AK-10, AK-11 und die Serial-Texte.
//
// Hinweis zu NaN/INFINITY: Die drei Funktionen bekommen nur bool- und Ganzzahl-Werte,
// Kommazahlen kommen darin nicht vor. Darum gibt es hier keine NaN-Fuzz-Fälle.
// (NaN bei grenzwertPruefen gehört zur bestehenden Funktion und nicht zu REQ-001.)

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "test_hilfe.h"
#include "logik.h"

// ---------------------------------------------------------------------------
// Hilfsfunktionen für die Tests
// ---------------------------------------------------------------------------

// So soll das Muster laut AK-5 aussehen: t = Millisekunden seit Beginn des Musters (0..4999).
// An bei [0,100), [200,300), [400,500). Sonst aus.
static bool musterSoll(uint32_t t) {
  return (t < 100) || (t >= 200 && t < 300) || (t >= 400 && t < 500);
}

// Ein zufälliger 32-Bit-Wert (rand() liefert je nach PC nur 15 Bit, darum mehrfach).
static uint32_t zufall32() {
  uint32_t a = (uint32_t)rand() & 0xFFFFu;
  uint32_t b = (uint32_t)rand() & 0xFFFFu;
  return (a << 16) | b;
}

// Ein "extremer" 32-Bit-Wert: oft an den Rändern, sonst zufällig.
static uint32_t zufallExtrem32() {
  switch (rand() % 8) {
    case 0: return 0u;
    case 1: return 1u;
    case 2: return 0xFFFFFFFFu;
    case 3: return 0xFFFFFFFEu;
    case 4: return 0x80000000u;
    case 5: return 0x7FFFFFFFu;
    default: return zufall32();
  }
}

// ---------------------------------------------------------------------------
// Teil A: imuStatusAktualisieren  (AK-1, AK-2)
// ---------------------------------------------------------------------------
static void testImuStatus() {
  std::printf("-- Teil A: imuStatusAktualisieren\n");

  // AK-2: 1 und 2 Fehlschläge lösen keinen Ausfall aus, der 3. schon.
  {
    ImuStatus s = {0, false};
    bool r1 = imuStatusAktualisieren(s, false, 3);
    pruefe(!r1 && !s.ausgefallen && s.fehlerInFolge == 1,
           "AK-2: 1 Fehlschlag in Folge loest KEINEN Ausfall aus");
    bool r2 = imuStatusAktualisieren(s, false, 3);
    pruefe(!r2 && !s.ausgefallen && s.fehlerInFolge == 2,
           "AK-2: 2 Fehlschlaege in Folge loesen KEINEN Ausfall aus");
    bool r3 = imuStatusAktualisieren(s, false, 3);
    pruefe(r3 && s.ausgefallen && s.fehlerInFolge == 3,
           "AK-2: 3 Fehlschlaege in Folge = IMU gilt als ausgefallen");
    bool r4 = imuStatusAktualisieren(s, false, 3);
    pruefe(r4 && s.ausgefallen && s.fehlerInFolge == 4,
           "AK-2: weitere Fehlschlaege: IMU bleibt ausgefallen");
  }

  // AK-2: Eine erfolgreiche Messung setzt den Zähler auf 0.
  {
    ImuStatus s = {0, false};
    imuStatusAktualisieren(s, false, 3);
    imuStatusAktualisieren(s, false, 3);
    bool r = imuStatusAktualisieren(s, true, 3);
    pruefe(!r && !s.ausgefallen && s.fehlerInFolge == 0,
           "AK-2: erfolgreiche Messung setzt Zaehler auf 0");
    imuStatusAktualisieren(s, false, 3);
    bool r2 = imuStatusAktualisieren(s, false, 3);
    pruefe(!r2 && !s.ausgefallen && s.fehlerInFolge == 2,
           "AK-2: nach Erfolg zaehlt man wieder bei 0 (2 Fehlschlaege sind kein Ausfall)");
  }

  // AK-2: "in Folge": Fehlschlag, Fehlschlag, Erfolg, Fehlschlag, Fehlschlag ... nie Ausfall.
  {
    ImuStatus s = {0, false};
    bool jemalsAusfall = false;
    for (int i = 0; i < 1000; i++) {
      imuStatusAktualisieren(s, false, 3);
      if (s.ausgefallen) jemalsAusfall = true;
      imuStatusAktualisieren(s, false, 3);
      if (s.ausgefallen) jemalsAusfall = true;
      imuStatusAktualisieren(s, true, 3);
      if (s.ausgefallen) jemalsAusfall = true;
    }
    pruefe(!jemalsAusfall, "AK-2: Muster 'fehl, fehl, ok' (nie 3 in Folge) ergibt nie einen Ausfall");
  }

  // AK-8 (Wiederkehr): Eine erfolgreiche Messung beendet den Ausfall sofort.
  {
    ImuStatus s = {0, false};
    for (int i = 0; i < 10; i++) imuStatusAktualisieren(s, false, 3);
    pruefe(s.ausgefallen, "Vorbereitung: IMU ist ausgefallen");
    bool r = imuStatusAktualisieren(s, true, 3);
    pruefe(!r && !s.ausgefallen && s.fehlerInFolge == 0,
           "AK-8: eine einzige erfolgreiche Messung beendet den Ausfall");
  }

  // AK-1: Jede IMU wird einzeln beurteilt (zwei getrennte Zustände).
  {
    ImuStatus oben = {0, false};
    ImuStatus unten = {0, false};
    for (int i = 0; i < 3; i++) {
      imuStatusAktualisieren(oben, true, 3);
      imuStatusAktualisieren(unten, false, 3);
    }
    pruefe(!oben.ausgefallen && oben.fehlerInFolge == 0,
           "AK-1: IMU 1 laeuft, wird nicht als ausgefallen gezaehlt");
    pruefe(unten.ausgefallen, "AK-1: IMU 2 faellt aus, unabhaengig von IMU 1");
  }

  // Die Schwelle ist ein Parameter (im Betrieb 3).
  {
    ImuStatus s = {0, false};
    bool r = false;
    for (int i = 1; i <= 4; i++) {
      r = imuStatusAktualisieren(s, false, 5);
      pruefe(!r, "Schwelle 5: nach weniger als 5 Fehlschlaegen noch kein Ausfall");
    }
    r = imuStatusAktualisieren(s, false, 5);
    pruefe(r && s.ausgefallen, "Schwelle 5: beim 5. Fehlschlag Ausfall");
  }
  {
    ImuStatus s = {0, false};
    bool r = imuStatusAktualisieren(s, false, 1);
    pruefe(r && s.ausgefallen, "Schwelle 1: schon der 1. Fehlschlag ist ein Ausfall");
  }

  // Grenzwert: Schwelle 0 wird wie 1 behandelt.
  {
    ImuStatus s = {0, false};
    bool r = imuStatusAktualisieren(s, false, 0);
    pruefe(r && s.ausgefallen && s.fehlerInFolge == 1,
           "Grenzwert: Schwelle 0 wird wie 1 behandelt (1. Fehlschlag = Ausfall)");
    bool r2 = imuStatusAktualisieren(s, true, 0);
    pruefe(!r2 && !s.ausgefallen && s.fehlerInFolge == 0,
           "Grenzwert: Schwelle 0, erfolgreiche Messung = kein Ausfall");
    ImuStatus s2 = {0, false};
    bool r3 = imuStatusAktualisieren(s2, true, 0);
    pruefe(!r3 && !s2.ausgefallen,
           "Grenzwert: Schwelle 0 ohne Fehlschlag = kein Ausfall (nicht dauernd ausgefallen)");
  }

  // Grenzwert: Zähler bleibt bei 255 stehen, kein Überlauf auf 0.
  {
    ImuStatus s = {0, false};
    bool immerAusgefallenAbDrei = true;
    for (int i = 1; i <= 600; i++) {
      bool r = imuStatusAktualisieren(s, false, 3);
      if (i >= 3 && !r) immerAusgefallenAbDrei = false;
    }
    pruefe(s.fehlerInFolge == 255, "Grenzwert: Zaehler bleibt bei 255 stehen (600 Fehlschlaege)");
    pruefe(s.ausgefallen, "Grenzwert: bei Zaehler 255 gilt die IMU weiter als ausgefallen");
    pruefe(immerAusgefallenAbDrei,
           "Grenzwert: kein Ueberlauf, IMU bleibt ab dem 3. Fehlschlag durchgehend ausgefallen");
  }
  {
    ImuStatus s = {254, true};
    bool r1 = imuStatusAktualisieren(s, false, 3);
    pruefe(r1 && s.fehlerInFolge == 255, "Grenzwert: 254 -> 255");
    bool r2 = imuStatusAktualisieren(s, false, 3);
    pruefe(r2 && s.fehlerInFolge == 255 && s.ausgefallen,
           "Grenzwert: 255 + 1 Fehlschlag bleibt 255 und ausgefallen (kein Ueberlauf auf 0)");
    bool r3 = imuStatusAktualisieren(s, true, 3);
    pruefe(!r3 && s.fehlerInFolge == 0 && !s.ausgefallen,
           "Grenzwert: nach 255 Fehlschlaegen genuegt ein Erfolg zur Wiederkehr");
  }

  // Grenzwert: Schwelle 255 (grösste mögliche) funktioniert.
  {
    ImuStatus s = {0, false};
    bool zuFruehAusgefallen = false;
    for (int i = 1; i <= 254; i++) {
      if (imuStatusAktualisieren(s, false, 255)) zuFruehAusgefallen = true;
    }
    pruefe(!zuFruehAusgefallen, "Grenzwert: Schwelle 255, nach 254 Fehlschlaegen noch kein Ausfall");
    bool r = imuStatusAktualisieren(s, false, 255);
    pruefe(r && s.ausgefallen && s.fehlerInFolge == 255,
           "Grenzwert: Schwelle 255, beim 255. Fehlschlag Ausfall");
    bool r2 = imuStatusAktualisieren(s, false, 255);
    pruefe(r2 && s.fehlerInFolge == 255, "Grenzwert: Schwelle 255, Zaehler bleibt bei 255, weiter ausgefallen");
  }
}

// ---------------------------------------------------------------------------
// Teil B: fehlerMusterMotorAn  (AK-5, AK-8)
// ---------------------------------------------------------------------------
static void testFehlerMuster() {
  std::printf("-- Teil B: fehlerMusterMotorAn\n");

  // Ohne Ausfall: Motor aus, Zustand zurückgesetzt.
  {
    FehlerMusterZustand z = {false, 0};
    bool alleAus = true;
    for (uint32_t t = 0; t < 6000; t += 7) {
      if (fehlerMusterMotorAn(z, false, t)) alleAus = false;
    }
    pruefe(alleAus, "AK-4/AK-5: ohne Ausfall ist das Fehlermuster immer aus");
    pruefe(!z.aktiv, "ohne Ausfall ist das Muster nicht aktiv");
  }

  // AK-5: Beim Beginn des Ausfalls startet das Muster SOFORT.
  {
    FehlerMusterZustand z = {false, 0};
    bool r = fehlerMusterMotorAn(z, true, 12345);
    pruefe(r, "AK-5: Muster startet sofort (erster Aufruf = Motor an)");
    pruefe(z.aktiv, "AK-5: Muster ist aktiv");
    pruefe(z.startMs == 12345, "AK-5: Startzeit = Zeit des ersten Aufrufs mit Ausfall");
  }
  // ... auch bei Startzeit 0 und an der Zeit-Obergrenze.
  {
    FehlerMusterZustand z = {false, 0};
    pruefe(fehlerMusterMotorAn(z, true, 0), "AK-5: Muster startet sofort, auch bei Zeit 0");
    FehlerMusterZustand z2 = {false, 0};
    pruefe(fehlerMusterMotorAn(z2, true, 0xFFFFFFFFu),
           "AK-5: Muster startet sofort, auch bei Zeit 4294967295");
  }

  // AK-5: Die genauen Grenzen der Stösse.
  {
    struct Punkt { uint32_t t; bool soll; };
    const Punkt punkte[] = {
      {0, true}, {1, true}, {50, true}, {99, true}, {100, false}, {101, false},
      {150, false}, {199, false}, {200, true}, {250, true}, {299, true}, {300, false},
      {350, false}, {399, false}, {400, true}, {450, true}, {499, true}, {500, false},
      {501, false}, {1000, false}, {2500, false}, {4999, false},
      {5000, true}, {5099, true}, {5100, false}, {5200, true}, {5499, true}, {5500, false},
      {9999, false}, {10000, true}, {10100, false}, {10200, true}, {10400, true}, {10500, false}
    };
    int fehler = 0;
    for (unsigned i = 0; i < sizeof(punkte) / sizeof(punkte[0]); i++) {
      FehlerMusterZustand z = {false, 0};
      fehlerMusterMotorAn(z, true, 1000);                    // Ausfall beginnt bei 1000
      bool r = fehlerMusterMotorAn(z, true, 1000 + punkte[i].t);
      if (r != punkte[i].soll) {
        std::printf("          falsch bei %lu ms nach Beginn: ist %d, soll %d\n",
                    (unsigned long)punkte[i].t, (int)r, (int)punkte[i].soll);
        fehler++;
      }
    }
    pruefe(fehler == 0, "AK-5: Stoss-Grenzen stimmen (an: 0-99, 200-299, 400-499; sonst aus)");
  }

  // AK-5: Jede Millisekunde über 3 Perioden (alle 5000 ms von vorn), 300 ms an pro Periode.
  {
    FehlerMusterZustand z = {false, 0};
    const uint32_t start = 777;
    int falsch = 0;
    int anProPeriode[3] = {0, 0, 0};
    for (uint32_t off = 0; off < 15000; off++) {
      bool r = fehlerMusterMotorAn(z, true, start + off);
      if (r != musterSoll(off % 5000u)) falsch++;
      if (r) anProPeriode[off / 5000u]++;
    }
    pruefe(falsch == 0, "AK-5: Muster stimmt in jeder Millisekunde (3 Perioden, Start zu Start 5000 ms)");
    pruefe(anProPeriode[0] == 300 && anProPeriode[1] == 300 && anProPeriode[2] == 300,
           "AK-5: pro Periode ist der Motor genau 300 ms an (3 Stoesse zu 100 ms)");
  }

  // AK-5: So wie die Schleife es macht: alle 20 ms ein Aufruf (50 Hz), über 60 Sekunden.
  {
    FehlerMusterZustand z = {false, 0};
    const uint32_t start = 100000;
    int falsch = 0;
    for (uint32_t off = 0; off < 60000; off += 20) {
      bool r = fehlerMusterMotorAn(z, true, start + off);
      if (r != musterSoll(off % 5000u)) falsch++;
    }
    pruefe(falsch == 0, "AK-5: Muster stimmt auch bei Aufrufen alle 20 ms (60 s lang)");
  }

  // AK-5: Das Ergebnis hängt nur von der Zeit seit Beginn ab (auch wenn Aufrufe ausfallen / springen).
  {
    FehlerMusterZustand z = {false, 0};
    fehlerMusterMotorAn(z, true, 5000);
    pruefe(!fehlerMusterMotorAn(z, true, 5000 + 7777), "AK-5: Sprung auf t=7777 (= 2777 in der Periode) -> aus");
    pruefe(fehlerMusterMotorAn(z, true, 5000 + 10050), "AK-5: Sprung auf t=10050 (= 50 in der Periode) -> an");
    pruefe(!fehlerMusterMotorAn(z, true, 5000 + 22210), "AK-5: Sprung auf t=22210 (= 2210 in der Periode) -> aus");
    pruefe(fehlerMusterMotorAn(z, true, 5000 + 20450), "AK-5: Sprung auf t=20450 (= 450 in der Periode, 3. Stoss) -> an");
  }

  // AK-8 / AK-5: Ende des Ausfalls -> sofort aus und zurückgesetzt. Neuer Ausfall = neuer Start.
  {
    FehlerMusterZustand z = {false, 0};
    fehlerMusterMotorAn(z, true, 1000);
    bool mitten = fehlerMusterMotorAn(z, true, 1050);   // mitten im 1. Stoss
    pruefe(mitten, "Vorbereitung: mitten im ersten Stoss ist der Motor an");
    bool nachEnde = fehlerMusterMotorAn(z, false, 1060); // Ausfall vorbei
    pruefe(!nachEnde, "AK-8: Ausfall vorbei -> Motor sofort aus (auch mitten im Stoss)");
    pruefe(!z.aktiv, "AK-8: Muster ist nach dem Ausfall nicht mehr aktiv");
    // Neuer Ausfall 90 ms später: Muster beginnt neu, nicht mitten im alten Muster
    bool neu = fehlerMusterMotorAn(z, true, 1150);
    pruefe(neu && z.aktiv && z.startMs == 1150, "AK-5: neuer Ausfall startet das Muster neu (sofort an, neue Startzeit)");
    pruefe(!fehlerMusterMotorAn(z, true, 1150 + 100), "AK-5: neues Muster: nach 100 ms aus");
    pruefe(fehlerMusterMotorAn(z, true, 1150 + 200), "AK-5: neues Muster: nach 200 ms wieder an");
  }

  // Zeitüberlauf von millis(): Ausfall beginnt kurz vor 2^32 - 1, Zeit springt auf 0.
  {
    const uint32_t startWerte[] = {
      0xFFFFFFFFu, 0xFFFFFFFFu - 50u, 0xFFFFFFFFu - 99u, 0xFFFFFFFFu - 100u,
      0xFFFFFFFFu - 250u, 0xFFFFFFFFu - 499u, 0xFFFFFFFFu - 2500u,
      0xFFFFFFFFu - 4999u, 0xFFFFFFFFu - 5000u, 0xFFFFFFFFu - 7777u, 0xFFFFFFFEu, 0x80000000u
    };
    int falsch = 0;
    for (unsigned i = 0; i < sizeof(startWerte) / sizeof(startWerte[0]); i++) {
      FehlerMusterZustand z = {false, 0};
      uint32_t start = startWerte[i];
      for (uint32_t off = 0; off < 16000; off++) {
        uint32_t jetzt = start + off;   // läuft über 4294967295 -> 0 hinweg
        bool r = fehlerMusterMotorAn(z, true, jetzt);
        if (r != musterSoll(off % 5000u)) {
          if (falsch < 5) {
            std::printf("          Zeitueberlauf: Start %lu, Abstand %lu ms: ist %d\n",
                        (unsigned long)start, (unsigned long)off, (int)r);
          }
          falsch++;
        }
      }
    }
    pruefe(falsch == 0, "AK-5: Muster stimmt auch ueber den Zeitueberlauf von millis() hinweg");
  }
  // Zeitüberlauf, Aufrufe alle 20 ms.
  {
    FehlerMusterZustand z = {false, 0};
    uint32_t start = 0u - 3000u;       // 3 Sekunden vor dem Überlauf
    int falsch = 0;
    for (uint32_t off = 0; off < 30000; off += 20) {
      bool r = fehlerMusterMotorAn(z, true, start + off);
      if (r != musterSoll(off % 5000u)) falsch++;
    }
    pruefe(falsch == 0, "AK-5: Zeitueberlauf bei Aufrufen alle 20 ms (3 s vor bis 27 s nach dem Ueberlauf)");
  }
}

// ---------------------------------------------------------------------------
// Teil C: motorSollAn  (AK-4)
// ---------------------------------------------------------------------------
static void testMotorSollAn() {
  std::printf("-- Teil C: motorSollAn\n");

  // Ohne Ausfall gilt zuWeit. Das Fehlermuster wird ignoriert.
  pruefe(!motorSollAn(false, false, false), "ohne Ausfall, nicht zu weit: Motor aus");
  pruefe(motorSollAn(false, true, false), "ohne Ausfall, zu weit: Motor an (normale Warnung)");
  pruefe(!motorSollAn(false, false, true), "ohne Ausfall: Fehlermuster-Wert wird ignoriert (aus)");
  pruefe(motorSollAn(false, true, true), "ohne Ausfall, zu weit: Motor an");

  // Bei Ausfall gilt NUR das Fehlermuster. zuWeit wird ignoriert.
  pruefe(!motorSollAn(true, false, false), "AK-4: Ausfall, Muster aus: Motor aus");
  pruefe(!motorSollAn(true, true, false),
         "AK-4: Ausfall hat Vorrang: auch bei 'zu weit' bleibt der Motor in der Musterpause aus");
  pruefe(motorSollAn(true, false, true), "AK-5: Ausfall, Muster an: Motor an (auch ohne 'zu weit')");
  pruefe(motorSollAn(true, true, true), "AK-5: Ausfall, Muster an: Motor an");
}

// ---------------------------------------------------------------------------
// Teil D/E/F gemeinsam: Eine "Kette" aus allen drei Funktionen + ein einfacher Gegenprüfer
// ---------------------------------------------------------------------------
// Der Gegenprüfer ("Orakel") zählt selbst mit, wie viele Fehlschläge in Folge es gab.
// Er ist absichtlich ganz einfach (nur Zählen), damit er nicht dieselben Fehler hat wie der Code.
struct Kette {
  ImuStatus imu1;
  ImuStatus imu2;
  FehlerMusterZustand muster;
  // Gegenprüfer:
  int fehlIn1;
  int fehlIn2;
  uint32_t streakStart1;
  uint32_t streakStart2;
  bool orakelAusfallVorher;
  uint32_t ausfallStart;
  // Zähler für Fehler und Statistik:
  long schritte;
  long ausfallZeilen;
  long ausfallBeginne;
  long fehlerFlag;       // Ausfall-Erkennung stimmt nicht
  long fehlerMotor;      // Motor ist an/aus, wo er es nicht sein dürfte
  long fehlerMuster;     // Muster ist nach Ausfall noch aktiv
  uint32_t maxErkennungMs;  // längste Zeit vom 1. Fehlschlag bis "ausgefallen"
};

static void ketteStart(Kette &k) {
  k.imu1.fehlerInFolge = 0;
  k.imu1.ausgefallen = false;
  k.imu2.fehlerInFolge = 0;
  k.imu2.ausgefallen = false;
  k.muster.aktiv = false;
  k.muster.startMs = 0;
  k.fehlIn1 = 0;
  k.fehlIn2 = 0;
  k.streakStart1 = 0;
  k.streakStart2 = 0;
  k.orakelAusfallVorher = false;
  k.ausfallStart = 0;
  k.schritte = 0;
  k.ausfallZeilen = 0;
  k.ausfallBeginne = 0;
  k.fehlerFlag = 0;
  k.fehlerMotor = 0;
  k.fehlerMuster = 0;
  k.maxErkennungMs = 0;
}

// Ein Messzyklus: beide IMUs melden "ok oder nicht", dazu das Ergebnis der Grenzwert-Prüfung.
// Gibt zurück, ob der Motor laut Code an sein soll.
static bool ketteSchritt(Kette &k, uint32_t zeit, bool ok1, bool ok2, bool zuWeit) {
  // Gegenprüfer: einfach mitzählen
  if (ok1) { k.fehlIn1 = 0; } else { if (k.fehlIn1 == 0) k.streakStart1 = zeit; k.fehlIn1++; }
  if (ok2) { k.fehlIn2 = 0; } else { if (k.fehlIn2 == 0) k.streakStart2 = zeit; k.fehlIn2++; }
  bool orakelAusfall = (k.fehlIn1 >= 3) || (k.fehlIn2 >= 3);

  // Der echte Code (Schwelle im Betrieb: 3)
  bool a1 = imuStatusAktualisieren(k.imu1, ok1, 3);
  bool a2 = imuStatusAktualisieren(k.imu2, ok2, 3);
  bool ausfall = a1 || a2;
  bool musterAn = fehlerMusterMotorAn(k.muster, ausfall, zeit);
  bool motor = motorSollAn(ausfall, zuWeit, musterAn);

  k.schritte++;
  if (a1 != (k.fehlIn1 >= 3)) k.fehlerFlag++;
  if (a2 != (k.fehlIn2 >= 3)) k.fehlerFlag++;

  // Wie lange hat die Erkennung gedauert (Zeit seit dem 1. Fehlschlag der Serie)?
  if (a1 && k.fehlIn1 == 3) {
    uint32_t dauer = zeit - k.streakStart1;
    if (dauer > k.maxErkennungMs) k.maxErkennungMs = dauer;
  }
  if (a2 && k.fehlIn2 == 3) {
    uint32_t dauer = zeit - k.streakStart2;
    if (dauer > k.maxErkennungMs) k.maxErkennungMs = dauer;
  }

  if (!orakelAusfall) {
    // Kein Ausfall: Motor folgt "zu weit", Muster ist zurückgesetzt (AK-8)
    if (motor != zuWeit) k.fehlerMotor++;
    if (k.muster.aktiv) k.fehlerMuster++;
  } else {
    if (!k.orakelAusfallVorher) {
      k.ausfallStart = zeit;
      k.ausfallBeginne++;
    }
    k.ausfallZeilen++;
    // Bei Ausfall: nur das Muster zählt, "zu weit" darf KEINE Rolle spielen (AK-4)
    bool soll = musterSoll((uint32_t)(zeit - k.ausfallStart) % 5000u);
    if (motor != soll) k.fehlerMotor++;
  }
  k.orakelAusfallVorher = orakelAusfall;
  return motor;
}

// ---------------------------------------------------------------------------
// Teil D: Zusammenspiel von Hand durchgespielt (AK-2, AK-4, AK-5, AK-8)
// ---------------------------------------------------------------------------
static void testZusammenspiel() {
  std::printf("-- Teil D: Zusammenspiel\n");

  Kette k;
  ketteStart(k);
  uint32_t t = 10000;

  // Alles ok, Arm überstreckt -> normale Warnung läuft (Motor an).
  bool motor = false;
  for (int i = 0; i < 10; i++, t += 20) motor = ketteSchritt(k, t, true, true, true);
  pruefe(motor, "Vorbereitung: ohne Ausfall und mit Ueberstreckung vibriert der Motor");

  // IMU 2 faellt aus. Waehrend der ersten 2 Fehlschlaege ist es noch kein Ausfall.
  uint32_t ersterFehlschlag = t;
  motor = ketteSchritt(k, t, true, false, true); t += 20;
  pruefe(!k.imu2.ausgefallen, "AK-2: nach 1 Fehlschlag noch kein Ausfall");
  motor = ketteSchritt(k, t, true, false, true); t += 20;
  pruefe(!k.imu2.ausgefallen, "AK-2: nach 2 Fehlschlaegen noch kein Ausfall");
  // 3. Fehlschlag: Ausfall -> Muster startet sofort (erster Stoss = an)
  uint32_t ausfallBeginn = t;
  motor = ketteSchritt(k, t, true, false, true); t += 20;
  pruefe(k.imu2.ausgefallen, "AK-2: nach 3 Fehlschlaegen Ausfall");
  pruefe(motor, "AK-5: Fehler-Vibration startet sofort mit dem ersten Stoss");
  pruefe(ausfallBeginn - ersterFehlschlag <= 100,
         "AK-2: Ausfall wird spaetestens 100 ms nach dem ersten Fehlschlag erkannt (hier 40 ms)");

  // Weiter: bis 100 ms nach Ausfallbeginn an, dann Pause, trotz zuWeit = true (AK-4)
  bool irgendwannAusTrotzZuWeit = false;
  bool nochAnNachErstemStoss = false;
  for (; t < ausfallBeginn + 600; t += 20) {
    motor = ketteSchritt(k, t, true, false, true);
    uint32_t off = t - ausfallBeginn;
    if (off >= 100 && off < 200 && !motor) irgendwannAusTrotzZuWeit = true;
    if (off >= 100 && off < 200 && motor) nochAnNachErstemStoss = true;
  }
  pruefe(irgendwannAusTrotzZuWeit && !nochAnNachErstemStoss,
         "AK-4: Ausfall hat Vorrang: in der Musterpause ist der Motor aus, obwohl der Arm 'zu weit' ist");

  // Ausfall dauert an: nach der Pause beginnt der 2. Stoss
  pruefe(k.fehlerMotor == 0, "AK-4/AK-5: bis hier nie ein falscher Motor-Zustand");

  // Wiederkehr: IMU 2 antwortet wieder, Arm ist NICHT zu weit -> Motor sofort aus (AK-8)
  motor = ketteSchritt(k, t, true, true, false);
  pruefe(!k.imu2.ausgefallen, "AK-8: IMU 2 antwortet wieder -> Ausfall beendet");
  pruefe(!motor, "AK-8: Motor ist im selben Messzyklus aus (hoechstens 20 ms spaeter erlaubt)");
  pruefe(!k.muster.aktiv, "AK-8: Fehlermuster ist zurueckgesetzt");
  t += 20;

  // Ausfall mitten in einem Stoss, dann Wiederkehr: Motor sofort aus.
  Kette k2;
  ketteStart(k2);
  uint32_t u = 50000;
  for (int i = 0; i < 3; i++, u += 20) ketteSchritt(k2, u, false, true, false);   // Ausfall IMU 1
  bool mitten = ketteSchritt(k2, u, false, true, false); u += 20;                  // 4. Fehlschlag, t=20 ms
  pruefe(mitten, "AK-5: 20 ms nach Beginn ist der Motor noch im 1. Stoss (an)");
  bool nachWieder = ketteSchritt(k2, u, true, true, false);
  pruefe(!nachWieder, "AK-8: Wiederkehr mitten im Stoss -> Motor sofort aus");

  // Beide IMUs fallen aus; eine kommt zurück -> Ausfall dauert an (mindestens eine ausgefallen).
  Kette k3;
  ketteStart(k3);
  uint32_t v = 0;
  for (int i = 0; i < 5; i++, v += 20) ketteSchritt(k3, v, false, false, true);
  pruefe(k3.imu1.ausgefallen && k3.imu2.ausgefallen, "AK-1: beide IMUs gelten einzeln als ausgefallen");
  ketteSchritt(k3, v, true, false, true); v += 20;
  pruefe(!k3.imu1.ausgefallen && k3.imu2.ausgefallen,
         "AK-8: IMU 1 kommt zurueck, IMU 2 nicht -> IMU 2 bleibt ausgefallen");
  pruefe(k3.muster.aktiv, "AK-4: solange mindestens eine IMU ausgefallen ist, laeuft das Fehlermuster weiter");
  ketteSchritt(k3, v, true, true, false); v += 20;
  pruefe(!k3.muster.aktiv && !k3.imu1.ausgefallen && !k3.imu2.ausgefallen,
         "AK-8: beide antworten wieder -> Ausfall vorbei");
  pruefe(k3.fehlerFlag == 0 && k3.fehlerMotor == 0 && k3.fehlerMuster == 0,
         "AK-2/AK-4/AK-5/AK-8: kein Fehler in der gesamten Abfolge");

  // Wackelkontakt: IMU kommt kurz zurück und fällt wieder aus -> Muster beginnt neu (akzeptiert laut REQ).
  Kette k4;
  ketteStart(k4);
  uint32_t w = 1000;
  for (int i = 0; i < 3; i++, w += 20) ketteSchritt(k4, w, true, false, false);
  uint32_t ersterStart = k4.muster.startMs;
  for (int i = 0; i < 4; i++, w += 20) ketteSchritt(k4, w, true, true, false);
  for (int i = 0; i < 3; i++, w += 20) ketteSchritt(k4, w, true, false, false);
  pruefe(k4.muster.aktiv && k4.muster.startMs != ersterStart,
         "AK-8: nach Wiederkehr und neuem Ausfall startet das Muster mit neuer Startzeit");
  pruefe(k4.fehlerMotor == 0 && k4.fehlerFlag == 0, "Wackelkontakt: Motor und Erkennung stimmen");
}

// ---------------------------------------------------------------------------
// Teil E: Szenarien aus Python (docs/tests/szenarien/ausfall_verlaeufe.py)
// ---------------------------------------------------------------------------
// Die CSV-Dateien haben die Spalten:  zeit_ms, imu1_ok, imu2_ok, zu_weit
// (1 = ja, 0 = nein). Python erzeugt NUR diese Eingaben. Die Logik wird hier in C++ geprüft.

// Sucht die CSV-Datei an ein paar möglichen Orten (je nachdem, von wo der Test gestartet wird).
static FILE* csvOeffnen(const char* name) {
  const char* ordner[] = {"scratch/szenarien/", "../scratch/szenarien/",
                          "../../scratch/szenarien/", "../../../scratch/szenarien/"};
  for (unsigned i = 0; i < sizeof(ordner) / sizeof(ordner[0]); i++) {
    char pfad[300];
    std::snprintf(pfad, sizeof(pfad), "%s%s.csv", ordner[i], name);
    FILE* f = std::fopen(pfad, "r");
    if (f != nullptr) return f;
  }
  return nullptr;
}

static void szenarioPruefen(const char* name, bool erwarteAusfall, long mindestensBeginne) {
  char text[200];
  FILE* f = csvOeffnen(name);
  std::snprintf(text, sizeof(text), "Szenario '%s': CSV-Datei gefunden (erst 'python3 docs/tests/szenarien/ausfall_verlaeufe.py')", name);
  pruefe(f != nullptr, text);
  if (f == nullptr) return;

  Kette k;
  ketteStart(k);
  char zeile[200];
  bool ersteZeile = true;
  while (std::fgets(zeile, sizeof(zeile), f) != nullptr) {
    if (ersteZeile) { ersteZeile = false; continue; }   // Kopfzeile
    unsigned long long zeit = 0;
    int ok1 = 1, ok2 = 1, zuWeit = 0;
    if (std::sscanf(zeile, "%llu,%d,%d,%d", &zeit, &ok1, &ok2, &zuWeit) != 4) continue;
    ketteSchritt(k, (uint32_t)zeit, ok1 != 0, ok2 != 0, zuWeit != 0);
  }
  std::fclose(f);

  std::snprintf(text, sizeof(text), "Szenario '%s': enthaelt genug Messzeilen (%ld)", name, k.schritte);
  pruefe(k.schritte >= 100, text);
  if (erwarteAusfall) {
    std::snprintf(text, sizeof(text), "Szenario '%s': Ausfall kommt vor und wird mindestens %ld mal erkannt (%ld mal)",
                  name, mindestensBeginne, k.ausfallBeginne);
    pruefe(k.ausfallBeginne >= mindestensBeginne, text);
  } else {
    std::snprintf(text, sizeof(text), "AK-2: Szenario '%s': kein Ausfall erkannt (nie 3 Fehlschlaege in Folge)", name);
    pruefe(k.ausfallZeilen == 0, text);
  }
  std::snprintf(text, sizeof(text), "AK-2: Szenario '%s': Ausfall genau dann erkannt, wenn 3 Fehlschlaege in Folge (Fehler: %ld)", name, k.fehlerFlag);
  pruefe(k.fehlerFlag == 0, text);
  std::snprintf(text, sizeof(text), "AK-4/AK-5/AK-8: Szenario '%s': Motor immer richtig (bei Ausfall nur Muster, sonst 'zu weit'; Fehler: %ld)", name, k.fehlerMotor);
  pruefe(k.fehlerMotor == 0, text);
  std::snprintf(text, sizeof(text), "AK-8: Szenario '%s': Muster nach dem Ausfall immer zurueckgesetzt (Fehler: %ld)", name, k.fehlerMuster);
  pruefe(k.fehlerMuster == 0, text);
  std::snprintf(text, sizeof(text), "AK-2: Szenario '%s': Erkennung hoechstens 100 ms nach dem 1. Fehlschlag (laengste: %lu ms)", name, (unsigned long)k.maxErkennungMs);
  pruefe(k.maxErkennungMs <= 100, text);
}

static void testSzenarien() {
  std::printf("-- Teil E: Szenarien (Python-CSV)\n");
  szenarioPruefen("ausfall_kurze_aussetzer", false, 0);
  szenarioPruefen("ausfall_imu2_dauer", true, 1);
  szenarioPruefen("ausfall_imu1_dauer", true, 1);
  szenarioPruefen("ausfall_beide", true, 1);
  szenarioPruefen("ausfall_wackelkontakt", true, 5);
  szenarioPruefen("ausfall_zeitueberlauf", true, 1);
  szenarioPruefen("ausfall_lang_minuten", true, 1);
  szenarioPruefen("ausfall_unregelmaessige_takte", true, 3);
}

// ---------------------------------------------------------------------------
// Teil F: Fuzz-Tests (zufällige und extreme Werte, fester Startwert)
// ---------------------------------------------------------------------------
static void testFuzz() {
  std::printf("-- Teil F: Fuzz-Tests (srand(42))\n");
  srand(42);

  // F1: imuStatusAktualisieren mit zufälligen Zuständen und Schwellen (auch 0 und 255).
  {
    long fehlerRueck = 0, fehlerOk = 0, fehlerZaehler = 0, fehlerSchwelleErreicht = 0, fehlerSchwelleNicht = 0;
    for (int i = 0; i < 300000; i++) {
      ImuStatus s = {0, false};
      s.fehlerInFolge = (uint8_t)(rand() % 256);
      s.ausgefallen = (rand() % 2) == 1;
      ImuStatus vorher = s;
      bool ok = (rand() % 2) == 1;
      uint8_t schwelle;
      switch (rand() % 5) {
        case 0: schwelle = 0; break;
        case 1: schwelle = 255; break;
        case 2: schwelle = 3; break;
        default: schwelle = (uint8_t)(rand() % 256); break;
      }
      bool r = imuStatusAktualisieren(s, ok, schwelle);
      if (r != s.ausgefallen) fehlerRueck++;
      if (ok) {
        if (s.fehlerInFolge != 0 || s.ausgefallen) fehlerOk++;
      } else {
        unsigned erwartet = (unsigned)vorher.fehlerInFolge + 1u;
        if (erwartet > 255u) erwartet = 255u;
        if (s.fehlerInFolge != erwartet) fehlerZaehler++;
        unsigned schw = (schwelle == 0) ? 1u : (unsigned)schwelle;
        if ((unsigned)s.fehlerInFolge >= schw && !s.ausgefallen) fehlerSchwelleErreicht++;
        if (!vorher.ausgefallen && (unsigned)s.fehlerInFolge < schw && s.ausgefallen) fehlerSchwelleNicht++;
      }
    }
    pruefe(fehlerRueck == 0, "Fuzz: Rueckgabewert ist immer gleich status.ausgefallen");
    pruefe(fehlerOk == 0, "Fuzz AK-2: erfolgreiche Messung setzt immer Zaehler 0 und 'nicht ausgefallen'");
    pruefe(fehlerZaehler == 0, "Fuzz: Zaehler steigt um 1 und bleibt bei 255 stehen (kein Ueberlauf)");
    pruefe(fehlerSchwelleErreicht == 0, "Fuzz AK-2: Zaehler >= Schwelle (0 wie 1) bedeutet immer 'ausgefallen'");
    pruefe(fehlerSchwelleNicht == 0, "Fuzz AK-2: Zaehler unter der Schwelle (vorher nicht ausgefallen) bedeutet nie 'ausgefallen'");
  }

  // F2: fehlerMusterMotorAn mit zufälligem Zustand, zufälliger Zeit (auch 0 und 4294967295).
  {
    long fehlerAus = 0, fehlerBeginn = 0, fehlerLaeuft = 0, fehlerZustand = 0;
    for (int i = 0; i < 300000; i++) {
      FehlerMusterZustand z = {false, 0};
      z.aktiv = (rand() % 2) == 1;
      z.startMs = zufallExtrem32();
      FehlerMusterZustand vorher = z;
      uint32_t jetzt = zufallExtrem32();
      bool ausfall = (rand() % 4) != 0;
      bool r = fehlerMusterMotorAn(z, ausfall, jetzt);
      if (!ausfall) {
        if (r || z.aktiv) fehlerAus++;
      } else if (!vorher.aktiv) {
        if (!r || !z.aktiv || z.startMs != jetzt) fehlerBeginn++;
      } else {
        bool soll = musterSoll((uint32_t)(jetzt - vorher.startMs) % 5000u);
        if (r != soll) fehlerLaeuft++;
        if (!z.aktiv || z.startMs != vorher.startMs) fehlerZustand++;
      }
    }
    pruefe(fehlerAus == 0, "Fuzz AK-8: ohne Ausfall immer Motor aus und Muster zurueckgesetzt");
    pruefe(fehlerBeginn == 0, "Fuzz AK-5: Beginn des Ausfalls: sofort an, Startzeit = jetzt");
    pruefe(fehlerLaeuft == 0, "Fuzz AK-5: laufendes Muster stimmt fuer jede Zeit (auch bei Zeitueberlauf)");
    pruefe(fehlerZustand == 0, "Fuzz: laufendes Muster behaelt seine Startzeit");
  }

  // F3: motorSollAn: alle Kombinationen, fail-safe.
  {
    long fehler = 0;
    for (int i = 0; i < 100000; i++) {
      bool ausfall = (rand() % 2) == 1;
      bool zuWeit = (rand() % 2) == 1;
      bool muster = (rand() % 2) == 1;
      bool r = motorSollAn(ausfall, zuWeit, muster);
      bool soll = ausfall ? muster : zuWeit;
      if (r != soll) fehler++;
      // fail-safe: bei Ausfall und Musterpause nie Motor an
      if (ausfall && !muster && r) fehler++;
    }
    pruefe(fehler == 0, "Fuzz AK-4: bei Ausfall zaehlt nur das Muster, 'zu weit' wird nie beachtet (fail-safe)");
  }

  // F4: Ganze Kette mit zufälligen Ausfällen, zufälligen Takten (1-40 ms) und Start nahe am Zeitüberlauf.
  {
    for (int lauf = 0; lauf < 20; lauf++) {
      Kette k;
      ketteStart(k);
      uint32_t zeit;
      switch (lauf % 4) {
        case 0: zeit = 0; break;
        case 1: zeit = 0u - 20000u; break;   // läuft während des Laufs über den Überlauf
        case 2: zeit = zufall32(); break;
        default: zeit = 0xFFFFFFFFu - 100u; break;
      }
      int wahrscheinlichkeit1 = 50, wahrscheinlichkeit2 = 50;
      for (long schritt = 0; schritt < 20000; schritt++) {
        if (schritt % 200 == 0) {
          const int stufen[] = {0, 10, 50, 90, 99, 100};
          wahrscheinlichkeit1 = stufen[rand() % 6];
          wahrscheinlichkeit2 = stufen[rand() % 6];
        }
        bool ok1 = (rand() % 100) < wahrscheinlichkeit1;
        bool ok2 = (rand() % 100) < wahrscheinlichkeit2;
        bool zuWeit = (rand() % 3) != 0;     // meistens "zu weit": schlimmster Fall
        ketteSchritt(k, zeit, ok1, ok2, zuWeit);
        zeit += (uint32_t)(1 + rand() % 40);
      }
      if (k.fehlerFlag != 0 || k.fehlerMotor != 0 || k.fehlerMuster != 0 || k.maxErkennungMs > 80) {
        std::printf("          Lauf %d: Flag %ld, Motor %ld, Muster %ld, laengste Erkennung %lu ms\n",
                    lauf, k.fehlerFlag, k.fehlerMotor, k.fehlerMuster, (unsigned long)k.maxErkennungMs);
        pruefe(false, "Fuzz: Kette (Erkennung + Muster + Motor) mit Zufallsfolgen fehlerfrei");
        return;
      }
    }
    pruefe(true, "Fuzz AK-2/4/5/8: Kette aus allen drei Funktionen mit Zufallsfolgen (20 Laeufe x 20000 Zyklen) fehlerfrei");
  }
}

int main() {
  testImuStatus();
  testFehlerMuster();
  testMotorSollAn();
  testZusammenspiel();
  testSzenarien();
  testFuzz();
  return testErgebnis();
}
