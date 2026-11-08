"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const D = require("../lib/ino-detect.js");

const FX = path.join(__dirname, "fixtures");
const read = (p) => fs.readFileSync(path.join(FX, p), "utf8");
const tabs = (dir) => ["a_INIT", "b_SETUP", "c_MAIN", "d_FUNCTIONS"].map((n) => ({ name: n + ".ino", text: read(dir + "/" + n + ".ino") }));
const lite = () => [{ name: "ShadowMDLite.ino", text: read("lite.ino") }];

test("lite fixture -> variant lite, forwarding true, baud 9600", () => {
  const r = D.detectSketch(lite());
  assert.equal(r.variant, "lite");
  assert.equal(r.forwarding, true);
  assert.equal(r.marcDuinoBaud, 9600);
  assert.equal(r.ros2d2, false);
  assert.equal(r.domeTracker, true);
  const up = r.buttons.find((b) => b.button === "footBtnUp");
  assert.equal(up.mdFunc, 2);
  assert.equal(up.custPanel, null);
  assert.ok(r.buttons.length > 40);
});

test("classic master fixture -> classic, forwarding false (ROS2D2 0, no PcForwarding.h)", () => {
  const r = D.detectSketch(tabs("classic"));
  assert.equal(r.variant, "classic");
  assert.equal(r.forwarding, false);
  assert.equal(r.ros2d2, false);
  assert.equal(r.marcDuinoBaud, 9600);
});

test("classic stock fixture -> classic, ros2d2 null or false", () => {
  const r = D.detectSketch(tabs("classic-stock"));
  assert.equal(r.variant, "classic");
  assert.ok(r.ros2d2 === null || r.ros2d2 === false);
  assert.equal(r.forwarding, false);
});

test("commented btnUP_MD_func = 12 is ignored; btnUP_MD_func = 80 is read", () => {
  const r = D.detectSketch(tabs("classic-stock"));
  const ups = r.buttons.filter((b) => b.button === "btnUP");
  assert.equal(ups.length, 1);
  assert.equal(ups[0].mdFunc, 80);
  assert.equal(ups[0].custPanel, 0);
});

test("classic forwarding: define 1 + PcForwarding.h include", () => {
  const stock = tabs("classic-stock");
  const extra = "\n#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1\n#include \"PcForwarding.h\"\n";
  const patched = stock.map((f, i) => (i === 0 ? { name: f.name, text: f.text + extra } : f));
  assert.equal(D.detectSketch(patched).forwarding, true);
  const onlyDefine = stock.map((f, i) => (i === 0 ? { name: f.name, text: f.text + "\n#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1\n" } : f));
  assert.equal(D.detectSketch(onlyDefine).forwarding, false);
  const commented = stock.map((f, i) => (i === 0 ? { name: f.name, text: f.text + "\n// #define ENABLE_PC_TO_MARCDUINO_FORWARDING 1\n// #include \"PcForwarding.h\"\n" } : f));
  assert.equal(D.detectSketch(commented).forwarding, false);
});

test("classic forwarding: ROS2D2 1 and rosHandleLine calls pcFwdHandleLine", () => {
  const text = "#define ENABLE_ROS2D2 1\nint btnUP_MD_func = 1;\nvoid rosHandleLine(char* l) {\n  pcFwdHandleLine(l);\n}\n";
  const r = D.detectSketch([{ name: "x.ino", text }]);
  assert.equal(r.ros2d2, true);
  assert.equal(r.forwarding, true);
});

test("garbage text -> unknown with warning not-shadow, no throw", () => {
  const r = D.detectSketch([{ name: "x.ino", text: "void setup(){}\nvoid loop(){}" }]);
  assert.equal(r.variant, "unknown");
  assert.ok(r.warnings.includes("not-shadow"));
});

test("empty and malformed input -> unknown, no throw", () => {
  for (const input of [[], undefined, null, "str", [{}], [{ name: "a", text: 5 }], [null], 42]) {
    const r = D.detectSketch(input);
    assert.equal(r.variant, "unknown");
    assert.deepEqual(r.buttons, []);
  }
});

test("huge input does not throw", () => {
  const r = D.detectSketch([{ name: "x", text: "/*".repeat(200000) + "int a_MD_func = ".repeat(50000) }]);
  assert.equal(r.variant, "unknown");
});

test("no baud -> warning no-baud", () => {
  const r = D.detectSketch([{ name: "x", text: "int btnUP_MD_func = 3;" }]);
  assert.equal(r.variant, "classic");
  assert.equal(r.marcDuinoBaud, null);
  assert.ok(r.warnings.includes("no-baud"));
});

test("commented-defines warning when forwarding define is only commented", () => {
  const r = D.detectSketch([{ name: "x", text: "int btnUP_MD_func = 3;\n// #define ENABLE_PC_TO_MARCDUINO_FORWARDING 1\n" }]);
  assert.ok(r.warnings.includes("commented-defines"));
});
