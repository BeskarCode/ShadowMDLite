"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const { Stk500v2, flashMega } = require("../lib/stk500v2.js");
const { FakeBootloader } = require("./fake-bootloader.js");

const FAST = { resetDelayMs: 0, syncTimeoutMs: 5, timeoutMs: 20 };

function image(size) {
  const img = new Uint8Array(size);
  for (let i = 0; i < size; i++) img[i] = (i * 13 + 1) & 0xff;
  return img;
}

async function rejectsWith(promise, code) {
  try {
    await promise;
  } catch (e) {
    assert.equal(e.code, code, "error code (message: " + e.message + ")");
    return e;
  }
  assert.fail("expected rejection with " + code);
}

test("flashMega writes 140 KB and fake flash matches; every LOAD_ADDRESS has bit 31 set", async () => {
  const fake = new FakeBootloader();
  const img = image(140 * 1024 + 17); // last page is partial
  img.fill(0xff, 0x8000, 0x8000 + 512); // two all-0xFF pages in the middle must still be written
  const phases = [];
  await flashMega(fake, img, (phase, done, total) => phases.push([phase, done, total]), FAST);

  assert.deepEqual(fake.flash.subarray(0, img.length), img);
  // rest of the last (partial) page is padded with 0xFF
  const end = Math.ceil(img.length / 256) * 256;
  assert.ok(fake.flash.subarray(img.length, end).every((b) => b === 0xff));
  assert.equal(fake.programWrites, end / 256);

  const loads = fake.commands().filter((c) => c.cmd === 0x06);
  assert.ok(loads.length > 0);
  for (const l of loads) {
    assert.equal(l.body.length, 5);
    assert.equal(l.body[1] & 0x80, 0x80, "bit 31 set");
  }
  // word addresses: the page at byte 0x20000 is loaded as word 0x10000
  assert.ok(loads.some((l) => l.body[2] === 0x01 && l.body[3] === 0x00 && l.body[4] === 0x00));

  const progs = fake.commands().filter((c) => c.cmd === 0x13);
  for (const p of progs) {
    assert.equal((p.body[1] << 8) | p.body[2], 256);
    assert.equal(p.body.length, 10 + 256);
  }
  assert.equal(fake.checksumErrors, 0);
  assert.equal(fake.left, true, "LEAVE_PROGMODE sent");
  assert.deepEqual(fake.dtr, [false, true]);

  const names = [...new Set(phases.map((p) => p[0]))];
  assert.deepEqual(names, ["reset", "sync", "write", "verify", "done"]);
  const lastWrite = phases.filter((p) => p[0] === "write").pop();
  assert.equal(lastWrite[1], lastWrite[2]);
});

test("trailing all-0xFF pages are not written", async () => {
  const fake = new FakeBootloader();
  const img = image(4096);
  img.fill(0xff, 1024); // pages 4..15 empty
  await flashMega(fake, img, () => {}, FAST);
  assert.equal(fake.programWrites, 4);
  assert.deepEqual(fake.flash.subarray(0, 1024), img.subarray(0, 1024));
});

test("sign-on skips junk the board sends right after reset", async () => {
  // the board sends junk right after reset; the reader has to skip it
  const fake = new FakeBootloader({ junkAfterReset: 40 });
  const stk = new Stk500v2(fake, FAST);
  await fake.setDTR(false);
  await fake.setDTR(true);
  assert.equal(await stk.signOn(), "AVRISP_2");
  assert.deepEqual(Array.from(await stk.readSignature()), [0x1e, 0x98, 0x01]);
});

test("flashMega with signature 1E 95 0F → wrong-chip and nothing written", async () => {
  const fake = new FakeBootloader({ signature: [0x1e, 0x95, 0x0f] });
  const before = fake.flash.slice();
  await rejectsWith(flashMega(fake, image(2048), () => {}, FAST), "wrong-chip");
  assert.deepEqual(fake.flash, before);
  assert.equal(fake.commands().filter((c) => c.cmd === 0x13 || c.cmd === 0x10).length, 0);
});

test("no bootloader at all → no-sync after 10 sign-on attempts", async () => {
  const fake = new FakeBootloader({ stopAfterFrames: 0 });
  await rejectsWith(flashMega(fake, image(256), () => {}, FAST), "no-sync");
  assert.equal(fake.commands().filter((c) => c.cmd === 0x01).length, 10);
});

test("bootloader stops answering mid-write → rejects with timeout", async () => {
  const fake = new FakeBootloader({ stopAfterFrames: 30 });
  const t0 = Date.now();
  await rejectsWith(flashMega(fake, image(140 * 1024), () => {}, FAST), "timeout");
  assert.ok(Date.now() - t0 < 2000);
});

test("verify mismatch (fake corrupts one byte) → verify-failed", async () => {
  const fake = new FakeBootloader({ corruptAt: 0x12345 });
  const e = await rejectsWith(flashMega(fake, image(100 * 1024), () => {}, FAST), "verify-failed");
  assert.equal(e.address, 0x12345);
});

test("a bad answer checksum is reported, not trusted", async () => {
  const fake = new FakeBootloader();
  const orig = fake._answer.bind(fake);
  fake._answer = (seq, body) => {
    orig(seq, body);
    fake.rx[fake.rx.length - 1] ^= 0xff;
  };
  const stk = new Stk500v2(fake, FAST);
  await rejectsWith(stk.readSignature(), "bad-checksum");
});
