"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const M = require("../lib/migrate-classic.js");
const { readDir, stockFiles, withEdits } = require("./migrate-helpers.js");

test("the stock classic sketch has no hand edits", () => {
  assert.deepEqual(M.findHandEdits(stockFiles(), "eebel"), []);
});

test("Fernando's classic sketch (PC forwarding, ROS2D2, Joy-Con layer) has no hand edits, only known add-ons", () => {
  const files = readDir("classic-fernando");
  assert.deepEqual(M.findHandEdits(files, "eebel"), []);
  assert.deepEqual(M.knownAddOns(files), ["pc-forwarding", "ros2d2", "controller-layer"]);
  assert.deepEqual(M.knownAddOns(stockFiles()), []);
});

test("a changed line inside a function is reported with its tab and line", () => {
  const files = withEdits(stockFiles(), [[/^    if\(myPS3->getButtonPress\(PS\) && myPS3->getButtonClick\(CROSS\)\)/m, "    if(myPS3->getButtonPress(PS) && myPS3->getButtonClick(CIRCLE))"]]);
  assert.deepEqual(M.findHandEdits(files, "eebel"), [{ code: "changed-function", name: "ps3ToggleSettings", file: "d_FUNCTIONS.ino", line: 364 }]);
});

test("a function the stock sketch does not have is reported as added", () => {
  const files = stockFiles();
  const d = files.find((f) => f.name === "d_FUNCTIONS.ino");
  const line = d.text.split(/\r?\n/).length + 1;
  d.text += "\nvoid myLights() {\n  Serial1.print(\"@0T5\\r\");\n}\n";
  assert.deepEqual(M.findHandEdits(files, "eebel"), [{ code: "added-function", name: "myLights", file: "d_FUNCTIONS.ino", line }]);
});

test("Vint43's original SHADOW_MD (one file) is recognised as its own version, with no hand edits", () => {
  const files = readDir("classic-vint43");
  const r = M.readClassic(files);
  assert.equal(r.variant, "vint43");
  assert.equal(r.buttons.length, 40);
  assert.deepEqual(M.findHandEdits(files, "vint43"), []);
  assert.equal(M.readClassic(stockFiles()).variant, "eebel");
  assert.equal(M.readClassic(readDir("classic-fernando")).variant, "eebel");
});

test("a change in Vint43's sketch is reported against Vint43's reference", () => {
  const files = withEdits(readDir("classic-vint43"), [[/^    if\(myPS3->getButtonPress\(PS\) && myPS3->getButtonClick\(CROSS\)\)/m, "    if(myPS3->getButtonPress(PS) && myPS3->getButtonClick(CIRCLE))"]]);
  const e = M.findHandEdits(files, "vint43");
  assert.deepEqual(e.map((x) => [x.code, x.name]), [["changed-function", "ps3ToggleSettings"]]);
});

test("only comments or spacing changed inside a function: not an edit", () => {
  const files = withEdits(stockFiles(), [[/^void ps3ToggleSettings\(DroidPad\* myPS3 = PS3NavFoot\)\r?\n\{/m, "void ps3ToggleSettings(DroidPad* myPS3 = PS3NavFoot)\n{   // my note\n\n"]]);
  assert.deepEqual(M.findHandEdits(files, "eebel"), []);
});
