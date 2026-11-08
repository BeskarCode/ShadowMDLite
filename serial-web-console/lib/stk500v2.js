/**
 * STK500v2 client for the Arduino Mega 2560 / Mega ADK bootloader (stk500boot.c),
 * the protocol avrdude speaks with "-c wiring" (stk500v2.c) when the Arduino IDE uploads.
 * Plain script (no modules) so index.html still opens from file://.
 *
 * Transport (Task 6 provides the Web Serial one):
 *   write(u8: Uint8Array): Promise<void>
 *   read(n, timeoutMs): Promise<Uint8Array>  exactly n bytes, or rejects with code "timeout"
 *   setDTR(on: boolean): Promise<void>
 *   drain(): Promise<void>                  throws away bytes received so far (input flush)
 *
 * Errors carry .code: "timeout" | "no-sync" | "wrong-chip" | "verify-failed" |
 * "bad-checksum" | "bad-status" | "protocol" | "too-big" | "empty".
 */
(function (root) {
  "use strict";

  const MESSAGE_START = 0x1b;
  const TOKEN = 0x0e;
  const STATUS_CMD_OK = 0x00;
  const ANSWER_CKSUM_ERROR = 0xb0;

  const CMD_SIGN_ON = 0x01;
  const CMD_LOAD_ADDRESS = 0x06;
  const CMD_ENTER_PROGMODE_ISP = 0x10;
  const CMD_LEAVE_PROGMODE_ISP = 0x11;
  const CMD_PROGRAM_FLASH_ISP = 0x13;
  const CMD_READ_FLASH_ISP = 0x14;
  const CMD_READ_SIGNATURE_ISP = 0x1b;

  const PAGE_SIZE = 256; // SPM_PAGESIZE of the ATmega2560
  const MAX_SIZE = 253952; // 256 KB minus the 8 KB bootloader section
  const MEGA2560_SIGNATURE = [0x1e, 0x98, 0x01];

  const DEFAULTS = {
    timeoutMs: 1000, // per command (a page write takes a few ms on the chip)
    syncTimeoutMs: 200, // per sign-on attempt
    syncAttempts: 10,
    resetDelayMs: 50, // DTR low time, and wait after raising it
  };

  function fail(code, msg, extra) {
    const e = new Error(msg);
    e.code = code;
    if (extra) Object.assign(e, extra);
    return e;
  }

  function sleep(ms) {
    return new Promise((r) => setTimeout(r, ms));
  }

  function hex(arr) {
    return Array.from(arr, (b) => b.toString(16).toUpperCase().padStart(2, "0")).join(" ");
  }

  /** Pages to write: from 0 up to the last page that is not all 0xFF. */
  function pageCount(bytes) {
    let last = -1;
    for (let i = bytes.length - 1; i >= 0; i--) {
      if (bytes[i] !== 0xff) {
        last = i;
        break;
      }
    }
    return last < 0 ? 0 : Math.floor(last / PAGE_SIZE) + 1;
  }

  /** One page of the image, padded with 0xFF. */
  function pageData(bytes, page) {
    const out = new Uint8Array(PAGE_SIZE).fill(0xff);
    const start = page * PAGE_SIZE;
    out.set(bytes.subarray(start, Math.min(start + PAGE_SIZE, bytes.length)));
    return out;
  }

  class Stk500v2 {
    constructor(transport, opts) {
      this.t = transport;
      this.opts = Object.assign({}, DEFAULTS, opts || {});
      this.seq = 1;
    }

    _frame(body) {
      const f = new Uint8Array(body.length + 6);
      f[0] = MESSAGE_START;
      f[1] = this.seq;
      f[2] = (body.length >> 8) & 0xff;
      f[3] = body.length & 0xff;
      f[4] = TOKEN;
      f.set(body, 5);
      let c = 0;
      for (let i = 0; i < f.length - 1; i++) c ^= f[i];
      f[f.length - 1] = c;
      return f;
    }

    /** Reads one answer frame with sequence number `seq`; skips junk and stale frames. */
    async _receive(seq, timeoutMs) {
      const deadline = Date.now() + timeoutMs;
      const left = () => Math.max(0, deadline - Date.now());
      for (;;) {
        const s = await this.t.read(1, left());
        if (s[0] !== MESSAGE_START) continue;
        const h = await this.t.read(4, left());
        if (h[3] !== TOKEN) continue;
        const size = (h[1] << 8) | h[2];
        const rest = await this.t.read(size + 1, left());
        let c = MESSAGE_START ^ h[0] ^ h[1] ^ h[2] ^ h[3];
        for (let i = 0; i < size; i++) c ^= rest[i];
        if (c !== rest[size]) throw fail("bad-checksum", "answer from the bootloader has a wrong checksum");
        if (h[0] !== seq) continue; // late answer to an earlier attempt
        return rest.subarray(0, size);
      }
    }

    /** Sends one command, returns the answer body (answer[0] = command, answer[1] = status). */
    async _command(body, timeoutMs) {
      const seq = this.seq;
      const frame = this._frame(body);
      this.seq = (this.seq + 1) & 0xff;
      await this.t.write(frame);
      const ans = await this._receive(seq, timeoutMs == null ? this.opts.timeoutMs : timeoutMs);
      if (ans[0] === ANSWER_CKSUM_ERROR) throw fail("bad-checksum", "the bootloader got a corrupted command");
      if (ans[0] !== body[0]) throw fail("protocol", "answer to command 0x" + ans[0].toString(16) + " instead of 0x" + body[0].toString(16));
      if (ans.length < 2 || ans[1] !== STATUS_CMD_OK) {
        throw fail("bad-status", "command 0x" + body[0].toString(16) + " failed (status 0x" + (ans[1] || 0).toString(16) + ")");
      }
      return ans;
    }

    /** CMD_SIGN_ON with retries; returns the programmer id ("AVRISP_2" on the Mega bootloader). */
    async signOn(onAttempt) {
      const n = this.opts.syncAttempts;
      let lastErr = null;
      for (let i = 1; i <= n; i++) {
        if (onAttempt) onAttempt(i, n);
        try {
          await this.t.drain();
          const ans = await this._command([CMD_SIGN_ON], this.opts.syncTimeoutMs);
          const len = ans[2] || 0;
          return String.fromCharCode.apply(null, Array.from(ans.subarray(3, 3 + len)));
        } catch (e) {
          lastErr = e;
        }
      }
      throw fail("no-sync", "no answer from the bootloader after " + n + " attempts" + (lastErr ? " (" + lastErr.message + ")" : ""));
    }

    /** Three CMD_READ_SIGNATURE_ISP, like avrdude: Read Signature Byte (0x30 0x00 i 0x00), poll index 4. */
    async readSignature() {
      const sig = new Uint8Array(3);
      for (let i = 0; i < 3; i++) {
        const ans = await this._command([CMD_READ_SIGNATURE_ISP, 0x04, 0x30, 0x00, i, 0x00]);
        sig[i] = ans[2];
      }
      return sig;
    }

    /** Same parameters avrdude sends for the ATmega2560 (avrdude.conf); the bootloader only answers OK. */
    async enterProg() {
      await this._command([CMD_ENTER_PROGMODE_ISP, 200, 100, 25, 32, 0, 0x53, 3, 0xac, 0x53, 0x00, 0x00]);
    }

    async leaveProg() {
      await this._command([CMD_LEAVE_PROGMODE_ISP, 1, 1]);
    }

    /** Word address (bytes / 2), big endian, bit 31 set as avrdude does for flash with a load-extended-address op. */
    async loadAddress(byteAddress) {
      const w = ((byteAddress >>> 1) | 0x80000000) >>> 0;
      await this._command([CMD_LOAD_ADDRESS, (w >>> 24) & 0xff, (w >>> 16) & 0xff, (w >>> 8) & 0xff, w & 0xff]);
    }

    /**
     * Writes every page from 0 to the last non-0xFF page, in order and without gaps:
     * the bootloader erases pages sequentially from 0 (eraseAddress), one per write.
     */
    async program(bytes, onProgress) {
      const pages = pageCount(bytes);
      const total = pages * PAGE_SIZE;
      for (let p = 0; p < pages; p++) {
        await this.loadAddress(p * PAGE_SIZE);
        const body = new Uint8Array(10 + PAGE_SIZE);
        // size, mode (0x41 | 0x80 = write page), delay 10, loadpage_lo 0x40, writepage 0x4C, read_lo 0x20, poll 00 00
        body.set([CMD_PROGRAM_FLASH_ISP, PAGE_SIZE >> 8, PAGE_SIZE & 0xff, 0xc1, 10, 0x40, 0x4c, 0x20, 0x00, 0x00]);
        body.set(pageData(bytes, p), 10);
        await this._command(body);
        if (onProgress) onProgress((p + 1) * PAGE_SIZE, total);
      }
      return total;
    }

    /** Reads back the pages program() wrote and compares them. */
    async verify(bytes, onProgress) {
      const pages = pageCount(bytes);
      const total = pages * PAGE_SIZE;
      for (let p = 0; p < pages; p++) {
        await this.loadAddress(p * PAGE_SIZE);
        const ans = await this._command([CMD_READ_FLASH_ISP, PAGE_SIZE >> 8, PAGE_SIZE & 0xff, 0x20]);
        if (ans.length !== PAGE_SIZE + 3) throw fail("protocol", "short flash read (" + ans.length + " bytes)");
        const want = pageData(bytes, p);
        for (let i = 0; i < PAGE_SIZE; i++) {
          if (ans[2 + i] !== want[i]) {
            const address = p * PAGE_SIZE + i;
            throw fail("verify-failed", "verify failed at 0x" + address.toString(16) + ": wrote 0x" + want[i].toString(16) + ", read 0x" + ans[2 + i].toString(16), {
              address, expected: want[i], actual: ans[2 + i],
            });
          }
        }
        if (onProgress) onProgress((p + 1) * PAGE_SIZE, total);
      }
      return total;
    }
  }

  /**
   * Full upload: DTR reset -> sign-on (10 x 200 ms) -> signature must be 1E 98 01 ->
   * enter prog -> write -> verify -> leave prog (the bootloader then starts the sketch).
   * onProgress(phase, done, total), phase: "reset" | "sync" | "write" | "verify" | "done".
   * opts (mainly for tests): timeoutMs, syncTimeoutMs, syncAttempts, resetDelayMs.
   */
  async function flashMega(transport, bytes, onProgress, opts) {
    const o = Object.assign({}, DEFAULTS, opts || {});
    const progress = onProgress || function () {};
    if (bytes.length > MAX_SIZE) throw fail("too-big", "image is " + bytes.length + " bytes, the Mega has " + MAX_SIZE);
    if (pageCount(bytes) === 0) throw fail("empty", "nothing to write");

    progress("reset", 0, 1);
    await transport.setDTR(false);
    await sleep(o.resetDelayMs);
    await transport.setDTR(true);
    await sleep(o.resetDelayMs);
    await transport.drain();
    progress("reset", 1, 1);

    const stk = new Stk500v2(transport, o);
    await stk.signOn((i, n) => progress("sync", i, n));
    const sig = await stk.readSignature();
    if (sig[0] !== MEGA2560_SIGNATURE[0] || sig[1] !== MEGA2560_SIGNATURE[1] || sig[2] !== MEGA2560_SIGNATURE[2]) {
      throw fail("wrong-chip", "signature " + hex(sig) + " is not an ATmega2560 (1E 98 01)", { signature: sig });
    }
    await stk.enterProg();
    await stk.program(bytes, (d, t) => progress("write", d, t));
    // On verify-failed we do not leave prog mode, so the bootloader does not jump into a bad sketch.
    await stk.verify(bytes, (d, t) => progress("verify", d, t));
    await stk.leaveProg();
    progress("done", 1, 1);
  }

  const api = { Stk500v2, flashMega, PAGE_SIZE, MAX_SIZE, MEGA2560_SIGNATURE, DEFAULTS };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.Stk500v2 = api;
})(this);
