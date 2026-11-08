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

  // ---- custom (type 2) buttons, as the classic sketch sends them (d_FUNCTIONS.ino,
  // marcDuinoButtonPush type == 2, and custMarcDuinoPanel)
  const GAP_MS = 100; // MARCDUINO_CMD_GAP_MS in ShadowMD Lite

  function soundCommand(mp3) {
    if (mp3 === 182) return "$87";
    if (mp3 === 183) return "$88";
    if (mp3 >= 184 && mp3 <= 200) return "$8" + String(mp3 - 175).padStart(2, "0"); // 184 -> $809 … 200 -> $825
    if (mp3 === 201) return "$82";
    if (mp3 === 202) return "$803";
    return null;
  }

  const LD_COMMANDS = { 1: "@0T1", 2: "@0T4", 3: "@0T5", 4: "@0T6", 5: "@0T10", 6: "@0T11", 7: "@0T92" };

  /**
   * The commands a custom button sends, in order, each with the wait before the next one:
   * [{ cmd, delayAfterMs }]. Same commands and timing as the classic sketch; waits are at
   * least 100 ms (what ShadowMD Lite leaves between MarcDuino commands).
   */
  function translateCustom(b) {
    const events = []; // { t, cmd }
    const at = (t, cmd) => events.push({ t, cmd: cmd + "\r" });
    const sound = soundCommand(b.mp3);
    if (sound) at(0, sound);
    if (b.panel >= 1 && b.panel <= 9) {
      at(0, ":CL00");
      if (b.panel >= 2 && b.panel <= 8) at(0, ":SE" + (49 + b.panel)); // 2 -> :SE51 … 8 -> :SE57
      if (b.panel === 9) {
        (b.dp || []).forEach((d) => {
          if (!d.use || d.start > 30) return;
          const ch = String(d.n).padStart(2, "0");
          at(d.start * 1000, ":OP" + ch);
          if (d.open > 0 && d.open < 31) at((d.start + d.open) * 1000, ":CL" + ch);
        });
      }
    }
    if (LD_COMMANDS[b.ldType]) at(0, LD_COMMANDS[b.ldType]);
    if (b.ldType === 8) {
      at(0, "@0T100");
      at(0, "@0M" + (b.ldText || ""));
    }
    const sorted = events.map((e, i) => ({ e, i })).sort((a, z) => a.e.t - z.e.t || a.i - z.i).map((x) => x.e);
    return sorted.map((e, i) => ({
      cmd: e.cmd,
      delayAfterMs: i + 1 < sorted.length ? Math.max(GAP_MS, sorted[i + 1].t - e.t) : GAP_MS,
    }));
  }

  /** A C string literal for s: quotes, backslashes, control and non-ASCII characters escaped. */
  function cLiteral(s) {
    const bytes = typeof TextEncoder !== "undefined" ? new TextEncoder().encode(String(s)) : Buffer.from(String(s), "utf8");
    let out = '"';
    for (const b of bytes) {
      if (b === 0x22) out += '\\"';
      else if (b === 0x5c) out += "\\\\";
      else if (b === 0x0d) out += "\\r";
      else if (b === 0x0a) out += "\\n";
      else if (b === 0x09) out += "\\t";
      else if (b < 0x20 || b > 0x7e) out += "\\" + b.toString(8).padStart(3, "0");
      else out += String.fromCharCode(b);
    }
    return out + '"';
  }

  const api = { VARIANTS, SETTINGS, indexSketch, readClassic, translateCustom, cLiteral };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.MigrateClassic = api;
})(this);
