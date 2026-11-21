"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const { createWebSerialTransport } = require("../lib/webserial-transport.js");
const { flashMega } = require("../lib/stk500v2.js");
const { FakeBootloader } = require("./fake-bootloader.js");

const FAST = { resetDelayMs: 0, bootWaitMs: 0, syncTimeoutMs: 20, timeoutMs: 50 };

/**
 * A stand-in for a Web Serial SerialPort (readable/writable web streams + setSignals).
 * With a FakeBootloader behind it, the bootloader's answers arrive in uneven chunks,
 * like USB packets do.
 */
function fakeSerialPort(fake) {
  let ctrl;
  let chunk = 1;
  const port = {
    signals: [],
    written: [],
    readable: new ReadableStream({ start(c) { ctrl = c; } }),
    writable: new WritableStream({
      write(u8) {
        port.written.push(Uint8Array.from(u8));
        if (port.onWrite) port.onWrite(port);
        return fake ? fake.write(u8) : undefined;
      },
    }),
    async setSignals(s) {
      port.signals.push(s);
      if (fake && "dataTerminalReady" in s) await fake.setDTR(s.dataTerminalReady);
    },
    send(bytes) {
      ctrl.enqueue(Uint8Array.from(bytes));
    },
    unplug() {
      const e = new Error("The device has been lost.");
      e.name = "NetworkError";
      ctrl.error(e);
    },
  };
  if (fake) {
    fake._send = (bytes) => {
      const a = Array.from(bytes);
      for (let i = 0; i < a.length; ) {
        const n = Math.min(a.length - i, chunk);
        port.send(a.slice(i, i + n));
        i += n;
        chunk = (chunk % 61) + 7; // 1, 8, 15 ... 64 bytes
      }
    };
  }
  return port;
}

function image(size) {
  const img = new Uint8Array(size);
  for (let i = 0; i < size; i++) img[i] = (i * 29 + 3) & 0xff;
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

test("read returns exactly n bytes across chunks and keeps the rest; drain discards", async () => {
  const port = fakeSerialPort(null);
  const t = createWebSerialTransport(port);
  port.send([1, 2]);
  port.send([3, 4, 5]);
  assert.deepEqual(Array.from(await t.read(4, 100)), [1, 2, 3, 4]);
  assert.deepEqual(Array.from(await t.read(1, 100)), [5]);
  const pending = t.read(3, 500);
  port.send([6]);
  port.send([7, 8, 9]);
  assert.deepEqual(Array.from(await pending), [6, 7, 8]);
  await new Promise((r) => setTimeout(r, 5));
  await t.drain();
  await rejectsWith(t.read(1, 20), "timeout");
  await t.close();
});

test("read times out with code timeout; bytes that arrive later are kept", async () => {
  const port = fakeSerialPort(null);
  const t = createWebSerialTransport(port);
  const t0 = Date.now();
  await rejectsWith(t.read(2, 40), "timeout");
  assert.ok(Date.now() - t0 >= 35);
  port.send([0xaa, 0xbb]);
  assert.deepEqual(Array.from(await t.read(2, 100)), [0xaa, 0xbb]);
  await t.close();
});

test("setDTR drives DTR and RTS together through setSignals", async () => {
  const port = fakeSerialPort(null);
  const t = createWebSerialTransport(port);
  await t.setDTR(false);
  await t.setDTR(true);
  assert.deepEqual(port.signals, [
    { dataTerminalReady: false, requestToSend: false },
    { dataTerminalReady: true, requestToSend: true },
  ]);
  await t.close();
});

test("close releases the reader and the writer so the port can be closed", async () => {
  const port = fakeSerialPort(null);
  const t = createWebSerialTransport(port);
  assert.equal(port.readable.locked, true);
  assert.equal(port.writable.locked, true);
  await t.close();
  assert.equal(port.readable.locked, false);
  assert.equal(port.writable.locked, false);
});

test("flashMega over the Web Serial transport writes 20 KB and the fake flash matches", async () => {
  const fake = new FakeBootloader();
  const port = fakeSerialPort(fake);
  const t = createWebSerialTransport(port);
  const img = image(20 * 1024 + 100);
  await flashMega(t, img, null, FAST);
  await t.close();
  assert.deepEqual(fake.flash.subarray(0, img.length), img);
  assert.equal(fake.checksumErrors, 0);
  assert.equal(fake.left, true);
  assert.deepEqual(port.signals.map((s) => s.dataTerminalReady), [false, true]);
});

test("unplugging mid-write rejects at once with disconnected (not after a timeout)", async () => {
  const fake = new FakeBootloader();
  const port = fakeSerialPort(fake);
  const t = createWebSerialTransport(port);
  let pages = 0;
  port.onWrite = () => {
    // the whole flash runs on microtasks with the fake, so unplug right here (no setTimeout)
    if (fake.programWrites >= 10 && pages++ === 0) port.unplug();
  };
  const t0 = Date.now();
  // a long command timeout: the error must come from the unplug, not from waiting
  const e = await rejectsWith(flashMega(t, image(64 * 1024), null, Object.assign({}, FAST, { timeoutMs: 5000 })), "disconnected");
  assert.ok(Date.now() - t0 < 2000, "took " + (Date.now() - t0) + " ms");
  assert.match(e.message, /lost/);
  // later calls fail the same way
  await rejectsWith(t.read(1, 1000), "disconnected");
  await t.close();
});

test("abort during sign-on rejects with cancelled, not no-sync, and nothing is written", async () => {
  const port = fakeSerialPort(null); // nobody answers
  const t = createWebSerialTransport(port);
  const phases = [];
  const p = flashMega(t, image(4096), (ph, d) => {
    phases.push(ph);
    if (ph === "sync" && d === 2) t.abort();
  }, Object.assign({}, FAST, { syncTimeoutMs: 100 }));
  await rejectsWith(p, "cancelled");
  assert.ok(!phases.includes("write"));
  const frames = port.written.filter((w) => w[0] === 0x1b);
  assert.ok(frames.every((f) => f[5] === 0x01), "only SIGN_ON frames were sent");
  await t.close();
});
