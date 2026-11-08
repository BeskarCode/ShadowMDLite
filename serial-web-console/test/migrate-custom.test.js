"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const M = require("../lib/migrate-classic.js");

const dp = (list) => Array.from({ length: 10 }, (_, i) => Object.assign({ n: i + 1, use: false, start: 0, open: 0 }, list.find((d) => d.n === i + 1) || {}));
const btn = (o) => Object.assign({ type: 2, mp3: 0, ldType: 0, ldText: "", panel: 0, dp: dp([]) }, o);

test("sound 202, panel sequence 3: sound, close all, :SE52", () => {
  assert.deepEqual(M.translateCustom(btn({ mp3: 202, panel: 3 })), [
    { cmd: "$803\r", delayAfterMs: 100 },
    { cmd: ":CL00\r", delayAfterMs: 100 },
    { cmd: ":SE52\r", delayAfterMs: 100 },
  ]);
});

test("custom panel sequence: each panel opens at its start and closes after its open time", () => {
  const b = btn({ panel: 9, dp: dp([{ n: 1, use: true, start: 1, open: 5 }, { n: 3, use: true, start: 2, open: 3 }]) });
  assert.deepEqual(M.translateCustom(b), [
    { cmd: ":CL00\r", delayAfterMs: 1000 },
    { cmd: ":OP01\r", delayAfterMs: 1000 },
    { cmd: ":OP03\r", delayAfterMs: 3000 },
    { cmd: ":CL03\r", delayAfterMs: 1000 },
    { cmd: ":CL01\r", delayAfterMs: 100 },
  ]);
});

test("sound table edges: 182 $87, 183 $88, 184 $809, 200 $825, 201 $82; 181 and 203 play nothing", () => {
  const first = (mp3) => (M.translateCustom(btn({ mp3 }))[0] || {}).cmd;
  assert.equal(first(182), "$87\r");
  assert.equal(first(183), "$88\r");
  assert.equal(first(184), "$809\r");
  assert.equal(first(200), "$825\r");
  assert.equal(first(201), "$82\r");
  assert.deepEqual(M.translateCustom(btn({ mp3: 181 })), []);
  assert.deepEqual(M.translateCustom(btn({ mp3: 203 })), []);
});

test("panel 1 only closes all; logic displays 1-7 send one command each, after the panels", () => {
  assert.deepEqual(M.translateCustom(btn({ panel: 1 })).map((c) => c.cmd), [":CL00\r"]);
  const ld = { 1: "@0T1", 2: "@0T4", 3: "@0T5", 4: "@0T6", 5: "@0T10", 6: "@0T11", 7: "@0T92" };
  for (const [t, cmd] of Object.entries(ld)) assert.deepEqual(M.translateCustom(btn({ ldType: +t })).map((c) => c.cmd), [cmd + "\r"]);
  assert.deepEqual(M.translateCustom(btn({ panel: 2, ldType: 1 })).map((c) => c.cmd), [":CL00\r", ":SE51\r", "@0T1\r"]);
});

test("custom logic text is sent as written and becomes a valid C literal", () => {
  const cmds = M.translateCustom(btn({ ldType: 8, ldText: 'Say "hi"\\' })).map((c) => c.cmd);
  assert.deepEqual(cmds, ["@0T100\r", '@0MSay "hi"\\\r']);
  assert.equal(M.cLiteral(cmds[1]), '"@0MSay \\"hi\\"\\\\\\r"');
  assert.equal(M.cLiteral("ñ\r"), '"\\303\\261\\r"');
});

test("a panel starting after 30 s is skipped; an open time of 0 opens and never closes", () => {
  const b = btn({ panel: 9, dp: dp([{ n: 2, use: true, start: 31, open: 3 }, { n: 4, use: true, start: 1, open: 0 }]) });
  assert.deepEqual(M.translateCustom(b).map((c) => c.cmd), [":CL00\r", ":OP04\r"]);
});

test("panels not marked as used are ignored", () => {
  const b = btn({ panel: 9, dp: dp([{ n: 5, use: false, start: 1, open: 2 }]) });
  assert.deepEqual(M.translateCustom(b).map((c) => c.cmd), [":CL00\r"]);
});
