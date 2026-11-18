// gui_flex_pruefen.js  (REQ-008 AK-15, nur Tests)
// Prüft das Live-GUI von Prototyp 2 (tools/gui_flex/jointsense_flex_live.html) OHNE Browser, nur mit node.
//
// Wie? Das Skript der Seite läuft in einer Attrappen-Umgebung (kein echter Browser, kein echter Arduino):
//   - ein falscher "Browser" (Elemente, Zeichenfläche, Zeit), der alles aufschreibt, was die Seite anzeigt
//   - ein falscher Web-Serial-Anschluss: die Prüfung schickt Zeilen (G, D, E, kaputte) hinein und sieht, was das GUI
//     an den Arduino zurückschickt (Knopf "Einmessen" = '\n')
// Weil die REQ weder Funktionsnamen noch Element-Namen des GUI festlegt, prüft der Test nur Verhalten:
//   "Wird nach dieser Zeile etwas anderes angezeigt?" und "Steht dieser Text/Wert auf der Seite?".
//
// Aufruf (aus dem Hauptordner):   node docs/tests/gui_flex_host/gui_flex_pruefen.js [pfad-zur-html-datei]
// Exit-Code 0 = alles bestanden, 1 = mindestens ein Fehler, 2 = nicht geprüft (Datei fehlt).
// Braucht node ab Version 18 (wegen stream/web).

process.env.TZ = "UTC";
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const web = require("stream/web");

const htmlPfad = process.argv[2] || path.join("tools", "gui_flex", "jointsense_flex_live.html");
if (!fs.existsSync(htmlPfad)) {
  console.log("nicht geprüft: GUI-Datei fehlt: " + htmlPfad);
  process.exit(2);
}
const html = fs.readFileSync(htmlPfad, "utf8");
let skript = "";
{
  const re = /<script\b([^>]*)>([\s\S]*?)<\/script>/gi;
  let m;
  while ((m = re.exec(html)) !== null) {
    if (!/\bsrc\s*=/i.test(m[1])) skript += m[2] + "\n;\n";
  }
}
if (skript.trim() === "") {
  console.log("FEHLER: kein eingebautes <script> in " + htmlPfad);
  process.exit(1);
}

// ------------------------------------------------------------------ kleine Prüf-Hilfen
let anzahl = 0;
let fehler = 0;
function pruefe(ok, text) {
  anzahl++;
  if (ok) console.log("  ok      " + text);
  else { fehler++; console.log("  FEHLER  " + text); }
}
function zusatz(ok, text) {
  console.log("  " + (ok ? "ok     " : "hinweis") + "  ZUSATZ: " + text);
}
const unbehandelt = [];
process.on("unhandledRejection", (e) => { unbehandelt.push("Promise: " + String((e && e.message) || e)); });
process.on("uncaughtException", (e) => { unbehandelt.push("Fehler: " + String((e && e.message) || e)); });

async function warte(n) {
  for (let i = 0; i < (n || 6); i++) await new Promise((r) => setImmediate(r));
}

function alsText(chunk) {
  if (typeof chunk === "string") return chunk;
  try {
    const b = ArrayBuffer.isView(chunk) ? Buffer.from(chunk.buffer, chunk.byteOffset, chunk.byteLength) : Buffer.from(chunk);
    return b.toString("latin1");
  } catch (e) { return "?"; }
}

function runden(x) {
  return typeof x === "number" ? (Number.isFinite(x) ? String(Math.round(x * 100) / 100) : String(x)) : x;
}
function beschreibe(x) {
  if (x && typeof x === "object" && x.__name) return "<" + x.__name + ">";
  if (typeof x === "function") return "[funktion]";
  if (typeof x === "number") return runden(x);
  try { return JSON.stringify(x); } catch (e) { return String(x); }
}

// ------------------------------------------------------------------ eine Sitzung = eine frische Seite + ein frischer Anschluss
function sitzungBauen() {
  const S = {
    T: 10000,              // "performance.now()" in ms
    schritte: 0,
    anzeige: {},           // "Element.Eigenschaft" -> letzter geschriebener Wert (Text)
    texte: [],             // alle Texte, die je geschrieben wurden
    ops: [],               // Zeichenbefehle der Zeichenflächen
    handler: {},           // "elementName:art" -> Funktion (z. B. "knopfEinmessen:click")
    fensterHandler: {},    // Ereignisse von window/document
    konsole: [],
    fehlerMeldungen: [],   // Fehler, die im GUI-Code geworfen wurden
    geschrieben: [],       // was das GUI an den Port geschickt hat
    intervalle: [],
    timeouts: [],
    elemente: {},
    zaehler: 0,
    zaehlerNeu: 0,
    raf: null,
    regler: null,
    verbunden: false,
    verbindenKnopf: null,
    info: { oeffnen: 0, offen: false, baud: null, angefragt: 0 },
  };

  function merke(schluessel, wert) {
    const s = String(wert);
    S.anzeige[schluessel] = s;
    S.texte.push(s);
  }

  function sicher(f, was) {
    try {
      const r = f();
      if (r && typeof r.catch === "function") r.catch((e) => S.fehlerMeldungen.push(was + ": " + ((e && e.message) || e)));
    } catch (e) {
      S.fehlerMeldungen.push(was + ": " + ((e && e.message) || e));
    }
  }

  // Zeichenfläche: schreibt alle Befehle in S.ops
  const ctx = new Proxy({}, {
    get(ziel, p) {
      if (typeof p === "symbol") return undefined;
      if (p in ziel) return ziel[p];
      if (p === "canvas") return { width: 800, height: 200 };
      if (p === "measureText") return (t) => ({ width: String(t).length * 6 });
      if (p === "createLinearGradient" || p === "createRadialGradient") {
        return (...a) => { S.ops.push(p + "(" + a.map(runden) + ")"); return { addColorStop: (o, c) => S.ops.push("stop(" + o + "," + c + ")") }; };
      }
      return (...a) => { S.ops.push(p + "(" + a.map(runden) + ")"); };
    },
    set(ziel, p, v) { ziel[p] = v; S.ops.push(p + "=" + beschreibe(v)); return true; },
  });

  const KEINE_ANZEIGE = ["scrollTop", "scrollLeft", "width", "height"];
  const METHODEN = ["remove", "click", "focus", "blur", "scrollIntoView", "removeChild", "insertBefore", "replaceChildren",
    "select", "showModal", "close", "setPointerCapture", "releasePointerCapture", "dispatchEvent", "normalize"];

  function neuesElement(elName) {
    const speicher = {};
    const kinder = [];
    const stil = new Proxy({}, {
      get(z, p) {
        if (typeof p === "symbol") return undefined;
        if (p === "setProperty") return (k, v) => merke(elName + ".style." + k, v);
        if (p === "removeProperty") return (k) => merke(elName + ".style." + k, "");
        return p in z ? z[p] : "";
      },
      set(z, p, v) { z[p] = v; merke(elName + ".style." + String(p), v); return true; },
    });
    const klassen = {
      add: (...a) => a.forEach((c) => merke(elName + ".class." + c, "1")),
      remove: (...a) => a.forEach((c) => merke(elName + ".class." + c, "0")),
      toggle: (c, force) => { merke(elName + ".class." + c, force === undefined ? "T" + (++S.zaehler) : (force ? "1" : "0")); return !!force; },
      contains: (c) => S.anzeige[elName + ".class." + c] === "1",
    };
    return new Proxy({ __name: elName }, {
      get(z, p) {
        if (typeof p === "symbol") return undefined;
        if (p === "__name") return elName;
        if (p in speicher) return speicher[p];
        switch (p) {
          case "id": return elName;
          case "children": case "childNodes": return kinder;
          case "firstChild": case "firstElementChild": return kinder[0];
          case "lastChild": case "lastElementChild": return kinder[kinder.length - 1];
          case "clientWidth": case "offsetWidth": return 800;
          case "clientHeight": case "offsetHeight": return 200;
          case "style": return stil;
          case "classList": return klassen;
          case "dataset": return {};
          case "getContext": return () => ctx;
          case "getBoundingClientRect": return () => ({ width: 800, height: 200, left: 0, top: 0, right: 800, bottom: 200 });
          case "addEventListener": return (art, f) => { S.handler[elName + ":" + art] = f; };
          case "removeEventListener": return () => {};
          case "setAttribute": return (k, v) => merke(elName + ".attr." + k, v);
          case "removeAttribute": return (k) => merke(elName + ".attr." + k, "");
          case "getAttribute": return (k) => (S.anzeige[elName + ".attr." + k] !== undefined ? S.anzeige[elName + ".attr." + k] : null);
          case "append": case "prepend": case "appendChild":
            return (...a) => { if (p === "prepend") kinder.unshift(...a); else kinder.push(...a); return a[0]; };
          case "querySelector": return (sel) => elementFuer("sel:" + sel);
          case "querySelectorAll": return () => [];
          case "closest": return () => null;
          case "contains": return () => false;
          case "cloneNode": return () => neuesElement("neu:klon#" + (++S.zaehlerNeu));
          default: break;
        }
        if (METHODEN.indexOf(p) >= 0) return () => {};
        return undefined;
      },
      set(z, p, v) {
        speicher[p] = v;
        if (typeof p === "string" && p.length > 2 && p.substring(0, 2) === "on" && typeof v === "function") {
          S.handler[elName + ":" + p.substring(2)] = v;
          return true;
        }
        if (typeof v === "function" || KEINE_ANZEIGE.indexOf(String(p)) >= 0) return true;
        merke(elName + "." + String(p), v);
        return true;
      },
    });
  }

  function elementFuer(name) {
    if (!S.elemente[name]) S.elemente[name] = neuesElement(name);
    return S.elemente[name];
  }

  const dokument = {
    getElementById: (id) => elementFuer(id),
    createElement: () => neuesElement("neu:element#" + (++S.zaehlerNeu)),
    createElementNS: () => neuesElement("neu:element#" + (++S.zaehlerNeu)),
    createTextNode: (t) => { const e = neuesElement("neu:text#" + (++S.zaehlerNeu)); e.textContent = t; return e; },
    querySelector: (sel) => elementFuer("sel:" + sel),
    querySelectorAll: () => [],
    body: elementFuer("body"),
    documentElement: elementFuer("html"),
    head: elementFuer("head"),
    addEventListener: (art, f) => { (S.fensterHandler[art] = S.fensterHandler[art] || []).push(f); },
    removeEventListener() {},
    hidden: false,
    visibilityState: "visible",
    readyState: "complete",
    title: "",
    fonts: { ready: Promise.resolve() },
  };

  // falscher Web-Serial-Anschluss
  const port = {
    readable: null,
    writable: null,
    async open(opt) {
      if (S.info.offen) throw new Error("InvalidStateError: Port ist schon offen");
      S.info.oeffnen++;
      S.info.offen = true;
      S.info.baud = opt && opt.baudRate;
      port.readable = new web.ReadableStream({ start(c) { S.regler = c; } });
      port.writable = new web.WritableStream({ write(chunk) { S.geschrieben.push(alsText(chunk)); } });
    },
    async close() { S.info.offen = false; },
    async forget() {},
    async setSignals() {},
    getInfo() { return {}; },
    addEventListener() {},
    removeEventListener() {},
  };

  const BASIS = Date.UTC(2026, 0, 1, 12, 0, 0);
  const kontext = {
    document: dokument,
    getComputedStyle: () => ({ getPropertyValue: () => "" }),
    navigator: {
      userAgent: "node-test",
      serial: {
        getPorts: async () => [],
        requestPort: async () => { S.info.angefragt++; return port; },
        addEventListener() {},
        removeEventListener() {},
      },
    },
    performance: { now: () => S.T },
    requestAnimationFrame: (f) => { S.raf = f; return 1; },
    cancelAnimationFrame() {},
    setInterval: (f, ms) => { S.intervalle.push({ f, ms, letzte: S.T }); return S.intervalle.length; },
    clearInterval: (id) => { if (S.intervalle[id - 1]) S.intervalle[id - 1].aus = true; },
    setTimeout: (f, ms) => { S.timeouts.push({ f, bei: S.T + (ms || 0) }); return S.timeouts.length; },
    clearTimeout: (id) => { if (S.timeouts[id - 1]) S.timeouts[id - 1].fertig = true; },
    Blob: class { constructor(teile) { this.text = (teile || []).join(""); } },
    URL: { createObjectURL: () => "blob:test", revokeObjectURL() {} },
    Date: class extends Date {
      constructor(...a) { if (a.length === 0) super(BASIS + S.T); else super(...a); }
      static now() { return BASIS + S.T; }
    },
    TextDecoder, TextEncoder, TextDecoderStream: web.TextDecoderStream, TextEncoderStream: web.TextEncoderStream,
    ReadableStream: web.ReadableStream, WritableStream: web.WritableStream, TransformStream: web.TransformStream,
    AbortController, queueMicrotask,
    console: { log() {}, info() {}, warn() {}, error: (...a) => S.konsole.push(a.join(" ")) },
    alert() {}, confirm: () => true, prompt: () => null,
    localStorage: { getItem: () => null, setItem() {}, removeItem() {} },
    sessionStorage: { getItem: () => null, setItem() {}, removeItem() {} },
    matchMedia: () => ({ matches: false, addEventListener() {}, removeEventListener() {} }),
    ResizeObserver: class { observe() {} unobserve() {} disconnect() {} },
    devicePixelRatio: 1, innerWidth: 1200, innerHeight: 800,
    addEventListener: (art, f) => { (S.fensterHandler[art] = S.fensterHandler[art] || []).push(f); },
    removeEventListener() {},
  };
  kontext.window = kontext;
  kontext.self = kontext;
  vm.createContext(kontext);

  S.bild = function () {
    if (S.raf) {
      const f = S.raf;
      S.raf = null;
      sicher(() => f(S.T), "Bild zeichnen");
    }
  };

  // Zeit vorrücken (in 20-ms-Schritten), dabei Zeitgeber auslösen und alle 100 ms ein Bild zeichnen
  S.vor = function (ms) {
    const ziel = S.T + ms;
    while (S.T + 20 <= ziel) {
      S.T += 20;
      S.schritte++;
      for (const t of S.timeouts.slice()) {
        if (!t.fertig && t.bei <= S.T) { t.fertig = true; sicher(t.f, "setTimeout"); }
      }
      for (const iv of S.intervalle.slice()) {
        if (iv.aus) continue;
        const dauer = Math.max(iv.ms || 0, 10);
        while (iv.letzte + dauer <= S.T) { iv.letzte += dauer; sicher(iv.f, "setInterval"); }
      }
      if (S.schritte % 5 === 0) S.bild();
    }
    S.T = Math.max(S.T, ziel);
  };

  S.laden = function () {
    sicher(() => vm.runInContext(skript, kontext, { filename: "gui_flex.html", timeout: 5000 }), "Seite laden");
    for (const art of ["DOMContentLoaded", "load"]) {
      for (const f of (S.fensterHandler[art] || [])) sicher(() => f({}), art);
    }
  };

  S.klickListe = () => Object.keys(S.handler).filter((k) => k.endsWith(":click"));
  S.klick = function (schluessel) {
    const f = S.handler[schluessel];
    const name = schluessel.substring(0, schluessel.lastIndexOf(":"));
    if (f) sicher(() => f({ target: S.elemente[name], currentTarget: S.elemente[name], preventDefault() {}, stopPropagation() {} }), "Klick " + schluessel);
  };

  // Klickt der Reihe nach auf die Knöpfe, bis das GUI den Port geöffnet hat
  S.verbinden = async function () {
    for (const k of S.klickListe()) {
      S.klick(k);
      await warte(6);
      if (S.info.offen) { S.verbindenKnopf = k; S.verbunden = true; return true; }
    }
    return false;
  };

  S.sendeRoh = async function (text) {
    if (!S.regler) return;
    S.regler.enqueue(new TextEncoder().encode(text));
    await warte(4);
  };
  S.sendeBytes = async function (bytes) {
    if (!S.regler) return;
    S.regler.enqueue(new Uint8Array(bytes));
    await warte(4);
  };
  // Zeile senden, dann "ms" Zeit vergehen lassen (Standard 300 ms: Bild und Zeitgeber kommen dran)
  S.zeile = async function (text, ms) {
    await S.sendeRoh(text + "\n");
    S.vor(ms === undefined ? 300 : ms);
    await warte(2);
  };

  S.schnappschuss = () => Object.assign({}, S.anzeige);
  S.geaendert = (vorher) => Object.keys(S.anzeige).filter((k) => vorher[k] !== S.anzeige[k]).map((k) => [k, S.anzeige[k]]);
  S.zustandText = () => JSON.stringify(Object.keys(S.anzeige).sort().map((k) => [k, S.anzeige[k]]));
  S.opsText = () => S.ops.join("\n");
  S.alleWerte = () => Object.keys(S.anzeige).map((k) => S.anzeige[k]).join(" | ");

  return S;
}

async function neueVerbundene() {
  const S = sitzungBauen();
  S.laden();
  await warte(4);
  S.vor(100);
  await S.verbinden();
  return S;
}

// Zeilen wie in AK-14
function dz(zeit, adc, widerstand, abweichungZehntel, motor, zustand) {
  return ["D", zeit, adc, widerstand, abweichungZehntel, motor, zustand].join(",");
}

// ------------------------------------------------------------------ die Prüfungen
async function main() {
  // ---------------- Verbindung
  console.log("AK-14/AK-15: GUI verbindet sich über Web Serial mit 115200 Baud");
  let S = await neueVerbundene();
  pruefe(S.fehlerMeldungen.length === 0, "Seite lädt ohne Fehler im Skript " + (S.fehlerMeldungen.length ? JSON.stringify(S.fehlerMeldungen.slice(0, 2)) : ""));
  pruefe(S.verbunden, "ein Klick auf einen der Knöpfe öffnet den Port (requestPort + open)");
  if (!S.verbunden) {
    console.log("  Abbruch: ohne Verbindung lassen sich die weiteren Prüfungen nicht durchführen (Knöpfe: " + JSON.stringify(S.klickListe()) + ")");
    console.log("\n" + (anzahl - fehler) + " von " + anzahl + " Prüfungen bestanden");
    process.exit(1);
  }
  pruefe(S.info.baud === 115200, "Port wird mit 115200 Baud geöffnet (gefunden: " + S.info.baud + ")");
  const verbindenKnopf = S.verbindenKnopf;
  const alleKnoepfe = S.klickListe();

  // ---------------- Knopf Einmessen
  console.log("AK-15: Knopf 'Einmessen' sendet '\\n'");
  {
    const ergebnisse = [];
    for (const k of alleKnoepfe) {
      if (k === verbindenKnopf) continue;
      const T = await neueVerbundene();
      const vorher = T.geschrieben.length;
      T.klick(k);
      await warte(6);
      ergebnisse.push([k, T.geschrieben.slice(vorher)]);
    }
    const treffer = ergebnisse.filter((e) => e[1].length === 1 && e[1][0] === "\n");
    pruefe(treffer.length >= 1, "ein Knopf schickt genau ein '\\n' an den Arduino (Einmessen). Alle Knöpfe: " + JSON.stringify(ergebnisse));
    const falsch = ergebnisse.filter((e) => e[1].some((w) => w !== "\n"));
    pruefe(falsch.length === 0, "kein Knopf schickt etwas anderes als '\\n' (sonst kann ein Klick etwas Unerwartetes auslösen)");
  }

  // ---------------- Anzeige der Werte
  console.log("AK-15: ADC-Rohwert, Widerstand, Zustand, Motor, Datenrate, Verlauf werden angezeigt");
  {
    S = await neueVerbundene();
    await S.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    const vor = S.schnappschuss();
    await S.zeile(dz(5020, 3071, 7331, 53, 1, 1));
    const neu = S.geaendert(vor).map((x) => x[1]).join(" | ");
    pruefe(/(?<![\d.,])3071(?!\d)/.test(neu), "ADC-Rohwert 3071 wird angezeigt");
    pruefe(/(?<![\d.,])7\.3[\s ]*kΩ/.test(neu), "Widerstand 7331 Ohm wird in kΩ mit einer Nachkommastelle angezeigt (7.3 kΩ)");
  }
  {
    const faelle = [
      [0, /Keine Referenz/, "Keine Referenz"],
      [1, /Bereit/, "Bereit"],
      [2, /Einmessen/, "Einmessen"],
      [3, /Sensor-Ausfall/, "Sensor-Ausfall"],
      [4, /Motor-Ausfall/, "Motor-Ausfall"],
    ];
    for (const f of faelle) {
      const z = f[0];
      S = await neueVerbundene();
      const basis = z === 1 ? 3 : 1;
      await S.zeile(basis === 3 ? dz(5000, 50, 0, 0, 0, 3) : dz(5000, 2000, 22000, 0, 0, 1));
      const vor = S.schnappschuss();
      await S.zeile(z === 3 ? dz(5020, 50, 0, 0, 0, 3) : dz(5020, 2000, 22000, z === 0 ? 0 : 10, 0, z));
      const neu = S.geaendert(vor).map((x) => x[1]).join(" | ");
      pruefe(f[1].test(neu), "Zustand " + z + " erscheint als Text '" + f[2] + "' (nach der Zeile neu geschrieben: " + neu.substring(0, 80) + ")");
    }
  }
  {
    const A = await neueVerbundene();
    await A.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    await A.zeile(dz(5020, 2200, 20000, 50, 0, 1));
    const B = await neueVerbundene();
    await B.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    await B.zeile(dz(5020, 2200, 20000, 50, 1, 1));
    pruefe(A.zustandText() !== B.zustandText(), "Motor an (1) und Motor aus (0) sehen auf der Seite verschieden aus");
  }
  {
    const A = await neueVerbundene();
    const B = await neueVerbundene();
    const C = await neueVerbundene();
    const serieA = [];
    for (let i = 0; i < 150; i++) serieA.push(Math.round(60 * Math.sin(i / 10)));
    for (const s of [A, B, C]) await s.zeile("G," + (s === B ? 100 : 50) + ",10,100", 20);
    for (let i = 0; i < 150; i++) {
      const abw = serieA[i];
      await A.zeile(dz(5000 + 20 * i, 2000, 22000, abw, 0, 1), 20);
      await B.zeile(dz(5000 + 20 * i, 2000, 22000, abw, 0, 1), 20);
      await C.zeile(dz(5000 + 20 * i, 2000, 22000, 0, 0, 1), 20);
    }
    for (const s of [A, B, C]) { s.vor(300); await warte(2); }
    pruefe(A.ops.length > 0, "das GUI zeichnet etwas auf die Zeichenfläche (Verlauf) (" + A.ops.length + " Zeichenbefehle)");
    pruefe(A.opsText() !== C.opsText(), "der Verlauf der Abweichung hängt von den Messwerten ab (Sinus gegen 0 gibt ein anderes Bild)");
    pruefe(A.opsText() !== B.opsText(), "die Grenzwert-Linie hängt von der G-Zeile ab (Grenzwert 5,0 % gegen 10,0 % gibt ein anderes Bild)");
  }
  {
    // Datenrate: 50 Zeilen pro Sekunde, danach 25 Zeilen pro Sekunde
    const A = await neueVerbundene();
    for (let i = 0; i < 150; i++) await A.zeile(dz(5000 + 20 * i, 3071, 7331, 53, 0, 1), 20);
    A.vor(20);
    const werteA = A.alleWerte();
    pruefe(/(?<![\d.,])(48|49|50|51|52)(?:[.,]\d+)?(?!\d)/.test(werteA), "Datenrate: bei 50 Zeilen pro Sekunde zeigt das GUI etwa 50 an");
    const B = await neueVerbundene();
    for (let i = 0; i < 100; i++) await B.zeile(dz(5000 + 40 * i, 3071, 7331, 53, 0, 1), 40);
    B.vor(20);
    const werteB = B.alleWerte();
    pruefe(/(?<![\d.,])(23|24|25|26|27)(?:[.,]\d+)?(?!\d)/.test(werteB) && !/(?<![\d.,])(48|49|50|51|52)(?:[.,]\d+)?(?!\d)/.test(werteB),
           "Datenrate: bei 25 Zeilen pro Sekunde zeigt das GUI etwa 25 an (und nicht 50)");
  }

  // ---------------- kaputte und unbekannte Zeilen
  console.log("AK-15: kaputte oder unbekannte Zeilen werden ignoriert, das GUI hängt nicht");
  {
    S = await neueVerbundene();
    await S.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    const fehlerVorher = unbehandelt.length;
    const unsinn = ["", "hallo", "D,1,2", "D,", "E,", "G,abc", "G,50", "X,1,2,3", "D,1,abc,def,ghi,jkl,mno",
      "D,1,2,3,4,5,6,7,8,9,10", "D,5000,3071,7331,53,1,9", "D,5000,3071,7331,53,2,1", "D,-5,-6,-7,-8,1,1",
      "\u0000\u0001ÿ", ",,,,", "D,,,,,,", "D,NaN,NaN,NaN,NaN,NaN,NaN", "D,1e999,1,1,1,1,1", "\r", "   ",
      "E," + "x".repeat(500), "G,1,2,3,4,5,6,7", "d,1,2,3,4,5,1", "E,a,b,c"];
    for (const z of unsinn) await S.zeile(z, 20);
    await S.sendeRoh("D,5000," + "9".repeat(100000));   // sehr lange Zeile ohne Zeilenende
    await S.zeile("", 20);
    await S.sendeBytes([0xff, 0xfe, 0x00, 0xc3, 0x28, 0x0a]);   // kein gültiges UTF-8
    S.vor(300);
    await S.zeile(dz(6000, 3071, 7331, 53, 1, 1));
    pruefe(S.fehlerMeldungen.length === 0 && unbehandelt.length === fehlerVorher,
           "keine Fehler im GUI-Skript durch kaputte Zeilen " + JSON.stringify(S.fehlerMeldungen.slice(0, 2).concat(unbehandelt.slice(fehlerVorher, fehlerVorher + 2))));
    const vor = S.schnappschuss();
    await S.zeile(dz(6020, 2999, 4444, 12, 0, 1));
    const neu = S.geaendert(vor).map((x) => x[1]).join(" | ");
    pruefe(/(?<![\d.,])2999(?!\d)/.test(neu), "nach all dem Unsinn verarbeitet das GUI eine gute Zeile ganz normal (ADC 2999 erscheint)");
    pruefe(!S.texte.some((t) => /\bNaN\b|undefined|Infinity/.test(t)), "auf der Seite steht nie 'NaN', 'undefined' oder 'Infinity'");
  }
  {
    // eine Zeile in Stücken ankommen lassen
    const A = await neueVerbundene();
    await A.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    await A.zeile(dz(5020, 3071, 7331, 53, 1, 1));
    const B = await neueVerbundene();
    await B.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    await B.sendeRoh("D,50");
    await B.sendeRoh("20,3071,73");
    await B.sendeRoh("31,53,1,1\n");
    B.vor(300);
    await warte(2);
    pruefe(A.zustandText() === B.zustandText(), "eine Zeile, die in 3 Stücken ankommt, wirkt genauso wie eine ganze Zeile");
  }

  // ---------------- Zusatz
  console.log("Zusatz (nicht in der REQ verlangt)");
  {
    S = await neueVerbundene();
    await S.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    const vor = S.schnappschuss();
    await S.zeile("E,Einmessen fertig");
    zusatz(S.geaendert(vor).some((x) => /Einmessen fertig/.test(x[1])), "E-Zeilen (Ereignisse) erscheinen als Text auf der Seite");
  }
  {
    S = await neueVerbundene();
    await S.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    const vor = S.schnappschuss();
    await S.sendeRoh(dz(5020, 3071, 7331, 53, 1, 1) + "\r\n");
    S.vor(300);
    await warte(2);
    zusatz(/(?<![\d.,])3071(?!\d)/.test(S.geaendert(vor).map((x) => x[1]).join(" | ")), "Zeilen mit '\\r\\n' am Ende werden auch gelesen");
  }
  {
    S = await neueVerbundene();
    await S.zeile(dz(5000, 2000, 22000, 0, 0, 1));
    const vor = S.zustandText();
    await S.zeile("X,1,2,3", 0);
    zusatz(S.zustandText() === vor, "eine unbekannte Zeile ('X,1,2,3') ändert nichts auf der Seite");
  }

  console.log("\n" + (anzahl - fehler) + " von " + anzahl + " Prüfungen bestanden");
  process.exit(fehler ? 1 : 0);
}

main().catch((e) => {
  console.log("FEHLER im Prüfskript: " + ((e && e.stack) || e));
  process.exit(1);
});
