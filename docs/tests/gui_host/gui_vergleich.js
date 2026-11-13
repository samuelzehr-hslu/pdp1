// gui_vergleich.js  (REQ-007 AK-8, nur Tests)
// Spielt aufgezeichnete Zeilen (G, E, D, P, Z) in die GUI-Datei ein, OHNE Browser: mit node und einem Attrappen-"Browser".
// Alles, was die Seite anzeigt (Texte, Farben, Klassen, Zeichenbefehle der Graphen, CSV-Aufnahme), wird in eine Datei geschrieben.
// Läuft das Skript mit der Datei VOR und NACH dem Aufräumen, müssen die Dateien gleich sein.
//
// Aufruf (aus dem Hauptordner):
//   node docs/tests/gui_host/gui_vergleich.js <gui.html> <ausgabe-ordner> [<ordner-mit-aufzeichnungen>]
// Die Aufzeichnungen sind die *_m1.txt Dateien von docs/tests/ino_host (Standard: scratch/REQ-007-ausgangslage/ino_host).
// Setzt GUI_VOLL=1, werden auch alle Zeichenbefehle einzeln geschrieben (gross), sonst nur eine Prüfsumme je Bild.
// Exit-Code 0 = gelaufen (Vergleich macht das Skript gui_vergleich.py).

process.env.TZ = "UTC";   // gleiche Uhrzeit-Anzeige auf jedem PC
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const crypto = require("crypto");

const htmlPfad = process.argv[2];
const ausOrdner = process.argv[3];
const aufzOrdner = process.argv[4] || path.join("scratch", "REQ-007-ausgangslage", "ino_host");
if (!htmlPfad || !ausOrdner) {
  console.error("Aufruf: node gui_vergleich.js <gui.html> <ausgabe-ordner> [<aufzeichnungen>]");
  process.exit(2);
}
fs.mkdirSync(ausOrdner, { recursive: true });

const html = fs.readFileSync(htmlPfad, "utf8");
const anfang = html.lastIndexOf("<script>");
const ende = html.lastIndexOf("</script>");
if (anfang < 0 || ende < 0) {
  console.error("Kein <script>-Block gefunden");
  process.exit(2);
}
const skript = html.substring(anfang + "<script>".length, ende);

function runden(x) {
  return typeof x === "number" ? (Number.isFinite(x) ? String(Math.round(x * 100) / 100) : String(x)) : x;
}
function beschreibe(x) {
  if (x && typeof x === "object" && x.__name) return "<" + x.__name + ">";
  if (typeof x === "function") return "[funktion]";
  if (typeof x === "number") return runden(x);
  return JSON.stringify(x);
}

function szenarioLaufen(name, zeilen, aktionen, extra) {
  const dom = [];            // Befehle an die Seite (ohne Zeichnen)
  let T = 10000;             // performance.now()
  let ops = [];              // Zeichenbefehle seit dem letzten Bild
  let bildNr = 0;
  const handler = {};
  const intervalle = [];
  const timeouts = [];
  let rafFunktion = null;
  let zaehlerNeu = 0;
  const elemente = {};
  const voll = process.env.GUI_VOLL === "1";
  const logDom = (text) => dom.push("[" + Math.round(T) + "] " + text);

  const ctx = new Proxy({}, {
    get(ziel, p) {
      if (typeof p === "symbol") return undefined;
      if (p in ziel) return ziel[p];
      if (p === "measureText") return (t) => ({ width: String(t).length * 6 });
      if (p === "createLinearGradient" || p === "createRadialGradient") {
        return (...a) => { ops.push(p + "(" + a.map(runden) + ")"); return { addColorStop: (o, c) => ops.push("stop(" + o + "," + c + ")") }; };
      }
      return (...a) => { ops.push(p + "(" + a.map(runden) + ")"); };
    },
    set(ziel, p, v) { ziel[p] = v; ops.push(p + "=" + beschreibe(v)); return true; },
  });

  const METHODEN = ["remove", "click", "focus", "blur", "setAttribute", "removeAttribute", "scrollIntoView", "removeChild",
    "insertBefore", "replaceChildren", "contains", "closest", "cloneNode", "querySelector", "querySelectorAll", "dispatchEvent"];

  function neuesElement(elName) {
    const speicher = {};
    const kinder = [];
    const stil = new Proxy({}, {
      get(z, p) {
        if (typeof p === "symbol") return undefined;
        if (p === "setProperty") return (k, v) => logDom(elName + ".style.setProperty(" + k + "," + v + ")");
        return p in z ? z[p] : "";
      },
      set(z, p, v) { z[p] = v; logDom(elName + ".style." + String(p) + " = " + beschreibe(v)); return true; },
    });
    const klassen = {
      add: (...a) => logDom(elName + ".classList.add(" + a + ")"),
      remove: (...a) => logDom(elName + ".classList.remove(" + a + ")"),
      toggle: (...a) => { logDom(elName + ".classList.toggle(" + a + ")"); return false; },
      contains: () => false,
    };
    const el = new Proxy({ __name: elName }, {
      get(z, p) {
        if (typeof p === "symbol") return undefined;
        if (p === "__name") return elName;
        if (p in speicher) return speicher[p];
        if (p === "id") return elName;
        if (p === "children") return kinder;
        if (p === "childNodes") return kinder;
        if (p === "firstChild") return kinder[0];
        if (p === "clientWidth") return 800;
        if (p === "clientHeight") return 200;
        if (p === "style") return stil;
        if (p === "classList") return klassen;
        if (p === "dataset") return {};
        if (p === "getContext") return () => ctx;
        if (p === "getBoundingClientRect") return () => ({ width: 800, height: 200, left: 0, top: 0, right: 800, bottom: 200 });
        if (p === "addEventListener") return (art, f) => { handler[elName + ":" + art] = f; };
        if (p === "append" || p === "prepend" || p === "appendChild") {
          return (...a) => { logDom(elName + "." + p + "(" + a.map(beschreibe) + ")"); if (p === "prepend") kinder.unshift(...a); else kinder.push(...a); return a[0]; };
        }
        if (p === "remove") return () => logDom(elName + ".remove()");
        if (p === "click") return () => logDom(elName + ".click()");
        if (METHODEN.indexOf(p) >= 0) return (...a) => { logDom(elName + "." + p + "(" + a.map(beschreibe) + ")"); };
        return undefined;
      },
      set(z, p, v) {
        speicher[p] = v;
        if (p === "width" || p === "height") return true;   // Grösse der Zeichenfläche: kein Anzeige-Befehl
        logDom(elName + "." + String(p) + " = " + beschreibe(v));
        return true;
      },
    });
    return el;
  }

  const document = {
    getElementById(id) {
      if (!elemente[id]) elemente[id] = neuesElement(id);
      return elemente[id];
    },
    createElement(tag) { zaehlerNeu++; return neuesElement("neu:" + tag + "#" + zaehlerNeu); },
    body: neuesElement("body"),
    addEventListener() {},
  };
  const blobs = [];
  class AttrappeBlob {
    constructor(teile, opt) { this.text = teile.join(""); this.art = opt && opt.type; blobs.push(this); }
  }
  const kontext = {
    document,
    window: { devicePixelRatio: 1 },
    getComputedStyle: () => ({ getPropertyValue: () => "monospace" }),
    navigator: { serial: { getPorts: async () => [], requestPort: async () => { throw new Error("kein Port"); }, addEventListener() {} } },
    performance: { now: () => T },
    requestAnimationFrame: (f) => { rafFunktion = f; return 1; },
    setInterval: (f, ms) => { intervalle.push({ f, ms, letzte: T }); return intervalle.length; },
    clearInterval: (id) => { if (intervalle[id - 1]) intervalle[id - 1].aus = true; },
    setTimeout: (f, ms) => { timeouts.push({ f, bei: T + ms }); return timeouts.length; },
    clearTimeout() {},
    Blob: AttrappeBlob,
    URL: { createObjectURL: (b) => "blob:" + (b && b.text ? b.text.length : 0), revokeObjectURL() {} },
    Date: class extends Date {   // feste Uhr: 2026-01-01 12:00 plus die Testzeit (sonst ändert sich jede Uhrzeit-Anzeige)
      constructor(...a) { if (a.length === 0) super(Date.UTC(2026, 0, 1, 12, 0, 0) + T); else super(...a); }
      static now() { return Date.UTC(2026, 0, 1, 12, 0, 0) + T; }
    },
    TextDecoder, TextEncoder, console: { log() {}, error() {}, warn() {} },
  };
  kontext.window.document = document;
  vm.createContext(kontext);

  function fehlerSicher(f, was) {
    try { return f(); } catch (e) { logDom("FEHLER bei " + was + ": " + (e && e.message)); }
    return undefined;
  }
  function bildAbschliessen() {
    const h = crypto.createHash("sha1").update(ops.join("\n")).digest("hex").substring(0, 12);
    dom.push("[" + Math.round(T) + "] BILD " + (++bildNr) + ": " + ops.length + " Zeichenbefehle, Pruefsumme " + h);
    if (voll) for (const o of ops) dom.push("      " + o);
    ops = [];
  }
  function bild() {
    fehlerSicher(() => { if (rafFunktion) rafFunktion(T); }, "Bild zeichnen");
    bildAbschliessen();
  }
  function intervalleBis(neu) {
    // Zeit in 20-ms-Schritten bis 'neu' vorrücken, dabei Intervalle und Zeitgeber auslösen und alle 250 ms ein Bild zeichnen
    while (T + 20 <= neu) {
      T += 20;
      for (const t of timeouts) if (!t.fertig && t.bei <= T) { t.fertig = true; fehlerSicher(t.f, "setTimeout"); }
      for (const iv of intervalle) {
        if (iv.aus) continue;
        while (iv.letzte + iv.ms <= T) { iv.letzte += iv.ms; fehlerSicher(iv.f, "setInterval " + iv.ms); }
      }
      if (Math.round(T) % 250 === 0) bild();
    }
    T = neu;
  }
  function ausloesen(id, art, ereignis) {
    const f = handler[id + ":" + art];
    logDom("AKTION " + id + " " + art + (f ? "" : " (kein Handler)"));
    if (f) fehlerSicher(() => f(ereignis || { target: elemente[id] }), "Aktion " + id);
  }

  fehlerSicher(() => vm.runInContext(skript, kontext), "Seite laden");
  vm.runInContext("Math.random = (function(){ let s = 12345; return function(){ s = (s * 1103515245 + 12345) % 2147483648; return s / 2147483648; }; })();", kontext);
  const verarbeiten = (zeile) => fehlerSicher(() => vm.runInContext("zeileVerarbeiten", kontext)(zeile), "Zeile '" + zeile.substring(0, 40) + "'");

  const start = T;
  intervalleBis(T + 100);
  bild();
  for (const z of zeilen.vorher || []) { logDom("ZEILE " + JSON.stringify(z)); verarbeiten(z); }

  let aktIdx = 0;
  const feed = zeilen.feed || [];
  for (const e of feed) {
    const ziel = start + 100 + e.ms;
    while (aktIdx < aktionen.length && start + 100 + aktionen[aktIdx].ms <= ziel) {
      intervalleBis(start + 100 + aktionen[aktIdx].ms);
      ausloesen(aktionen[aktIdx].id, aktionen[aktIdx].art, aktionen[aktIdx].ereignis);
      aktIdx++;
    }
    intervalleBis(ziel);
    const kurz = e.zeile.charAt(0) === "D" ? null : e.zeile;   // D-Zeilen nicht einzeln aufschreiben (zu viele), E/G/P/Z schon
    if (kurz) logDom("ZEILE " + JSON.stringify(kurz));
    verarbeiten(e.zeile);
  }
  intervalleBis(T + 3000);   // 3 s Stille: "keine Daten"
  bild();
  if (extra) extra({ T: () => T, intervalleBis, ausloesen, bild, logDom, elemente, kontext });
  bild();
  for (const b of blobs) {
    const zl = b.text.split("\n");
    dom.push("BLOB " + b.art + ": " + zl.length + " Zeilen, Pruefsumme " + crypto.createHash("sha1").update(b.text).digest("hex").substring(0, 12));
    dom.push("      " + zl.slice(0, 3).join(" | "));
  }
  fs.writeFileSync(path.join(ausOrdner, "gui_" + name + ".txt"), dom.join("\n") + "\n");
  return dom.length;
}

function feedLesen(datei) {
  const aus = [];
  for (const z of fs.readFileSync(datei, "utf8").split(/\r?\n/)) {
    const m = /^S,(\d+),(.*)$/.exec(z);
    if (m) aus.push({ ms: parseInt(m[1], 10), zeile: m[2] });
  }
  return aus;
}

const UNSINN = ["", "hallo", "D,1,2", "D,", "E,GIBTS_NICHT", "G,abc,def", "G,50", "P,1", "Z,", "Z,abc", "E,", "X,1,2,3", "D,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20"];
const GUT_VOR = ["E,START", "G,50,20"];
const AKTIONEN = [
  { ms: 6000, id: "knopfEinmessen", art: "click" },
  { ms: 8000, id: "knopfPause", art: "click" },
  { ms: 9000, id: "knopfPause", art: "click" },
  { ms: 15000, id: "knopfAufnahme", art: "click" },
  { ms: 25000, id: "knopfAufnahme", art: "click" },
];

const szenarien = ["normal", "abbruch", "ohne_imu2"];
let gesamt = 0;
for (const s of szenarien) {
  const datei = path.join(aufzOrdner, s + "_m1.txt");
  if (!fs.existsSync(datei)) { console.error("Aufzeichnung fehlt: " + datei); process.exit(2); }
  gesamt += szenarioLaufen(s, { vorher: UNSINN.concat(GUT_VOR), feed: feedLesen(datei) }, AKTIONEN);
}
// Demo-Modus der Seite (ohne Arduino)
gesamt += szenarioLaufen("demo", { vorher: [], feed: [] }, [], (h) => {
  const haken = h.kontext.document.getElementById("demoHaken");
  haken.checked = true;
  h.ausloesen("demoHaken", "change", { target: haken });
  h.intervalleBis(h.T() + 3000);
  h.ausloesen("knopfEinmessen", "click");
  h.intervalleBis(h.T() + 12000);
  h.ausloesen("knopfEinmessen", "click");
  h.intervalleBis(h.T() + 12000);
  h.ausloesen("knopfAufnahme", "click");
  h.intervalleBis(h.T() + 4000);
  h.ausloesen("knopfAufnahme", "click");
  haken.checked = false;
  h.ausloesen("demoHaken", "change", { target: haken });
  h.intervalleBis(h.T() + 2000);
});
console.log("GUI-Lauf fertig: " + gesamt + " Zeilen Protokoll in " + ausOrdner);
