"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const { parseIntelHex } = require("../lib/intel-hex.js");

function codeOf(fn) {
  try {
    fn();
  } catch (e) {
    return e.code;
  }
  return "no error";
}

test("parseIntelHex reads a 2-record file and checks the checksum", () => {
  const text = ":0400000001020304F2\r\n:00000001FF\r\n";
  const r = parseIntelHex(text);
  assert.equal(r.size, 4);
  assert.deepEqual(Array.from(r.bytes), [1, 2, 3, 4]);
});

test("parseIntelHex fills gaps with 0xFF", () => {
  const r = parseIntelHex(":0100000011EE\n:0100040022D9\n:00000001FF\n");
  assert.equal(r.size, 5);
  assert.deepEqual(Array.from(r.bytes), [0x11, 0xff, 0xff, 0xff, 0x22]);
});

test("parseIntelHex handles type 02/04 extended addresses above 64 KB", () => {
  // 02: segment 0x1000 -> base 0x10000; 04: upper 0x0001 -> base 0x10000 too, then 0x0002 -> 0x20000
  const seg = ":020000021000EC\n:02001000AABB89\n:00000001FF\n";
  const r1 = parseIntelHex(seg);
  assert.equal(r1.size, 0x10012);
  assert.equal(r1.bytes[0x10010], 0xaa);
  assert.equal(r1.bytes[0x10011], 0xbb);
  assert.equal(r1.bytes[0], 0xff);

  const lin = ":020000040002F8\n:01000500CC2E\n:00000001FF\n";
  const r2 = parseIntelHex(lin);
  assert.equal(r2.size, 0x20006);
  assert.equal(r2.bytes[0x20005], 0xcc);
});

test("parseIntelHex round-trips a 140 KB image written by toIntelHex", () => {
  const img = new Uint8Array(140 * 1024);
  for (let i = 0; i < img.length; i++) img[i] = (i * 31 + 5) & 0xff;
  const r = parseIntelHex(toIntelHex(img));
  assert.equal(r.size, img.length);
  assert.deepEqual(r.bytes, img);
});

test("parseIntelHex rejects > 253952 bytes with too-big", () => {
  // one byte at 0x3E000 (the bootloader section) -> 253953 bytes
  const line = rec(0xe000, 0x00, [0x00]);
  assert.equal(codeOf(() => parseIntelHex(":020000040003F7\n" + line + "\n:00000001FF\n")), "too-big");
  // exactly 253952 bytes is fine
  const ok = rec(0xdfff, 0x00, [0x12]);
  assert.equal(parseIntelHex(":020000040003F7\n" + ok + "\n:00000001FF\n").size, 253952);
});

test("parseIntelHex rejects a wrong checksum with bad-checksum", () => {
  assert.equal(codeOf(() => parseIntelHex(":0400000001020304F3\n:00000001FF\n")), "bad-checksum");
});

test("parseIntelHex rejects garbage lines with bad-line and an empty file with empty", () => {
  assert.equal(codeOf(() => parseIntelHex("hello\n")), "bad-line");
  assert.equal(codeOf(() => parseIntelHex(":04000000010203\n")), "bad-line");
  assert.equal(codeOf(() => parseIntelHex(":0400000001020304F2\n:00000009F7\n")), "bad-line");
  assert.equal(codeOf(() => parseIntelHex("")), "empty");
  assert.equal(codeOf(() => parseIntelHex(":00000001FF\n")), "empty");
});

test("parseIntelHex rejects a file without the type-01 end record with truncated", () => {
  // a .hex cut short (download interrupted, copy-paste lost the tail): data but no :00000001FF
  assert.equal(codeOf(() => parseIntelHex(":0400000001020304F2\n")), "truncated");
  assert.equal(codeOf(() => parseIntelHex(":0400000001020304F2\r\n:0100040022D9\r\n")), "truncated");
  const img = new Uint8Array(4096).fill(0x5a);
  const full = toIntelHex(img);
  const cut = full.slice(0, full.lastIndexOf(":00000001FF"));
  assert.equal(codeOf(() => parseIntelHex(cut)), "truncated");
  // the end record may be followed by blank lines
  assert.equal(parseIntelHex(full + "\r\n\r\n").size, 4096);
});

function toIntelHex(img) {
  const lines = [];
  let upper = -1;
  for (let a = 0; a < img.length; a += 16) {
    if (a >> 16 !== upper) {
      upper = a >> 16;
      lines.push(rec(0, 0x04, [(upper >> 8) & 0xff, upper & 0xff]));
    }
    lines.push(rec(a & 0xffff, 0x00, Array.from(img.subarray(a, Math.min(a + 16, img.length)))));
  }
  lines.push(":00000001FF");
  return lines.join("\r\n") + "\r\n";
}

function rec(addr, type, data) {
  const b = [data.length, (addr >> 8) & 0xff, addr & 0xff, type].concat(data);
  const sum = (0x100 - (b.reduce((a, x) => a + x, 0) & 0xff)) & 0xff;
  return ":" + b.concat([sum]).map((x) => x.toString(16).toUpperCase().padStart(2, "0")).join("");
}
