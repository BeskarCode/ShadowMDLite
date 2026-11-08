/**
 * Classic Shadow MD (Eebel) to ShadowMD Lite migrator, without the DOM.
 * ShadowMD Lite is a fork of the original Shadow MD.
 *   readClassic(files)       what the classic sketch has: variant, settings, buttons
 * Plain script (no modules) so index.html still opens from file://. Needs lib/sketch-text.js.
 */
(function (root) {
  "use strict";

  const SketchText = typeof module !== "undefined" && module.exports ? require("./sketch-text.js") : root.SketchText;

  /** Settings copied by value (as written in the sketch). */
  const SETTINGS = [
    "PS3ControllerFootMac", "PS3ControllerDomeMAC", "PS3ControllerBackupFootMac", "PS3ControllerBackupDomeMAC",
    "drivespeed1", "drivespeed2", "turnspeed", "domespeed", "ramping",
    "joystickFootDeadZoneRange", "joystickDomeDeadZoneRange", "driveDeadBandRange", "invertTurnDirection",
    "domeAutoSpeed", "time360DomeTurn", "marcDuinoBaudRate", "motorControllerBaudRate", "serialLatency", "CustomSongMax",
  ];
  const DEFINES = ["SABERTOOTH_ADDR", "SYREN_ADDR", "ENABLE_ROS2D2", "SHADOW_DEBUG", "SHADOW_VERBOSE"];

  const DIRS = [["UP", "Up"], ["Left", "Left"], ["Right", "Right"], ["Down", "Down"]];
  const MODS = [["", ""], ["CROSS", "Cross"], ["CIRCLE", "Circle"], ["PS", "PS"], ["L1", "L1"]];

  /** Classic button name -> { lite, controller }: btn… is the FOOT controller, FTbtn… the DOME one. */
  function eebelButtonMap() {
    const map = {};
    [["btn", "footBtn", "foot"], ["FTbtn", "domeBtn", "dome"]].forEach(([cp, lp, controller]) => {
      DIRS.forEach(([cd, ld]) => {
        MODS.forEach(([cm, lm]) => {
          map[cp + cd + (cm ? "_" + cm : "")] = { lite: lp + ld + lm, controller };
        });
      });
    });
    return map;
  }

  const VARIANTS = {
    eebel: {
      recognize: (idx) => "btnUP_type" in idx.vars && "FTbtnUP_MD_func" in idx.vars,
      buttonMap: eebelButtonMap(),
      settings: SETTINGS,
    },
  };

  /**
   * First active definition of every variable and #define, across the files in order:
   * vars[name] / defines[name] = { value, file, line }. Comments are ignored.
   */
  function indexSketch(files) {
    const vars = {};
    const defines = {};
    (files || []).forEach((f) => {
      const lines = SketchText.stripComments(String(f.text || "").replace(/\r\n?/g, "\n")).split("\n");
      lines.forEach((l, i) => {
        const d = /^\s*#\s*define\s+(\w+)\b[ \t]*(.*?)\s*$/.exec(l);
        if (d) {
          if (!(d[1] in defines)) defines[d[1]] = { value: d[2], file: f.name, line: i + 1 };
          return;
        }
        const v = /^\s*(?:static\s+|const\s+|unsigned\s+|volatile\s+)*[A-Za-z_]\w*\s+(\w+)\s*=\s*(.*?)\s*;/.exec(l);
        if (v && !(v[1] in vars)) vars[v[1]] = { value: v[2], file: f.name, line: i + 1 };
      });
    });
    return { vars, defines };
  }

  function num(v, dflt) {
    const n = v ? parseInt(v.value, 0) : NaN;
    return Number.isFinite(n) ? n : dflt;
  }

  /** The text of a C string literal ("a\"b" -> a"b); anything else as it is. */
  function cString(v) {
    if (!v) return "";
    const s = v.value;
    if (!/^".*"$/.test(s)) return s;
    return s.slice(1, -1).replace(/\\(.)/g, (m, c) => ({ n: "\n", r: "\r", t: "\t" }[c] || c));
  }

  function readButton(idx, classic, info) {
    const at = idx.vars[classic + "_MD_func"];
    const v = (suffix) => idx.vars[classic + suffix];
    const dp = [];
    for (let n = 1; n <= 10; n++) {
      dp.push({
        n,
        use: /^true$/i.test((v("_use_DP" + n) || {}).value || ""),
        start: num(v("_DP" + n + "_open_start_delay"), 0),
        open: num(v("_DP" + n + "_stay_open_time"), 0),
      });
    }
    return {
      classic,
      lite: info.lite,
      controller: info.controller,
      type: num(v("_type"), 1),
      mdFunc: num(at, 0),
      mp3: num(v("_cust_MP3_num"), 0),
      ldType: num(v("_cust_LD_type"), 0),
      ldText: cString(v("_cust_LD_text")),
      panel: num(v("_cust_panel"), 0),
      dp,
      file: at.file,
      line: at.line,
    };
  }

  /**
   * What a classic sketch has. variant: "eebel" or null; looksLike (when null): "lite" | "unknown".
   * settings / defines: values as written; buttons: one per classic button found.
   */
  function readClassic(files) {
    const idx = indexSketch(files);
    const id = Object.keys(VARIANTS).find((k) => VARIANTS[k].recognize(idx)) || null;
    const out = { variant: id, looksLike: null, settings: {}, defines: {}, buttons: [] };
    if (!id) {
      out.looksLike = "footBtnUp_MD_func" in idx.vars ? "lite" : "unknown";
      return out;
    }
    const V = VARIANTS[id];
    V.settings.forEach((s) => {
      if (idx.vars[s]) out.settings[s] = idx.vars[s].value;
    });
    DEFINES.forEach((d) => {
      if (idx.defines[d]) out.defines[d] = idx.defines[d].value;
    });
    Object.keys(V.buttonMap).forEach((classic) => {
      if (idx.vars[classic + "_MD_func"]) out.buttons.push(readButton(idx, classic, V.buttonMap[classic]));
    });
    return out;
  }

  const api = { VARIANTS, SETTINGS, indexSketch, readClassic };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.MigrateClassic = api;
})(this);
