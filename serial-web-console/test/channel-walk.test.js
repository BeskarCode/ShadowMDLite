"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const W = require("../lib/channel-walk.js");

const cfg = {
  version: 1,
  markers: [
    { id: "m1", name: "Pie panel", nameEs: "Panel pastel", kind: "servo", board: "S1:", channel: "07" },
    { id: "m2", name: "Door", kind: "servo", board: "S3:", channel: "07" },
    { id: "h1", name: "Front holo", kind: "holo", board: "S1:", holoId: "07", axes: ["h", "v"] },
  ],
};

test("channel is two digits", () => {
  assert.equal(W.channelStr(7), "07");
  assert.equal(W.channelStr(13), "13");
});

test("open then close commands carry the board prefix (double colon, as the console sends them)", () => {
  assert.deepEqual(W.walkCommands("S1:", 7), { open: "S1::OP07", close: "S1::CL07" });
  assert.deepEqual(W.walkCommands("S3:", 12), { open: "S3::OP12", close: "S3::CL12" });
});

test("finds the servo marker on that board and channel, never a holo or the other board", () => {
  assert.equal(W.findServo(cfg, "S1:", 7).id, "m1");
  assert.equal(W.findServo(cfg, "S3:", 7).id, "m2");
  assert.equal(W.findServo(cfg, "S1:", 8), null);
  assert.equal(W.findServo({ markers: [cfg.markers[2]] }, "S1:", 7), null);
});

test("default name: Dome 07 / Body 07 (same in both languages)", () => {
  assert.deepEqual(W.defaultName("S1:", 7), { name: "Dome 07", nameEs: "Dome 07" });
  assert.deepEqual(W.defaultName("S3:", 3), { name: "Body 03", nameEs: "Body 03" });
});

test("range: from 1, to 13 by default, clamped to 1..99 and from <= to", () => {
  assert.deepEqual(W.normalizeRange(), { from: 1, to: 13 });
  assert.deepEqual(W.normalizeRange(1, 200), { from: 1, to: 99 });
  assert.deepEqual(W.normalizeRange(0, 5), { from: 1, to: 5 });
  assert.deepEqual(W.normalizeRange(9, 4), { from: 9, to: 9 });
  assert.deepEqual(W.normalizeRange("3", "x"), { from: 3, to: 13 });
});
