/**
 * Reads a pasted ShadowMD sketch (Lite or classic Shadow MD, one block or several tabs):
 * variant, MarcDuino baud, USB forwarding, extras and the MarcDuino function per button.
 * Plain script (no modules) so index.html still opens from file://. Never throws.
 */
(function (root) {
  "use strict";

  /** Removes // and block comments, keeping string/char literals and line breaks. */
  function stripComments(src) {
    let out = "";
    const n = src.length;
    let i = 0;
    while (i < n) {
      const c = src[i];
      const d = src[i + 1];
      if (c === "/" && d === "/") {
        while (i < n && src[i] !== "\n") i++;
      } else if (c === "/" && d === "*") {
        i += 2;
        while (i < n && !(src[i] === "*" && src[i + 1] === "/")) {
          if (src[i] === "\n") out += "\n";
          i++;
        }
        i += 2;
      } else if (c === '"' || c === "'") {
        let j = i + 1;
        while (j < n && src[j] !== c && src[j] !== "\n") j += src[j] === "\\" ? 2 : 1;
        out += src.slice(i, Math.min(j + 1, n));
        i = j + 1;
      } else {
        out += c;
        i++;
      }
    }
    return out;
  }

  function defineValue(text, name) {
    const m = new RegExp(String.raw`^[ \t]*#[ \t]*define[ \t]+` + name + String.raw`[ \t]+(\d+)`, "m").exec(text);
    return m ? parseInt(m[1], 10) : null;
  }

  /** Body of the function whose name is followed by "(" ... ")" "{", or null. */
  function functionBody(text, name) {
    const re = new RegExp(String.raw`\b` + name + String.raw`\s*\([^)]*\)\s*\{`, "g");
    const m = re.exec(text);
    if (!m) return null;
    let depth = 1;
    let i = m.index + m[0].length;
    const start = i;
    while (i < text.length && depth > 0) {
      if (text[i] === "{") depth++;
      else if (text[i] === "}") depth--;
      i++;
    }
    return text.slice(start, i);
  }

  function detectSketch(files) {
    const r = {
      variant: "unknown",
      marcDuinoBaud: null,
      forwarding: null,
      ros2d2: null,
      domeTracker: null,
      buttons: [],
      warnings: [],
    };
    try {
      const list = Array.isArray(files) ? files : [];
      const raw = list
        .map((f) => (f && typeof f.text === "string" ? f.text : ""))
        .join("\n");
      if (!raw.trim()) {
        r.warnings.push("not-shadow");
        return r;
      }
      const text = stripComments(raw);

      const fwdDefine = defineValue(text, "ENABLE_PC_TO_MARCDUINO_FORWARDING");
      const hasFwdName = /\bENABLE_PC_TO_MARCDUINO_FORWARDING\b/.test(text);
      const isLite = hasFwdName && /\bprocessPCSerial/.test(text);
      const hasButtons = /\b\w+_MD_func\b/.test(text);

      if (isLite) r.variant = "lite";
      else if (/\bbtnUP_MD_func\b/.test(text)) r.variant = "classic";
      else if (hasButtons) r.variant = "classic";
      if (r.variant === "unknown") r.warnings.push("not-shadow");

      const baud = /\bint\s+marcDuinoBaudRate\s*=\s*(\d+)/.exec(text);
      r.marcDuinoBaud = baud ? parseInt(baud[1], 10) : null;
      if (r.variant !== "unknown" && r.marcDuinoBaud === null) r.warnings.push("no-baud");

      const ros = defineValue(text, "ENABLE_ROS2D2");
      if (r.variant === "lite") {
        r.forwarding = fwdDefine === 1;
        r.ros2d2 = ros === null ? false : ros === 1;
        const dt = defineValue(text, "ENABLE_DOME_TRACKER");
        r.domeTracker = dt === null ? false : dt === 1;
      } else if (r.variant === "classic") {
        r.ros2d2 = ros === null ? null : ros === 1;
        r.domeTracker = r.ros2d2; // the classic dome tracker lives under ENABLE_ROS2D2
        const viaInclude = fwdDefine === 1 && /^[ \t]*#[ \t]*include[ \t]+"PcForwarding\.h"/m.test(text);
        const body = functionBody(text, "rosHandleLine");
        const viaRos = ros === 1 && body !== null && /\bpcFwdHandleLine\b/.test(body);
        r.forwarding = viaInclude || viaRos;
      }

      if (r.variant !== "unknown") {
        const seen = new Set();
        const re = /\bint\s+(\w+?)_MD_func\s*=\s*(-?\d+)/g;
        let m;
        while ((m = re.exec(text)) !== null) {
          const name = m[1];
          if (seen.has(name)) continue;
          seen.add(name);
          const p = new RegExp(String.raw`\bint\s+` + name + String.raw`_cust_panel\s*=\s*(-?\d+)`).exec(text);
          r.buttons.push({
            button: name,
            mdFunc: parseInt(m[2], 10),
            custPanel: p ? parseInt(p[1], 10) : null,
          });
        }
        if (
          /^[ \t]*#[ \t]*(define|include)[ \t]+(ENABLE_PC_TO_MARCDUINO_FORWARDING|ENABLE_ROS2D2|"?PcForwarding)/m.test(
            raw.replace(/\/\*[\s\S]*?\*\//g, ""),
          ) === false &&
          /^[ \t]*\/\/[ \t]*#[ \t]*define[ \t]+(ENABLE_PC_TO_MARCDUINO_FORWARDING|ENABLE_ROS2D2)\b/m.test(raw)
        ) {
          r.warnings.push("commented-defines");
        }
      }
    } catch (e) {
      r.variant = "unknown";
      r.buttons = [];
      r.warnings.push("parse-error");
    }
    return r;
  }

  const api = { detectSketch, stripComments };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.InoDetect = api;
})(this);
