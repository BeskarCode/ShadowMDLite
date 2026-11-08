"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const LiteBase = require("../lib/lite-base.js");
const ClassicReference = require("../lib/classic-reference.js");
const T = require("../lib/sketch-text.js");

const STOCK = path.join(__dirname, "fixtures", "classic-stock");
const stockFiles = () =>
  fs.readdirSync(STOCK).filter((f) => f.endsWith(".ino")).sort().map((name) => ({ name, text: fs.readFileSync(path.join(STOCK, name), "utf8") }));

test("LiteBase has the marcDuinoButtonPush anchor once and the 60 button variables", () => {
  const n = LiteBase.text.split("void marcDuinoButtonPush(int type, int MD_func)\n{").length - 1;
  assert.equal(n, 1);
  const vars = new Set(LiteBase.text.match(/^\s*int\s+\w+_MD_func\b/gm).map((s) => s.trim().split(/\s+/)[1]));
  assert.equal(vars.size, 60);
  assert.ok(!LiteBase.text.includes("\r"), "stored with LF line endings");
});

test("LiteBase is up to date with ShadowMDLite.ino (run node scripts/build-lite-base.js after changing the sketch)", () => {
  const ino = fs.readFileSync(path.join(__dirname, "..", "..", "ShadowMDLite.ino"), "utf8").replace(/\r\n?/g, "\n");
  assert.ok(LiteBase.text === ino, "lib/lite-base.js is out of date: run node scripts/build-lite-base.js");
});

test("LiteBase has a version", () => {
  assert.equal(typeof LiteBase.version, "string");
  assert.ok(LiteBase.version.length > 0);
});

test("stripComments keeps the line count and drops comment text", () => {
  const out = T.stripComments("a // x\nb /* y\nz */ c");
  assert.equal(out.split("\n").length, 3);
  assert.ok(!/[xyz]/.test(out));
  assert.ok(out.includes("a") && out.includes("b") && out.includes("c"));
});

test("stripComments leaves comment markers inside strings alone", () => {
  assert.equal(T.stripComments('s = "http://x"; // c').trim(), 's = "http://x";');
});

test("splitFunctions finds marcDuinoButtonPush in d_FUNCTIONS.ino at line 463", () => {
  const fns = T.splitFunctions(stockFiles());
  const f = fns.find((x) => x.name === "marcDuinoButtonPush");
  assert.ok(f);
  assert.equal(f.file, "d_FUNCTIONS.ino");
  assert.equal(f.line, 463);
});

test("normalizeBody ignores comments, spacing, preprocessor lines, pcFwd and ROS2D2 additions", () => {
  const a = T.normalizeBody("{ a();\n pcFwdReadUsb();\n#if ENABLE_ROS2D2\n rosX();\n#endif\n b(); }");
  assert.equal(a, T.normalizeBody("{a();b();}"));
  assert.equal(T.normalizeBody("{ a(); // note\n  b(); }"), T.normalizeBody("{a();b();}"));
});

test("normalizeBody keeps the #else branch of an add-on block (the add-on switched off is the stock code)", () => {
  const withAddOn = "{\n#if ENABLE_ROS2D2\n x = track(s);\n#else\n x = s;\n#endif\n}";
  assert.equal(T.normalizeBody(withAddOn), T.normalizeBody("{ x = s; }"));
  const nested = "{\n#if ENABLE_ROS2D2\n#if A\n y();\n#endif\n#else\n z();\n#endif\n}";
  assert.equal(T.normalizeBody(nested), T.normalizeBody("{ z(); }"));
  assert.equal(T.normalizeBody("{\n#if ENABLE_PC_TO_MARCDUINO_FORWARDING && !ENABLE_ROS2D2\n pcFwdReadUsb();\n#endif\n a(); }"), T.normalizeBody("{a();}"));
});

test("fnv1a gives 8 hex characters and differs for different text", () => {
  assert.match(T.fnv1a("abc"), /^[0-9a-f]{8}$/);
  assert.notEqual(T.fnv1a("abc"), T.fnv1a("abd"));
});

test("ClassicReference has every function of the stock classic sketch", () => {
  const ref = ClassicReference.eebel.functions;
  const fns = T.splitFunctions(stockFiles());
  assert.ok(fns.length > 20);
  for (const f of fns) assert.equal(ref[f.key], T.fnv1a(T.normalizeBody(f.body)), f.key);
});
