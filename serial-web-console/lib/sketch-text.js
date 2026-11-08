/**
 * Small text tools for Arduino sketches, shared by the classic to Lite migrator:
 * comments out (line numbers kept), top-level functions, a normalised body to compare two
 * versions of a function, and a short hash.
 * Plain script (no modules) so index.html still opens from file://.
 */
(function (root) {
  "use strict";

  /** Comments become spaces (newlines kept, so line numbers do not move); strings are left alone. */
  function stripComments(text) {
    const s = String(text);
    let out = "";
    let i = 0;
    while (i < s.length) {
      const c = s[i];
      const n = s[i + 1];
      if (c === '"' || c === "'") {
        let j = i + 1;
        while (j < s.length && s[j] !== c && s[j] !== "\n") j += s[j] === "\\" ? 2 : 1;
        out += s.slice(i, j + 1);
        i = j + 1;
      } else if (c === "/" && n === "/") {
        while (i < s.length && s[i] !== "\n") {
          out += " ";
          i++;
        }
      } else if (c === "/" && n === "*") {
        const end = s.indexOf("*/", i + 2);
        const stop = end < 0 ? s.length : end + 2;
        out += s.slice(i, stop).replace(/[^\n]/g, " ");
        i = stop;
      } else {
        out += c;
        i++;
      }
    }
    return out;
  }

  const NOT_FUNCTIONS = new Set(["if", "while", "for", "switch", "catch", "return", "sizeof"]);

  /**
   * Top-level function definitions of every file: { key, name, file, line, body }.
   * `line` is the 1-based line of the name in its file; `key` is the name, or name#2, name#3…
   * for later functions with the same name; `body` runs from "{" to the matching "}".
   */
  function splitFunctions(files) {
    const out = [];
    const seen = {};
    (files || []).forEach((f) => {
      const s = stripComments(String(f.text || "").replace(/\r\n?/g, "\n"));
      let depth = 0;
      let headStart = 0;
      for (let i = 0; i < s.length; i++) {
        const c = s[i];
        if (c === '"' || c === "'") {
          let j = i + 1;
          while (j < s.length && s[j] !== c && s[j] !== "\n") j += s[j] === "\\" ? 2 : 1;
          i = j;
          continue;
        }
        if (depth === 0 && (c === ";" || c === "}")) headStart = i + 1;
        if (depth === 0 && c === "\n" && /^\s*#/.test(s.slice(headStart, i))) headStart = i + 1;
        if (c === "{") {
          if (depth === 0) {
            const head = s.slice(headStart, i);
            const m = /\b([A-Za-z_]\w*)\s*\(([^()]|\([^()]*\))*\)\s*(const\s*)?$/.exec(head);
            if (m && !NOT_FUNCTIONS.has(m[1]) && !/^\s*(struct|class|enum|union|namespace)\b/.test(head) && head.replace(/\([^()]*\)/g, "").indexOf("=") < 0) {
              let d = 0;
              let j = i;
              for (; j < s.length; j++) {
                if (s[j] === "{") d++;
                else if (s[j] === "}" && --d === 0) break;
              }
              const nameAt = headStart + m.index;
              const line = s.slice(0, nameAt).split("\n").length;
              seen[m[1]] = (seen[m[1]] || 0) + 1;
              const key = seen[m[1]] === 1 ? m[1] : m[1] + "#" + seen[m[1]];
              out.push({ key, name: m[1], file: f.name, line, body: s.slice(i, j + 1) });
            }
          }
          depth++;
        } else if (c === "}") {
          depth = Math.max(0, depth - 1);
          if (depth === 0) headStart = i + 1;
        }
      }
    });
    return out;
  }

  /** Drops #if <NAME> … #endif blocks (nested #if counted) for the given define names. */
  function dropIfBlocks(lines, names) {
    const re = new RegExp("^\\s*#\\s*if(def)?\\s+(" + names.join("|") + ")\\b");
    const out = [];
    for (let i = 0; i < lines.length; i++) {
      if (!re.test(lines[i])) {
        out.push(lines[i]);
        continue;
      }
      let d = 0;
      for (; i < lines.length; i++) {
        if (/^\s*#\s*if/.test(lines[i])) d++;
        else if (/^\s*#\s*endif/.test(lines[i]) && --d === 0) break;
      }
    }
    return out;
  }

  /**
   * A function body reduced to what it does: no comments, no spacing, no preprocessor lines,
   * and without the known additions (ROS2D2 and PC forwarding blocks, pcFwd… / ros… calls),
   * so a sketch that only has those still matches the stock one.
   */
  function normalizeBody(body) {
    let lines = stripComments(String(body).replace(/\r\n?/g, "\n")).split("\n");
    lines = dropIfBlocks(lines, ["ENABLE_ROS2D2", "ENABLE_PC_TO_MARCDUINO_FORWARDING"]);
    lines = lines.filter((l) => !/^\s*#/.test(l) && !/\b(pcFwd|ros)[A-Z]\w*\s*\(/.test(l));
    return lines.join("").replace(/\s+/g, "");
  }

  /** 32-bit FNV-1a of the text, as 8 hex characters. */
  function fnv1a(text) {
    let h = 0x811c9dc5;
    const s = String(text);
    for (let i = 0; i < s.length; i++) {
      h ^= s.charCodeAt(i);
      h = Math.imul(h, 0x01000193) >>> 0;
    }
    return h.toString(16).padStart(8, "0");
  }

  const api = { stripComments, splitFunctions, normalizeBody, fnv1a };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.SketchText = api;
})(this);
