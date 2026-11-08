/**
 * Pure helpers for the guided channel walk (no DOM): which commands move a channel, which
 * part is already on it, the default name of a new part and the range to walk.
 * Plain script (no modules) so index.html still opens from file://.
 */
(function (root) {
  "use strict";

  const MAX_CHANNEL = 99;
  const DEFAULT_TO = 13;

  function channelStr(n) {
    return String(n).padStart(2, "0");
  }

  /** Open and close lines for channel n of a board ("S1:" dome, "S3:" body). */
  function walkCommands(board, n) {
    const ch = channelStr(n);
    return { open: board + ":OP" + ch, close: board + ":CL" + ch };
  }

  /** The servo marker on that board and channel, or null (holos are not walked). */
  function findServo(cfg, board, n) {
    const ch = channelStr(n);
    const list = cfg && Array.isArray(cfg.markers) ? cfg.markers : [];
    return list.find((m) => m.kind === "servo" && m.board === board && m.channel === ch) || null;
  }

  function defaultName(board, n) {
    const name = (board === "S3:" ? "Body " : "Dome ") + channelStr(n);
    return { name, nameEs: name };
  }

  function toInt(v, fallback) {
    const n = parseInt(v, 10);
    return Number.isFinite(n) ? n : fallback;
  }

  /** from/to as integers in 1..99 with from <= to; to is 13 when not given. */
  function normalizeRange(from, to) {
    const f = Math.min(MAX_CHANNEL, Math.max(1, toInt(from, 1)));
    const t = Math.min(MAX_CHANNEL, Math.max(1, toInt(to, DEFAULT_TO)));
    return { from: f, to: Math.max(f, t) };
  }

  const api = { channelStr, walkCommands, findServo, defaultName, normalizeRange, MAX_CHANNEL, DEFAULT_TO };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.ChannelWalk = api;
})(this);
