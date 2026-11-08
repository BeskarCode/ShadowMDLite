"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const M = require("../lib/migrate-classic.js");
const { readDir, stockFiles, liteFiles, withEdits, CUSTOM_UP } = require("./migrate-helpers.js");

const NOW = new Date("2026-11-08T12:00:00Z");
const value = (ino, name) => {
  const m = new RegExp("^\\s*[A-Za-z_][\\w ]*\\b" + name + "\\s*=\\s*([^;]*);", "m").exec(ino);
  return m && m[1].trim();
};

test("the stock classic sketch migrates cleanly", () => {
  const r = M.migrate(stockFiles(), undefined, { now: NOW });
  assert.equal(r.ok, true);
  assert.equal(value(r.ino, "footBtnUp_MD_func"), "80");
  assert.equal(value(r.ino, "domeBtnUp_MD_func"), "82");
  assert.equal(value(r.ino, "drivespeed1"), "90"); // the classic's value, not Lite's 70
  assert.equal(value(r.ino, "PS3ControllerFootMac"), '"00:06:F5:13:C6:D5"');
  assert.match(r.ino, /^#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1\b/m);
  assert.match(r.ino, /^#define ENABLE_ROS2D2 0\b/m);
  assert.deepEqual(r.report.edits, []);
  assert.equal(r.report.liteOnly.length, 20);
  assert.ok(r.report.liteOnly.includes("domeBtnUpL2Cross_MD_func"));
  assert.equal(r.report.buttons.length, 40);
  // Eebel's stock sketch already has three custom (type 2) buttons
  assert.equal(r.report.buttons.filter((b) => b.kind === "standard").length, 37);
  assert.deepEqual(r.report.custom.map((c) => [c.button, c.code, c.commands.map((k) => k.cmd)]), [
    ["btnUP_PS", 201, ["$82\r", "@0T10\r"]],
    ["btnUP_L1", 202, ["$809\r"]],
    ["btnDown_PS", 203, ["$803\r", "@0T1\r"]],
  ]);
  assert.equal(value(r.ino, "footBtnUpPS_MD_func"), "201");
  assert.deepEqual(r.report.warnings, []);
  assert.match(r.ino.split("\n")[0], /migrated from a classic Shadow MD/);
  assert.match(r.ino, /a fork of the original Shadow MD/);
});

test("a custom button becomes code 201 and a queued function", () => {
  const r = M.migrate(withEdits(stockFiles(), CUSTOM_UP), undefined, { now: NOW });
  assert.equal(value(r.ino, "footBtnUp_MD_func"), "201");
  assert.equal(r.ino.split("void runMigratedFunction(uint8_t n)").length - 1, 1);
  assert.match(r.ino, /case 1:/);
  assert.ok(r.ino.includes('queueSerial1("$803\\r", 100);'));
  assert.ok(r.ino.includes('queueSerial1(":OP03\\r", 3000);'));
  assert.equal(r.ino.split("if (MD_func >= 200) { runMigratedFunction(MD_func - 200); return; }").length - 1, 1);
  assert.ok(r.ino.indexOf("void runMigratedFunction") < r.ino.indexOf("void marcDuinoButtonPush(int type, int MD_func)\n{"));
  const b = r.report.buttons.find((x) => x.classic === "btnUP");
  assert.deepEqual([b.kind, b.code], ["custom", 201]);
  assert.equal(r.report.custom[0].commands[0].cmd, "$803\r");
});

test("a function number Lite does not have leaves the button unassigned, with a warning", () => {
  const r = M.migrate(withEdits(stockFiles(), [[/^int btnLeft_MD_func = \d+;/m, "int btnLeft_MD_func = 95;"]]), undefined, { now: NOW });
  assert.equal(value(r.ino, "footBtnLeft_MD_func"), "0");
  assert.deepEqual(r.report.warnings.filter((w) => w.code === "unknown-md-func"), [{ code: "unknown-md-func", detail: { button: "btnLeft", mdFunc: 95 } }]);
  assert.equal(r.report.buttons.find((x) => x.classic === "btnLeft").kind, "unassigned");
});

test("a setting missing in the classic sketch keeps Lite's value, with a warning", () => {
  const r = M.migrate(withEdits(stockFiles(), [[/^int serialLatency = 25;.*$/m, ""]]), undefined, { now: NOW });
  assert.equal(r.report.settings.find((s) => s.name === "serialLatency").status, "kept-lite");
  assert.ok(r.report.warnings.some((w) => w.code === "missing-setting" && w.detail.name === "serialLatency"));
});

test("ROS2D2 and the known add-ons of Fernando's sketch carry over", () => {
  const files = withEdits(readDir("classic-fernando"), [[/^#define ENABLE_ROS2D2 0/m, "#define ENABLE_ROS2D2 1"]]);
  const r = M.migrate(files, undefined, { now: NOW });
  assert.equal(r.ok, true);
  assert.match(r.ino, /^#define ENABLE_ROS2D2 1\b/m);
  assert.deepEqual(r.report.addOns, ["pc-forwarding", "ros2d2", "controller-layer"]);
  assert.deepEqual(r.report.edits, []);
});

test("debug output off in the classic sketch is off in Lite", () => {
  const files = withEdits(stockFiles(), [[/^#define SHADOW_VERBOSE\b.*$/m, "//#define SHADOW_VERBOSE"]]);
  const r = M.migrate(files, undefined, { now: NOW });
  assert.doesNotMatch(r.ino, /^#define SHADOW_VERBOSE\b/m);
  assert.match(r.ino, /^#define SHADOW_DEBUG\b/m);
});

test("ShadowMD Lite is not migrated; the same input gives the same sketch", () => {
  assert.deepEqual(M.migrate(liteFiles()).ok, false);
  assert.equal(M.migrate(liteFiles()).reason, "not-classic");
  const a = M.migrate(stockFiles(), undefined, { now: NOW }).ino;
  const b = M.migrate(stockFiles(), undefined, { now: new Date("2027-01-01T00:00:00Z") }).ino;
  assert.equal(a.split("\n").slice(3).join("\n"), b.split("\n").slice(3).join("\n"));
});
