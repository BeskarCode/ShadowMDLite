"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const O = require("../lib/onboarding.js");

function memStorage(init) {
  const data = Object.assign({}, init);
  return {
    getItem: (k) => (Object.prototype.hasOwnProperty.call(data, k) ? data[k] : null),
    setItem: (k, v) => {
      data[k] = String(v);
    },
    data,
  };
}

test("steps in order", () => {
  assert.deepEqual(O.STEPS, ["welcome", "sketch", "patch", "flash", "connect", "walk", "done"]);
});

test("initial state", () => {
  assert.deepEqual(O.initialState(), { step: "welcome", skipped: false, detect: null });
});

test("welcome → sketch with any start choice", () => {
  for (const start of ["example", "empty", "import"]) {
    assert.equal(O.next(O.initialState(), { start }).step, "sketch");
  }
});

test("sketch: classic without forwarding → patch", () => {
  const s = O.next({ step: "sketch", skipped: false, detect: null }, { detect: { variant: "classic", forwarding: false } });
  assert.equal(s.step, "patch");
  assert.equal(s.detect.variant, "classic");
});

test("sketch: classic with forwarding → connect; lite → connect; unknown → connect keeping detect", () => {
  const base = { step: "sketch", skipped: false, detect: null };
  assert.equal(O.next(base, { detect: { variant: "classic", forwarding: true } }).step, "connect");
  assert.equal(O.next(base, { detect: { variant: "lite", forwarding: true } }).step, "connect");
  const u = O.next(base, { detect: { variant: "unknown", warnings: ["not-shadow"] } });
  assert.equal(u.step, "connect");
  assert.deepEqual(u.detect.warnings, ["not-shadow"]);
});

test("sketch: skip → connect", () => {
  assert.equal(O.next({ step: "sketch", skipped: false, detect: null }, { skip: true }).step, "connect");
});

test("patch → flash; flash → connect when flashed or skipped", () => {
  assert.equal(O.next({ step: "patch", skipped: false, detect: null }, {}).step, "flash");
  assert.equal(O.next({ step: "flash", skipped: false, detect: null }, { flashed: true }).step, "connect");
  assert.equal(O.next({ step: "flash", skipped: false, detect: null }, { skip: true }).step, "connect");
  assert.equal(O.next({ step: "flash", skipped: false, detect: null }, {}).step, "flash");
});

test("connect → walk only with pong; walk → done", () => {
  assert.equal(O.next({ step: "connect", skipped: false, detect: null }, { pong: true }).step, "walk");
  assert.equal(O.next({ step: "connect", skipped: false, detect: null }, {}).step, "connect");
  assert.equal(O.next({ step: "walk", skipped: false, detect: null }, {}).step, "done");
});

test("next never mutates the state it gets", () => {
  const s = { step: "welcome", skipped: false, detect: null };
  O.next(s, { start: "example" });
  assert.equal(s.step, "welcome");
});

test("shouldAutoOpen: first time yes; after done, after skip or with a saved configuration no", () => {
  assert.equal(O.shouldAutoOpen(O.initialState(), false), true);
  assert.equal(O.shouldAutoOpen({ step: "done", skipped: false, detect: null }, false), false);
  assert.equal(O.shouldAutoOpen({ step: "sketch", skipped: true, detect: null }, false), false);
  assert.equal(O.shouldAutoOpen(O.initialState(), true), false);
});

test("shouldAutoOpen: a guide left half-way opens again after a reload, even with parts saved by it", () => {
  assert.equal(O.shouldAutoOpen({ step: "patch", skipped: false, detect: null }, true), true);
  assert.equal(O.shouldAutoOpen({ step: "welcome", skipped: false, detect: null }, true), false);
});

test("save and load round-trip under shadowmd_onboarding_v1", () => {
  const st = memStorage();
  const s = { step: "flash", skipped: false, detect: { variant: "classic", forwarding: false } };
  O.saveState(st, s);
  assert.ok(st.data.shadowmd_onboarding_v1);
  assert.deepEqual(O.loadState(st), s);
});

test("unreadable or missing saved state → initialState", () => {
  assert.deepEqual(O.loadState(memStorage()), O.initialState());
  assert.deepEqual(O.loadState(memStorage({ shadowmd_onboarding_v1: "{not json" })), O.initialState());
  assert.deepEqual(O.loadState(memStorage({ shadowmd_onboarding_v1: '{"step":"nowhere"}' })), O.initialState());
  const throwing = { getItem() { throw new Error("blocked"); }, setItem() { throw new Error("blocked"); } };
  assert.deepEqual(O.loadState(throwing), O.initialState());
  assert.doesNotThrow(() => O.saveState(throwing, O.initialState()));
});
