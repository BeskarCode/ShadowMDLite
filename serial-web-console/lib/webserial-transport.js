/**
 * Web Serial transport for lib/stk500v2.js (flashMega).
 * Plain script (no modules) so index.html still opens from file://.
 *
 * createWebSerialTransport(port, trace?) -> Transport (trace(line) gets a timestamped log of
 * DTR changes and bytes sent/received, for diagnosing a board that does not answer), where `port` is an OPEN SerialPort
 * (the caller opens it at 115200 for the bootloader and closes it afterwards):
 *   write(u8)            sends bytes
 *   read(n, timeoutMs)   exactly n bytes, or rejects with code "timeout"
 *   setDTR(on)           port.setSignals({ dataTerminalReady: on, requestToSend: on })
 *   drain()              throws away the bytes received so far (input flush)
 *   abort()              the user cancelled: pending and later calls reject with code "cancelled"
 *   close()              stops reading and releases the stream locks (does not close the port)
 *
 * One reader for the whole session fills an internal buffer; read() takes from it.
 * If the port goes away (USB unplugged) pending and later calls reject with code
 * "disconnected" instead of waiting for a timeout.
 */
(function (root) {
  "use strict";

  function fail(code, msg, cause) {
    const e = new Error(msg);
    e.code = code;
    if (cause) e.cause = cause;
    return e;
  }

  /** Resolves when p settles or after ms, whichever is first (a stuck port must not hang the UI). */
  function bounded(p, ms) {
    return Promise.race([Promise.resolve(p).catch(() => {}), new Promise((r) => setTimeout(r, ms))]);
  }

  function hexBytes(u8) {
    return Array.from(u8, (b) => b.toString(16).toUpperCase().padStart(2, "0")).join(" ");
  }

  function createWebSerialTransport(port, trace) {
    const t0 = Date.now();
    const log = typeof trace === "function" ? (msg) => trace("+" + (Date.now() - t0) + "ms " + msg) : () => {};
    const reader = port.readable.getReader();
    const writer = port.writable.getWriter();
    let buf = new Uint8Array(0);
    let waiter = null; // { n, resolve, reject, timer }
    let dead = null; // error that ends the session ("disconnected" / "cancelled")
    let closed = false;

    function take(n) {
      const out = buf.slice(0, n);
      buf = buf.slice(n);
      return out;
    }

    function settle() {
      if (!waiter) return;
      const w = waiter;
      if (buf.length >= w.n) {
        waiter = null;
        clearTimeout(w.timer);
        w.resolve(take(w.n));
      } else if (dead) {
        waiter = null;
        clearTimeout(w.timer);
        w.reject(dead);
      }
    }

    function kill(err) {
      if (!dead) dead = err;
      settle();
    }

    const pump = (async () => {
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value && value.length) {
            log("RX " + hexBytes(value));
            const next = new Uint8Array(buf.length + value.length);
            next.set(buf);
            next.set(value, buf.length);
            buf = next;
            settle();
          }
        }
        if (!closed) kill(fail("disconnected", "the serial port stopped sending (cable unplugged?)"));
      } catch (e) {
        if (!closed) kill(fail("disconnected", "the serial port was lost (" + ((e && e.message) || e) + ")", e));
      }
    })();

    return {
      async write(u8) {
        if (dead) throw dead;
        try {
          log("TX " + hexBytes(u8));
          await writer.write(u8);
        } catch (e) {
          kill(fail("disconnected", "could not write to the serial port (" + ((e && e.message) || e) + ")", e));
          throw dead;
        }
      },

      read(n, timeoutMs) {
        return new Promise((resolve, reject) => {
          if (waiter) {
            reject(fail("protocol", "two reads at the same time"));
            return;
          }
          if (buf.length >= n) {
            resolve(take(n));
            return;
          }
          if (dead) {
            reject(dead);
            return;
          }
          const w = { n, resolve, reject, timer: null };
          w.timer = setTimeout(() => {
            if (waiter === w) waiter = null;
            reject(fail("timeout", "no answer from the board within " + timeoutMs + " ms"));
          }, Math.max(0, timeoutMs));
          waiter = w;
        });
      },

      async setDTR(on) {
        if (dead) throw dead;
        try {
          log("DTR/RTS " + (on ? "on" : "off"));
          await port.setSignals({ dataTerminalReady: !!on, requestToSend: !!on });
        } catch (e) {
          kill(fail("disconnected", "could not change DTR (" + ((e && e.message) || e) + ")", e));
          throw dead;
        }
      },

      async drain() {
        if (buf.length) log("drain " + buf.length + " bytes");
        buf = new Uint8Array(0);
      },

      abort() {
        kill(fail("cancelled", "cancelled by the user"));
      },

      /** Ends the session and releases the locks so the caller can port.close(). */
      async close() {
        closed = true;
        kill(fail("cancelled", "transport closed"));
        await bounded(reader.cancel(), 1000);
        await bounded(pump, 1000);
        try {
          reader.releaseLock();
        } catch (_) {}
        await bounded(writer.close(), 1000);
        try {
          writer.releaseLock();
        } catch (_) {}
      },
    };
  }

  const api = { createWebSerialTransport };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.WebSerialTransport = api;
})(this);
