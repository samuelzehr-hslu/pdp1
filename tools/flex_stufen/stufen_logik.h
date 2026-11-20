// stufen_logik.h - reine Rechnungen für den Stufen-Test (OHNE Arduino-Befehle).
// Darum kann man diese Datei auch auf dem PC mit g++ testen.
// Die Zeit (millis) wird als Zahl übergeben und nicht hier abgefragt.
//
// "inline" vor einer Funktion heisst nur: Die Funktion darf direkt hier in der .h-Datei stehen.
// So braucht der Sketch keine zusätzliche .cpp-Datei.

#ifndef STUFEN_LOGIK_H
#define STUFEN_LOGIK_H

#include <stdint.h>

// ---------- Feste Werte (gleich wie in jointsense_flex/logik.h) ----------
const float TEILER_WIDERSTAND_OHM = 22000.0f;  // Festwiderstand im Spannungsteiler
const int ADC_MAX = 4095;                      // grösster ADC-Wert bei 12 Bit
const int ADC_GUELTIG_MIN = 100;               // darunter: Kabel ab?
const int ADC_GUELTIG_MAX = 3995;              // darüber: Kurzschluss?

// Ist der ADC-Wert brauchbar?
inline bool adcGueltig(int adc) {
  return adc >= ADC_GUELTIG_MIN && adc <= ADC_GUELTIG_MAX;
}

// Widerstand des Flex-Sensors aus dem Spannungsteiler: R = 22000 x (4095 / adc - 1).
// Bei ungültigem ADC-Wert gibt es -1 (damit darf niemand rechnen).
inline float widerstandOhm(int adc) {
  if (!adcGueltig(adc)) {
    return -1.0f;
  }
  return TEILER_WIDERSTAND_OHM * ((float)ADC_MAX / (float)adc - 1.0f);
}

// Abweichung in Prozent von der Referenz. Positiv = weniger Widerstand = Richtung Überstreckung.
// Ohne Referenz (0 oder kleiner) gibt es 0.
inline float abweichungProzent(float widerstand, float referenz) {
  if (!(referenz > 0.0f)) {
    return 0.0f;
  }
  return (referenz - widerstand) / referenz * 100.0f;
}

// ---------- Grenzwert mit Entprellung und Hysterese ----------
//
// Entprellung: Die Abweichung muss eine Weile (z. B. 80 ms) am Stück über dem Grenzwert sein,
//              sonst löst ein einzelner Ausreisser keinen Alarm aus.
// Hysterese:   Ist der Alarm an, geht er erst aus, wenn die Abweichung klar zurück ist
//              (unter Grenzwert minus Hysterese, z. B. unter 5 - 1 = 4 %).
//              Sonst würde der Alarm an der Grenze dauernd an/aus flackern.
//
//   Abweichung %
//     6 |      ****
//     5 |-----*----*------  Grenzwert: hier AN (nach 80 ms)
//     4 |....*......*.....  Grenzwert - Hysterese: erst hier AUS
//       +--------------------> Zeit

// Was sich die Prüfung zwischen zwei Messungen merken muss.
struct GrenzwertZustand {
  bool zuWeit = false;          // ist der Alarm gerade an?
  bool ueberGrenze = false;     // war die letzte Messung über dem Grenzwert?
  uint32_t ueberSeitMs = 0;     // seit wann ist die Abweichung über dem Grenzwert?
};

// Prüft eine neue Messung. Gibt true zurück, solange der Alarm ("zu weit") an ist.
inline bool grenzwertPruefen(GrenzwertZustand &z, float abweichung, uint32_t jetztMs,
                             float grenzwert, float hysterese, uint32_t entprellMs) {
  // Unsinnige Zahl (NaN: "keine Zahl")? Dann sicherheitshalber alles aus.
  // Trick: NaN ist mit sich selbst nie gleich.
  if (abweichung != abweichung) {
    z = GrenzwertZustand{};
    return false;
  }

  if (z.zuWeit) {
    // Alarm ist an: erst aus, wenn klar unter Grenzwert minus Hysterese.
    if (abweichung < grenzwert - hysterese) {
      z.zuWeit = false;
      z.ueberGrenze = false;
    }
    return z.zuWeit;
  }

  // Alarm ist aus: Ist die Abweichung über dem Grenzwert?
  if (abweichung > grenzwert) {
    if (!z.ueberGrenze) {
      z.ueberGrenze = true;     // erste Messung über dem Grenzwert: Zeit merken
      z.ueberSeitMs = jetztMs;
    }
    if (jetztMs - z.ueberSeitMs >= entprellMs) {
      z.zuWeit = true;          // lange genug drüber: Alarm an
    }
  } else {
    z.ueberGrenze = false;      // wieder drunter: Entprellung beginnt von vorne
  }
  return z.zuWeit;
}

#endif
