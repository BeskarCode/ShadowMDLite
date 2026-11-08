"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const M = require("../lib/migrate-classic.js");
const { stockFiles, liteFiles, withEdits, CUSTOM_UP } = require("./migrate-helpers.js");

const SETTINGS = [
  "PS3ControllerFootMac", "PS3ControllerDomeMAC", "PS3ControllerBackupFootMac", "PS3ControllerBackupDomeMAC",
  "drivespeed1", "drivespeed2", "turnspeed", "domespeed", "ramping",
  "joystickFootDeadZoneRange", "joystickDomeDeadZoneRange", "driveDeadBandRange", "invertTurnDirection",
  "domeAutoSpeed", "time360DomeTurn", "marcDuinoBaudRate", "motorControllerBaudRate", "serialLatency", "CustomSongMax",
];

const byClassic = (r, name) => r.buttons.find((b) => b.classic === name);

test("the stock classic sketch is Eebel's, with 40 buttons mapped to Lite names", () => {
  const r = M.readClassic(stockFiles());
  assert.equal(r.variant, "eebel");
  assert.equal(r.buttons.length, 40);
  const up = byClassic(r, "btnUP");
  assert.deepEqual([up.lite, up.controller, up.type, up.mdFunc], ["footBtnUp", "foot", 1, 80]); // not the commented-out 12
  assert.equal(up.file, "a_INIT.ino");
  const ft = byClassic(r, "FTbtnUP");
  assert.deepEqual([ft.lite, ft.controller, ft.type, ft.mdFunc], ["domeBtnUp", "dome", 1, 82]);
  assert.equal(byClassic(r, "btnLeft_CROSS").lite, "footBtnLeftCross");
  assert.equal(byClassic(r, "FTbtnDown_L1").lite, "domeBtnDownL1");
  assert.equal(byClassic(r, "btnRight_PS").lite, "footBtnRightPS");
  assert.equal(byClassic(r, "FTbtnUP_CIRCLE").lite, "domeBtnUpCircle");
});

test("all 19 settings are read as written", () => {
  const r = M.readClassic(stockFiles());
  for (const s of SETTINGS) assert.ok(s in r.settings, s);
  assert.equal(r.settings.marcDuinoBaudRate, "9600");
  assert.equal(r.settings.PS3ControllerFootMac, '"00:06:F5:13:C6:D5"');
  assert.equal(r.settings.invertTurnDirection, "-1");
  assert.equal(r.defines.SABERTOOTH_ADDR, "128");
  assert.equal(r.defines.SHADOW_DEBUG, "");
  assert.ok(!("ENABLE_ROS2D2" in r.defines));
});

test("one pasted block and CRLF line endings give the same reading", () => {
  const tabs = stockFiles();
  const a = M.readClassic(tabs);
  const one = M.readClassic([{ name: "sketch.ino", text: tabs.map((f) => f.text).join("\n") }]);
  const crlf = M.readClassic(tabs.map((f) => ({ name: f.name, text: f.text.replace(/\r?\n/g, "\r\n") })));
  const strip = (r) => ({ variant: r.variant, settings: r.settings, defines: r.defines, buttons: r.buttons.map(({ file, line, ...b }) => b) });
  assert.deepEqual(strip(one), strip(a));
  assert.deepEqual(crlf, a);
});

test("ShadowMD Lite and other text are not taken for a classic sketch", () => {
  assert.deepEqual([M.readClassic(liteFiles()).variant, M.readClassic(liteFiles()).looksLike], [null, "lite"]);
  const other = M.readClassic([{ name: "x.ino", text: "void setup() {}\nvoid loop() {}\n" }]);
  assert.deepEqual([other.variant, other.looksLike], [null, "unknown"]);
});

test("a custom (type 2) button is read with its sound, panel and custom panel sequence", () => {
  const up = byClassic(M.readClassic(withEdits(stockFiles(), CUSTOM_UP)), "btnUP");
  assert.equal(up.type, 2);
  assert.equal(up.mp3, 202);
  assert.equal(up.panel, 9);
  assert.equal(up.ldType, 0);
  assert.equal(up.dp.length, 10);
  assert.deepEqual(up.dp[2], { n: 3, use: true, start: 2, open: 3 });
  assert.equal(up.dp[0].use, false);
});

test("a define written in hex is kept as written", () => {
  const r = M.readClassic(withEdits(stockFiles(), [[/^#define SABERTOOTH_ADDR\s+128/m, "#define SABERTOOTH_ADDR    0x80"]]));
  assert.equal(r.defines.SABERTOOTH_ADDR, "0x80");
});
