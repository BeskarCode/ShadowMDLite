"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

global.window = global.window || {};
require("../i18n.js");
const I = global.window.SHADOWMD_LITE_I18N;
const html = fs.readFileSync(path.join(__dirname, "..", "index.html"), "utf8");

/** Keys index.html uses: data-i18n* attributes, L("key"), [["key", …]] message lines, and the dynamic families. */
function usedKeys() {
  const keys = new Set();
  for (const m of html.matchAll(/data-i18n(?:-html|-placeholder|-title|-alt)?="([^"]+)"/g)) keys.add(m[1]);
  for (const m of html.matchAll(/\bL\("([A-Za-z0-9_\-]+)"\s*[,)]/g)) keys.add(m[1]);
  for (const m of html.matchAll(/\[\["([A-Za-z0-9_\-]+)"/g)) keys.add(m[1]);
  ["welcome", "sketch", "patch", "flash", "connect", "walk", "done"].forEach((s) => ["onbStep_", "onbT_", "onbX_"].forEach((p) => keys.add(p + s)));
  ["pc-forwarding", "ros2d2", "controller-layer"].forEach((a) => keys.add("migAddOn_" + a));
  return keys;
}

test("every UI text the page uses exists in English and Spanish", () => {
  const missing = [];
  for (const k of usedKeys()) for (const lang of ["en", "es"]) if (!(k in I[lang])) missing.push(lang + ":" + k);
  assert.deepEqual(missing, []);
});

test("English and Spanish have the same keys", () => {
  assert.deepEqual(Object.keys(I.en).filter((k) => !(k in I.es)), []);
  assert.deepEqual(Object.keys(I.es).filter((k) => !(k in I.en)), []);
});
