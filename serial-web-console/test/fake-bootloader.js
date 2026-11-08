"use strict";
/**
 * In-memory fake of the Arduino Mega 2560 "stk500v2" bootloader (stk500boot.c),
 * exposed through the same Transport interface the flasher uses:
 *   write(u8), read(n, timeoutMs), setDTR(on), drain().
 *
 * Behaviour copied from the real bootloader where it matters:
 *  - frame 0x1B seq sizeHi sizeLo 0x0E body xor; answers echo seq;
 *  - bad checksum -> answer ANSWER_CKSUM_ERROR (0xB0) / STATUS_CKSUM_ERROR (0xC1);
 *  - LOAD_ADDRESS takes a WORD address, big endian, address = (word << 1) in 32 bits
 *    (so bit 31 falls off, as on the chip);
 *  - PROGRAM_FLASH erases the page at a sequential eraseAddress (0, 256, 512 ...)
 *    and then programs (bits can only go 1 -> 0), address auto-increments;
 *  - READ_FLASH answers [0x14, OK, data..., OK], address auto-increments;
 *  - READ_SIGNATURE answers byte number msgBuffer[4] of the signature.
 */

const FLASH_SIZE = 262144;
const PAGE = 256;

function xor(arr, n) {
  let c = 0;
  for (let i = 0; i < n; i++) c ^= arr[i];
  return c;
}

function timeoutError() {
  const e = new Error("timeout");
  e.code = "timeout";
  return e;
}

class FakeBootloader {
  /**
   * @param {object} [opts]
   * @param {number[]} [opts.signature]   3 bytes, default 1E 98 01
   * @param {number} [opts.stopAfterFrames]  stop answering after N frames have been answered
   * @param {number} [opts.corruptAt]     byte address that is flipped after it is programmed
   * @param {Uint8Array} [opts.oldFlash]  previous flash content (default: a non-0xFF "old sketch")
   * @param {number} [opts.junkAfterReset] bytes of junk sent right after reset
   */
  constructor(opts) {
    opts = opts || {};
    this.signature = opts.signature || [0x1e, 0x98, 0x01];
    this.stopAfterFrames = opts.stopAfterFrames == null ? Infinity : opts.stopAfterFrames;
    this.corruptAt = opts.corruptAt == null ? -1 : opts.corruptAt;
    this.flash = new Uint8Array(FLASH_SIZE);
    if (opts.oldFlash) this.flash.set(opts.oldFlash);
    else for (let i = 0; i < FLASH_SIZE; i++) this.flash[i] = (i * 7 + 3) & 0x7f; // old sketch, never 0xFF
    this.junkAfterReset = opts.junkAfterReset || 0;
    this.rx = []; // bytes from bootloader to host
    this.inBuf = []; // bytes from host to bootloader
    this.waiter = null;
    this.answered = 0;
    this.frames = []; // every valid frame body received
    this.checksumErrors = 0;
    this.programWrites = 0;
    this.dtr = [];
    this.inBootloader = true;
    this.left = false;
    this._resetState();
  }

  _resetState() {
    this.address = 0;
    this.eraseAddress = 0;
  }

  /** Commands seen, as { cmd, body }. */
  commands() {
    return this.frames.map((b) => ({ cmd: b[0], body: b }));
  }

  // ---- Transport interface ----
  async write(u8) {
    for (const b of u8) this.inBuf.push(b);
    this._parse();
  }

  read(n, timeoutMs) {
    return new Promise((resolve, reject) => {
      const tryNow = () => {
        if (this.rx.length >= n) {
          resolve(Uint8Array.from(this.rx.splice(0, n)));
          return true;
        }
        return false;
      };
      if (tryNow()) return;
      const t = setTimeout(() => {
        this.waiter = null;
        reject(timeoutError());
      }, timeoutMs);
      this.waiter = () => {
        if (tryNow()) {
          clearTimeout(t);
          this.waiter = null;
        }
      };
    });
  }

  async setDTR(on) {
    this.dtr.push(on);
    if (on && this.dtr.length >= 2 && this.dtr[this.dtr.length - 2] === false) {
      // Rising edge after a low pulse: the board resets into the bootloader.
      this._resetState();
      this.inBootloader = true;
      this.left = false;
      for (let i = 0; i < this.junkAfterReset; i++) this._send([0x00]);
    }
  }

  async drain() {
    this.rx.length = 0;
  }

  // ---- bootloader ----
  _send(bytes) {
    for (const b of bytes) this.rx.push(b);
    if (this.waiter) this.waiter();
  }

  _parse() {
    for (;;) {
      while (this.inBuf.length && this.inBuf[0] !== 0x1b) this.inBuf.shift();
      if (this.inBuf.length < 5) return;
      const size = (this.inBuf[2] << 8) | this.inBuf[3];
      if (this.inBuf[4] !== 0x0e) {
        this.inBuf.shift();
        continue;
      }
      const total = 5 + size + 1;
      if (this.inBuf.length < total) return;
      const frame = this.inBuf.splice(0, total);
      const seq = frame[1];
      if (xor(frame, total) !== 0) {
        this.checksumErrors++;
        this._answer(seq, [0xb0, 0xc1]);
        continue;
      }
      const body = frame.slice(5, 5 + size);
      this.frames.push(body);
      const ans = this._handle(body);
      if (ans) this._answer(seq, ans);
    }
  }

  _answer(seq, body) {
    if (this.answered >= this.stopAfterFrames) return;
    this.answered++;
    const out = [0x1b, seq, (body.length >> 8) & 0xff, body.length & 0xff, 0x0e].concat(body);
    out.push(xor(out, out.length));
    this._send(out);
  }

  _handle(b) {
    switch (b[0]) {
      case 0x01: // SIGN_ON
        return [0x01, 0x00, 8].concat(Array.from("AVRISP_2", (c) => c.charCodeAt(0)));
      case 0x10: // ENTER_PROGMODE_ISP
        return [0x10, 0x00];
      case 0x11: // LEAVE_PROGMODE_ISP
        this.left = true;
        this.inBootloader = false;
        return [0x11, 0x00];
      case 0x1b: { // READ_SIGNATURE_ISP
        const i = b[4];
        const v = i === 0 ? this.signature[0] : i === 1 ? this.signature[1] : this.signature[2];
        return [0x1b, 0x00, v, 0x00];
      }
      case 0x06: { // LOAD_ADDRESS: word address, 32-bit shift drops bit 31
        const w = ((b[1] << 24) | (b[2] << 16) | (b[3] << 8) | b[4]) >>> 0;
        this.address = (w << 1) >>> 0;
        return [0x06, 0x00];
      }
      case 0x13: { // PROGRAM_FLASH_ISP
        const size = (b[1] << 8) | b[2];
        if (this.eraseAddress < FLASH_SIZE - 8192) {
          this.flash.fill(0xff, this.eraseAddress, this.eraseAddress + PAGE);
          this.eraseAddress += PAGE;
        }
        for (let i = 0; i < size; i++) {
          const a = this.address + i;
          this.flash[a] &= b[10 + i];
          if (a === this.corruptAt) this.flash[a] ^= 0x01;
        }
        this.address += size;
        this.programWrites++;
        return [0x13, 0x00];
      }
      case 0x14: { // READ_FLASH_ISP
        const size = (b[1] << 8) | b[2];
        const out = [0x14, 0x00];
        for (let i = 0; i < size; i++) out.push(this.flash[this.address + i]);
        this.address += size;
        out.push(0x00);
        return out;
      }
      default:
        return [b[0], 0xc0]; // STATUS_CMD_FAILED
    }
  }
}

module.exports = { FakeBootloader, FLASH_SIZE };
