/**
 * Droid configuration model: markers (servos and holos) with board, channel and position.
 * Plain script (no modules) so index.html still opens from file://.
 */
(function (root) {
  "use strict";

  const STORAGE_KEY = "shadowmd_droid_config_v1";
  const FORMAT = "shadowmd-droid-config";
  const VERSION = 1;
  const CHANNEL_RE = /^(0[1-9]|[1-9][0-9])$/;

  /** The layout that used to be hard-coded in index.html, plus holo axes. */
  const EXAMPLE_ZONES = [
    // Markers aligned with the reference blueprint image.
    { id: "s0", label: "Panel 4 (Lower Panel)", labelEs: "Panel 4 (panel inferior)", kind: "servo", servoIndex: 0, mdPanel: "04", target: "S1:", map: { left: 95.19, top: 26.94, w: 3.2, h: 3.2 } },
    { id: "s1", label: "Panel 3 (Lower Panel)", labelEs: "Panel 3 (panel inferior)", kind: "servo", servoIndex: 1, mdPanel: "03", target: "S1:", map: { left: 94.6, top: 31.6, w: 3.2, h: 3.2 } },
    { id: "s2", label: "Panel 2 (Lower Panel)", labelEs: "Panel 2 (panel inferior)", kind: "servo", servoIndex: 2, mdPanel: "02", target: "S1:", map: { left: 91.7, top: 35.7, w: 3.2, h: 3.2 } },
    { id: "s3", label: "Panel 1 (Lower Panel)", labelEs: "Panel 1 (panel inferior)", kind: "servo", servoIndex: 3, mdPanel: "01", target: "S1:", map: { left: 88.37, top: 38.94, w: 3.2, h: 3.2 } },
    { id: "s4", label: "Panel 5 (Lower Panel)", labelEs: "Panel 5 (panel inferior)", kind: "servo", servoIndex: 4, mdPanel: "05", target: "S1:", map: { left: 53.83, top: 36.56, w: 3.2, h: 3.2 } },
    { id: "s5", label: "Panel 6 (Lower Panel)", labelEs: "Panel 6 (panel inferior)", kind: "servo", servoIndex: 5, mdPanel: "06", target: "S1:", map: { left: 49.65, top: 29.77, w: 3.2, h: 3.2 } },
    // Upper: diagram number ≠ Marcduino; mdPanel is the real command index.
    { id: "s8", label: "Panel 10 (Upper Panel)", labelEs: "Panel 10 (panel superior)", kind: "servo", servoIndex: 8, mdPanel: "07", target: "S1:", map: { left: 61.77, top: 23.09, w: 3.2, h: 3.2 } },
    { id: "s9", label: "Panel 9 (Upper Panel)", labelEs: "Panel 9 (panel superior)", kind: "servo", servoIndex: 9, mdPanel: "08", target: "S1:", map: { left: 65.95, top: 30.22, w: 3.2, h: 3.2 } },
    { id: "s10", label: "Panel 8 (Upper Panel)", labelEs: "Panel 8 (panel superior)", kind: "servo", servoIndex: 10, mdPanel: "09", target: "S1:", map: { left: 77.37, top: 30.33, w: 3.2, h: 3.2 } },
    { id: "s11", label: "Panel 7 (Upper Panel)", labelEs: "Panel 7 (panel superior)", kind: "servo", servoIndex: 11, mdPanel: "10", target: "S1:", map: { left: 82.24, top: 24.45, w: 3.2, h: 3.2 } },
    { id: "s12", label: "Circular Top Panel", labelEs: "Panel circular superior", kind: "servo", servoIndex: 12, mdPanel: "11", target: "S1:" },
    // Body (S3:) — mdPanel for :MV/:OP/:CL/#SO/#SC (body servo 1→01 … 9→09).
    { id: "b1", label: "Body 1 · DPL - DataPanel Door", labelEs: "Body 1 · DPL - Puerta datapanel", kind: "servo", servoIndex: 1, mdPanel: "01", target: "S3:", map: { left: 40.32, top: 61.46, w: 3.2, h: 3.2 } },
    { id: "b2", label: "Body 2 · UtlArmU - Upper Utility Arm", labelEs: "Body 2 · UtlArmU - Brazo útil superior", kind: "servo", servoIndex: 2, mdPanel: "02", target: "S3:", map: { left: 32.24, top: 46.18, w: 3.2, h: 3.2 } },
    { id: "b3", label: "Body 3 · UtlArmL - Lower Utility Arm", labelEs: "Body 3 · UtlArmL - Brazo útil inferior", kind: "servo", servoIndex: 3, mdPanel: "03", target: "S3:", map: { left: 27.93, top: 51.04, w: 3.2, h: 3.2 } },
    { id: "b4", label: "Body 4 · LBdyDr - Left BreadPan Door", labelEs: "Body 4 · LBdyDr - Puerta izquierda", kind: "servo", servoIndex: 4, mdPanel: "04", target: "S3:", map: { left: 15.53, top: 58.74, w: 3.2, h: 3.2 } },
    { id: "b5", label: "Body 5 · LArm - Left Arm Raise/Lower", labelEs: "Body 5 · LArm - Brazo izq. subir/bajar", kind: "servo", servoIndex: 5, mdPanel: "05", target: "S3:", map: { left: 15.81, top: 53.87, w: 3.2, h: 3.2 } },
    { id: "b6", label: "Body 6 · LArmTool - Left Arm Tool", labelEs: "Body 6 · LArmTool - Herramienta brazo izq.", kind: "servo", servoIndex: 6, mdPanel: "06", target: "S3:", map: { left: 15.95, top: 49.01, w: 3.2, h: 3.2 } },
    { id: "b7", label: "Body 7 · ToolRBdyDr - Right BreadPan Door", labelEs: "Body 7 · ToolRBdyDr - Puerta derecha", kind: "servo", servoIndex: 7, mdPanel: "07", target: "S3:", map: { left: 46.45, top: 59.76, w: 3.2, h: 3.2 } },
    { id: "b8", label: "Body 8 · RArm - Right Arm Raise/Lower", labelEs: "Body 8 · RArm - Brazo dcho. subir/bajar", kind: "servo", servoIndex: 8, mdPanel: "08", target: "S3:", map: { left: 46.59, top: 55.12, w: 3.2, h: 3.2 } },
    { id: "b9", label: "Body 9 · RArmTool - Right Arm Tool", labelEs: "Body 9 · RArmTool - Herramienta brazo dcho.", kind: "servo", servoIndex: 9, mdPanel: "09", target: "S3:", map: { left: 47.15, top: 48.89, w: 3.2, h: 3.2 } },
    { id: "holo_f", label: "Front Holo", labelEs: "Holo frontal", kind: "holo", holoId: "01", target: "S1:", axes: ["h", "v"], maps: [
      { left: 82.1, top: 43.7, w: 3.2, h: 3.2 },
      { left: 81.4, top: 37.3, w: 3.2, h: 3.2 },
    ] },
    { id: "holo_r", label: "Rear Holo", labelEs: "Holo trasero", kind: "holo", holoId: "03", target: "S1:", axes: ["h", "v"], maps: [
      { left: 71.3, top: 7.0, w: 3.2, h: 3.2 },
      { left: 65.53, top: 4.19, w: 3.2, h: 3.2 },
    ] },
    { id: "holo_t", label: "Top Holo", labelEs: "Holo superior", kind: "holo", holoId: "02", target: "S1:", axes: [], maps: [
      { left: 81.96, top: 14.60, w: 3.2, h: 3.2 },
      { left: 75.14, top: 14.60, w: 3.2, h: 3.2 },
    ] },
  ];

  function err(code, message, extra) {
    const e = new Error(message);
    e.code = code;
    if (extra) Object.assign(e, extra);
    return e;
  }

  function zoneToMarker(z) {
    const m = { id: z.id, name: z.label, nameEs: z.labelEs, kind: z.kind, board: z.target };
    if (z.kind === "holo") {
      m.holoId = z.holoId;
      m.axes = (z.axes || []).slice();
    } else {
      m.channel = z.mdPanel;
    }
    const maps = z.maps || (z.map ? [z.map] : []);
    if (maps.length) m.map2d = maps.map((r) => ({ left: r.left, top: r.top, w: r.w, h: r.h }));
    return m;
  }

  function emptyConfig() {
    return { version: VERSION, markers: [] };
  }

  function exampleConfig() {
    return { version: VERSION, markers: EXAMPLE_ZONES.map(zoneToMarker) };
  }

  function validateConfig(cfg) {
    const errors = [];
    const seen = {};
    const markers = cfg && Array.isArray(cfg.markers) ? cfg.markers : [];
    markers.forEach((m) => {
      if (m.kind !== "servo" && m.kind !== "holo") {
        errors.push({ id: m.id, code: "bad-kind" });
        return;
      }
      if (typeof m.name !== "string" || !m.name.trim()) errors.push({ id: m.id, code: "missing-name" });
      const ch = m.kind === "holo" ? m.holoId : m.channel;
      if (typeof ch !== "string" || !CHANNEL_RE.test(ch)) {
        errors.push({ id: m.id, code: "bad-channel" });
        return;
      }
      if (m.kind === "servo") {
        const key = m.board + ch;
        if (seen[key] !== undefined) errors.push({ id: m.id, code: "duplicate-channel", other: seen[key] });
        else seen[key] = m.id;
      }
    });
    return { ok: errors.length === 0, errors };
  }

  function serializeConfig(cfg, pwmRanges) {
    const out = { format: FORMAT, version: VERSION, markers: cfg.markers };
    if (pwmRanges) out.pwmRanges = pwmRanges;
    return JSON.stringify(out, null, 2);
  }

  function parseConfig(text) {
    let data;
    try {
      data = JSON.parse(text);
    } catch (e) {
      throw err("bad-json", "Invalid JSON");
    }
    if (!data || typeof data !== "object" || data.format !== FORMAT || !Array.isArray(data.markers)) {
      throw err("bad-format", "Not a droid configuration");
    }
    if (data.version !== VERSION) throw err("bad-version", "Unsupported version: " + data.version);
    const config = { version: VERSION, markers: data.markers };
    const v = validateConfig(config);
    if (!v.ok) throw err("invalid", "Invalid configuration", { errors: v.errors });
    return { config, pwmRanges: data.pwmRanges && typeof data.pwmRanges === "object" ? data.pwmRanges : null };
  }

  function loadConfig(storage) {
    try {
      const raw = storage.getItem(STORAGE_KEY);
      if (!raw) return null;
      return parseConfig(raw).config;
    } catch (e) {
      return null;
    }
  }

  function saveConfig(storage, cfg) {
    try {
      storage.setItem(STORAGE_KEY, serializeConfig(cfg));
    } catch (e) {
      /* storage unavailable or full: ignore */
    }
  }

  function toZones(cfg) {
    return cfg.markers.map((m) => {
      const z = { id: m.id, label: m.name, labelEs: m.nameEs || m.name, kind: m.kind, target: m.board };
      if (m.kind === "holo") {
        z.holoId = m.holoId;
        z.axes = (m.axes || []).slice();
      } else {
        z.mdPanel = m.channel;
      }
      if (m.pos3d) z.pos3d = m.pos3d;
      if (m.part) z.part = m.part;
      const maps = m.map2d || [];
      if (maps.length === 1) z.map = maps[0];
      else if (maps.length > 1) z.maps = maps;
      return z;
    });
  }

  function newMarkerId(cfg) {
    const used = new Set(cfg.markers.map((m) => m.id));
    let n = 1;
    while (used.has("m" + n)) n++;
    return "m" + n;
  }

  const api = {
    STORAGE_KEY, EXAMPLE_ZONES, exampleConfig, emptyConfig, validateConfig,
    serializeConfig, parseConfig, loadConfig, saveConfig, toZones, newMarkerId,
  };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.DroidConfig = api;
})(this);
