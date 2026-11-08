/**
 * Intel HEX reader for the .hex files the Arduino IDE exports ("Export compiled binary").
 * Plain script (no modules) so index.html still opens from file://.
 *
 * parseIntelHex(text) -> { bytes: Uint8Array (from address 0, gaps = 0xFF), size }
 * Errors carry .code: "bad-line" | "bad-checksum" | "too-big" | "empty".
 */
(function (root) {
  "use strict";

  /** Application flash of an ATmega2560: 256 KB minus the 8 KB bootloader section. */
  const MAX_SIZE = 253952;

  function fail(code, msg) {
    const e = new Error(msg);
    e.code = code;
    return e;
  }

  function parseIntelHex(text) {
    const buf = new Uint8Array(MAX_SIZE).fill(0xff);
    let size = 0;
    let base = 0;
    let sawData = false;
    const lines = String(text == null ? "" : text).split(/\r?\n|\r/);

    for (let n = 0; n < lines.length; n++) {
      const line = lines[n].trim();
      const where = "line " + (n + 1);
      if (line === "") continue;
      if (line[0] !== ":" || line.length < 11 || (line.length - 1) % 2 !== 0 || !/^:[0-9A-Fa-f]+$/.test(line)) {
        throw fail("bad-line", where + ": not an Intel HEX record");
      }
      const rec = new Uint8Array((line.length - 1) / 2);
      for (let i = 0; i < rec.length; i++) rec[i] = parseInt(line.substr(1 + i * 2, 2), 16);
      const count = rec[0];
      if (rec.length !== count + 5) throw fail("bad-line", where + ": byte count does not match the record length");
      let sum = 0;
      for (let i = 0; i < rec.length; i++) sum = (sum + rec[i]) & 0xff;
      if (sum !== 0) throw fail("bad-checksum", where + ": wrong checksum");
      const offset = (rec[1] << 8) | rec[2];
      const type = rec[3];
      const data = rec.subarray(4, 4 + count);

      if (type === 0x00) {
        const addr = base + offset;
        if (addr + count > MAX_SIZE) {
          throw fail("too-big", where + ": data at 0x" + addr.toString(16) + " goes past " + MAX_SIZE + " bytes (the Mega's application flash)");
        }
        buf.set(data, addr);
        if (count > 0) sawData = true;
        if (addr + count > size) size = addr + count;
      } else if (type === 0x01) {
        break;
      } else if (type === 0x02) {
        if (count !== 2) throw fail("bad-line", where + ": bad extended segment address record");
        base = ((data[0] << 8) | data[1]) * 16;
      } else if (type === 0x04) {
        if (count !== 2) throw fail("bad-line", where + ": bad extended linear address record");
        base = ((data[0] << 8) | data[1]) * 65536;
      } else if (type === 0x03 || type === 0x05) {
        // start address records: no effect on the flash image
      } else {
        throw fail("bad-line", where + ": unknown record type " + type);
      }
    }

    if (!sawData) throw fail("empty", "the file has no data");
    return { bytes: buf.slice(0, size), size };
  }

  const api = { parseIntelHex, MAX_SIZE };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.IntelHex = api;
})(this);
