"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const P = require("../lib/ino-patch.js");
const D = require("../lib/ino-detect.js");

const FX = path.join(__dirname, "fixtures");
const read = (p) => fs.readFileSync(path.join(FX, p), "utf8");
const tabs = (dir) => ["a_INIT", "b_SETUP", "c_MAIN", "d_FUNCTIONS"].map((n) => ({ name: n + ".ino", text: read(dir + "/" + n + ".ino") }));
const lite = () => [{ name: "ShadowMDLite.ino", text: read("lite.ino") }];
const normEol = (s) => s.replace(/\r\n/g, "\n");
const lines = (s) => normEol(s).split("\n");
const file = (r, name) => r.files.find((f) => f.name === name);

test("patchClassic on classic-stock fixture inserts define, include and call, and adds PcForwarding.h", () => {
  const input = tabs("classic-stock");
  const r = P.patchClassic(input);
  assert.equal(r.ok, true);
  assert.deepEqual([...r.changed].sort(), ["PcForwarding.h", "a_INIT.ino", "c_MAIN.ino"]);
  assert.equal(file(r, "PcForwarding.h").text, P.PC_FORWARDING_H);

  const a = lines(file(r, "a_INIT.ino").text);
  const v = a.findIndex((l) => /^#define SHADOW_VERBOSE\b/.test(l));
  const d = a.indexOf("#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1");
  const inc = a.indexOf('#include "PcForwarding.h"');
  assert.ok(v >= 0 && d > v && d <= v + 3, "define right after SHADOW_VERBOSE");
  assert.equal(inc, d + 1);

  const c = lines(file(r, "c_MAIN.ino").text);
  const fm = c.indexOf("    footMotorDrive();");
  assert.deepEqual(c.slice(fm - 3, fm), [
    "#if ENABLE_PC_TO_MARCDUINO_FORWARDING && !ENABLE_ROS2D2",
    "    pcFwdReadUsb();",
    "#endif",
  ]);
  // only additions: removing the inserted lines gives the original back
  assert.equal(c.filter((l, i) => i < fm - 3 || i >= fm).join("\n"), normEol(input[2].text));
  assert.equal(file(r, "b_SETUP.ino").text, input[1].text);
  assert.equal(file(r, "d_FUNCTIONS.ino").text, input[3].text);
  // the input is not modified
  assert.equal(input[0].text, read("classic-stock/a_INIT.ino"));
});

test("patched output is detected as forwarding true by detectSketch", () => {
  for (const dir of ["classic-stock", "classic"]) {
    const before = D.detectSketch(tabs(dir));
    assert.equal(before.forwarding, false, dir);
    const r = P.patchClassic(tabs(dir));
    assert.equal(r.ok, true, dir);
    const after = D.detectSketch(r.files);
    assert.equal(after.variant, "classic", dir);
    assert.equal(after.forwarding, true, dir);
    assert.deepEqual(after.buttons, before.buttons, dir);
  }
});

test("classic master fixture: define after ENABLE_ROS2D2 and the hook at the top of rosHandleLine", () => {
  const r = P.patchClassic(tabs("classic"));
  assert.equal(r.ok, true);
  assert.deepEqual([...r.changed].sort(), ["PcForwarding.h", "a_INIT.ino", "c_MAIN.ino", "d_FUNCTIONS.ino"]);
  const a = lines(file(r, "a_INIT.ino").text);
  const ros = a.indexOf("#define ENABLE_ROS2D2 0");
  assert.equal(a.indexOf("#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1"), ros + 2);
  const f = lines(file(r, "d_FUNCTIONS.ino").text);
  const h = f.indexOf("void rosHandleLine(const char* line)");
  assert.deepEqual(f.slice(h + 1, h + 5), [
    "{",
    "#if ENABLE_PC_TO_MARCDUINO_FORWARDING",
    "  if (pcFwdHandleLine(line)) return;   // S1:, S3:, PING, HELP (PcForwarding.h)",
    "#endif",
  ]);
  // also detected as forwarding with ROS2D2 on
  const on = r.files.map((x) => ({ name: x.name, text: x.text.replace("#define ENABLE_ROS2D2 0", "#define ENABLE_ROS2D2 1") }));
  assert.equal(D.detectSketch(on).forwarding, true);
});

test("CRLF tabs stay CRLF", () => {
  const input = tabs("classic-stock").map((f) => ({ name: f.name, text: normEol(f.text).replace(/\n/g, "\r\n") }));
  const r = P.patchClassic(input);
  assert.equal(r.ok, true);
  for (const name of ["a_INIT.ino", "c_MAIN.ino"]) {
    assert.equal(/(^|[^\r])\n/.test(file(r, name).text), false, name);
  }
});

test("patching twice → already-patched", () => {
  const r = P.patchClassic(tabs("classic-stock"));
  assert.deepEqual(P.patchClassic(r.files), { ok: false, reason: "already-patched" });
});

test("define present but 0 → set to 1, nothing duplicated", () => {
  const r1 = P.patchClassic(tabs("classic-stock"));
  const off = r1.files
    .filter((f) => f.name !== "PcForwarding.h")
    .map((f) => ({ name: f.name, text: f.text.replace("#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1", "#define ENABLE_PC_TO_MARCDUINO_FORWARDING 0") }));
  const r2 = P.patchClassic(off);
  assert.equal(r2.ok, true);
  assert.deepEqual([...r2.changed].sort(), ["PcForwarding.h", "a_INIT.ino"]);
  for (const f of r1.files) assert.equal(file(r2, f.name).text, f.text, f.name);
});

test("lite fixture → not-classic", () => {
  assert.deepEqual(P.patchClassic(lite()), { ok: false, reason: "not-classic" });
  assert.deepEqual(P.patchClassic([{ name: "x.ino", text: "void setup(){}" }]), { ok: false, reason: "not-classic" });
});

test("loop without footMotorDrive(); → anchor-not-found with where 'c_MAIN.ino'", () => {
  const input = tabs("classic-stock").map((f) =>
    f.name === "c_MAIN.ino" ? { name: f.name, text: f.text.replace("    footMotorDrive();", "    // footMotorDrive();") } : f,
  );
  assert.deepEqual(P.patchClassic(input), { ok: false, reason: "anchor-not-found", where: "c_MAIN.ino" });
});

test("footMotorDrive(); twice in loop → anchor-not-found, no guessing", () => {
  const input = tabs("classic-stock").map((f) =>
    f.name === "c_MAIN.ino" ? { name: f.name, text: f.text.replace("    domeDrive();", "    footMotorDrive();\n    domeDrive();") } : f,
  );
  assert.deepEqual(P.patchClassic(input), { ok: false, reason: "anchor-not-found", where: "c_MAIN.ino" });
});

test("no ENABLE_ROS2D2 nor SHADOW_DEBUG/VERBOSE line → anchor-not-found with where 'a_INIT.ino'", () => {
  const input = tabs("classic-stock").map((f) =>
    f.name === "a_INIT.ino" ? { name: f.name, text: f.text.replace(/^#define SHADOW_(DEBUG|VERBOSE).*$/gm, "") } : f,
  );
  assert.deepEqual(P.patchClassic(input), { ok: false, reason: "anchor-not-found", where: "a_INIT.ino" });
});

test("commented-out SHADOW_DEBUG/VERBOSE lines still work as the anchor", () => {
  const input = tabs("classic-stock").map((f) =>
    f.name === "a_INIT.ino" ? { name: f.name, text: f.text.replace(/^#define SHADOW_/gm, "//#define SHADOW_") } : f,
  );
  const r = P.patchClassic(input);
  assert.equal(r.ok, true);
  assert.equal(D.detectSketch(r.files).forwarding, true);
});

test("one pasted block works like the tabs", () => {
  const one = [{ name: "Shadow.ino", text: tabs("classic-stock").map((f) => f.text).join("\n") }];
  const r = P.patchClassic(one);
  assert.equal(r.ok, true);
  assert.deepEqual([...r.changed].sort(), ["PcForwarding.h", "Shadow.ino"]);
  assert.equal(D.detectSketch(r.files).forwarding, true);
});

test("PC_FORWARDING_H equals the classic repo file", (t) => {
  const p = "C:/Claude/R2D2/SHADOW_MD_EEBEL/Shadow_MD_Eebel_Tabbed/PcForwarding.h";
  if (!fs.existsSync(p)) return t.skip("classic repo not here");
  assert.equal(normEol(P.PC_FORWARDING_H), normEol(fs.readFileSync(p, "utf8")));
});
