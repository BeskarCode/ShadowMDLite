/**
 * Adds PC forwarding (USB S1:/S3: lines -> MarcDuinos) to a pasted classic Shadow MD sketch,
 * the same way the classic repo does it: PcForwarding.h, its #define and #include, the
 * pcFwdReadUsb() call in loop() and, with ROS2D2, the pcFwdHandleLine() hook in
 * rosHandleLine(). It only adds lines (or turns an existing define from 0 to 1) and never
 * guesses: an anchor that is missing or not unique gives "anchor-not-found".
 * Plain script (no modules) so index.html still opens from file://. Needs lib/ino-detect.js.
 */
(function (root) {
  "use strict";

  const InoDetect =
    typeof module !== "undefined" && module.exports ? require("./ino-detect.js") : root.InoDetect;

  // Exact copy of Shadow_MD_Eebel_Tabbed/PcForwarding.h in the classic repo (a test checks it).
  const PC_FORWARDING_H = [
    "// =======================================================================================",
    "//   PC forwarding: lines from the USB serial port go to the MarcDuinos",
    "// =======================================================================================",
    "// Used by the serial web console (see docs/web-console.md). Turn it on with",
    "// #define ENABLE_PC_TO_MARCDUINO_FORWARDING 1 in a_INIT.ino; with 0 this file adds nothing.",
    "//",
    "// One command per line (115200 baud, ended by \\n, \\r or \\r\\n):",
    "//   S1:<command>   sends <command> to the dome MarcDuino (Serial1), e.g. S1::OP01",
    "//   S3:<command>   sends <command> to the body MarcDuino (Serial3)",
    "//   PING           answers PONG",
    "//   HELP           lists these commands",
    "// A \\r is added to the command when it does not end with one (MarcDuino syntax).",
    "// Same lines and answers as ShadowMD Lite.",
    "#ifndef PC_FORWARDING_H",
    "#define PC_FORWARDING_H",
    "",
    "#if ENABLE_PC_TO_MARCDUINO_FORWARDING",
    "",
    "#define PC_FWD_LINE_SIZE 64            // USB line buffer, including the terminating NUL",
    "",
    "// One line from the USB serial port, without the line ending. true when the line was",
    "// a forwarding command (S1:, S3:, PING, HELP) and has been answered; false when it is",
    "// not one, so the caller can try its own commands.",
    "bool pcFwdHandleLine(const char* line)",
    "{",
    "  // Skip leading and trailing spaces/tabs.",
    "  while (*line == ' ' || *line == '\\t') line++;",
    "  size_t len = strlen(line);",
    "  while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\\t')) len--;",
    "  if (len == 0) return false;",
    "",
    "  if ((len == 4 && strncmp(line, \"PING\", 4) == 0) || (len == 4 && strncmp(line, \"ping\", 4) == 0))",
    "  {",
    "    Serial.println(F(\"PONG\"));",
    "    return true;",
    "  }",
    "",
    "  if ((len == 4 && strncmp(line, \"HELP\", 4) == 0) || (len == 4 && strncmp(line, \"help\", 4) == 0))",
    "  {",
    "    Serial.println(F(\"Shadow MD PC Serial forwarding ON\"));",
    "    Serial.println(F(\"Use: S1:<COMMAND> or S3:<COMMAND>\"));",
    "    Serial.println(F(\"Example: S1::SE01\"));",
    "    return true;",
    "  }",
    "",
    "  // S1:<command> or S3:<command>. The command may already begin with ':' (MarcDuino syntax).",
    "  if (len < 3 || !(line[0] == 'S' || line[0] == 's') || !(line[1] == '1' || line[1] == '3') || line[2] != ':')",
    "    return false;",
    "",
    "  int targetPort = (line[1] == '1') ? 1 : 3;",
    "  const char* payload = line + 3;",
    "  const char* end = line + len;",
    "  while (payload < end && (*payload == ' ' || *payload == '\\t')) payload++;",
    "  size_t n = end - payload;",
    "  if (n == 0)",
    "  {",
    "    Serial.println(F(\"ERR Empty command payload\"));",
    "    return true;",
    "  }",
    "",
    "  // Copy the command and make sure it ends with the carriage return MarcDuino expects.",
    "  char cmd[PC_FWD_LINE_SIZE + 1];",
    "  if (n > PC_FWD_LINE_SIZE - 1)",
    "  {",
    "    Serial.println(F(\"ERR Command too long\"));",
    "    return true;",
    "  }",
    "  memcpy(cmd, payload, n);",
    "  if (cmd[n - 1] != '\\r') cmd[n++] = '\\r';",
    "  cmd[n] = '\\0';",
    "",
    "  if (targetPort == 1)",
    "    Serial1.print(cmd);",
    "  else",
    "    Serial3.print(cmd);",
    "",
    "  Serial.print(F(\"OK forwarded to \"));",
    "  Serial.println(targetPort == 1 ? F(\"Serial1\") : F(\"Serial3\"));",
    "  return true;",
    "}",
    "",
    "char    pcFwdLine[PC_FWD_LINE_SIZE];   // USB line being received",
    "uint8_t pcFwdLineLen = 0;",
    "bool    pcFwdLineOverflow = false;     // line longer than the buffer: discard it",
    "bool    pcFwdLineBadByte = false;      // NUL or non-ASCII byte in the line: reject it",
    "",
    "// Reads the USB serial port without blocking and hands each complete line (ended by \\n or",
    "// \\r, so \\r\\n works too) to pcFwdHandleLine(). A line that does not fit in",
    "// PC_FWD_LINE_SIZE - 1 characters is discarded up to its end and answered with",
    "// \"ERR line too long\"; a line with a NUL or non-ASCII byte, or one that is not a",
    "// forwarding command, is answered with \"ERR unknown command\".",
    "// Not used with ENABLE_ROS2D2 1: then rosReadUsb() reads the port and rosHandleLine()",
    "// calls pcFwdHandleLine() first.",
    "void pcFwdReadUsb()",
    "{",
    "  while (Serial.available() > 0)",
    "  {",
    "    char c = (char)Serial.read();",
    "",
    "    if (c == '\\n' || c == '\\r')",
    "    {",
    "      if (pcFwdLineOverflow)",
    "      {",
    "        Serial.println(F(\"ERR line too long\"));",
    "      } else if (pcFwdLineBadByte)",
    "      {",
    "        Serial.println(F(\"ERR unknown command\"));",
    "      } else if (pcFwdLineLen > 0)",
    "      {",
    "        pcFwdLine[pcFwdLineLen] = '\\0';",
    "        if (!pcFwdHandleLine(pcFwdLine))",
    "        {",
    "          // A line of only spaces is ignored, anything else is not ours.",
    "          const char* p = pcFwdLine;",
    "          while (*p == ' ' || *p == '\\t') p++;",
    "          if (*p != '\\0') Serial.println(F(\"ERR unknown command\"));",
    "        }",
    "      }",
    "      pcFwdLineLen = 0;",
    "      pcFwdLineOverflow = false;",
    "      pcFwdLineBadByte = false;",
    "      continue;",
    "    }",
    "",
    "    if (pcFwdLineOverflow) continue;  // drop the rest of a line that is too long",
    "    if (c == '\\0' || (uint8_t)c >= 0x80) pcFwdLineBadByte = true;",
    "    if (pcFwdLineLen < PC_FWD_LINE_SIZE - 1)",
    "    {",
    "      pcFwdLine[pcFwdLineLen++] = c;",
    "    } else",
    "    {",
    "      pcFwdLineOverflow = true;",
    "    }",
    "  }",
    "}",
    "",
    "#endif // ENABLE_PC_TO_MARCDUINO_FORWARDING",
    "#endif // PC_FORWARDING_H",
    "",
  ].join("\n");

  const DEFINE_LINES = [
    "// Serial web console: S1:/S3: lines over USB go to the MarcDuinos (see PcForwarding.h). 0 = off.",
    "#define ENABLE_PC_TO_MARCDUINO_FORWARDING 1",
    '#include "PcForwarding.h"',
  ];

  const RE_FWD_DEFINE = /^[ \t]*#[ \t]*define[ \t]+ENABLE_PC_TO_MARCDUINO_FORWARDING\b/;
  const RE_FWD_INCLUDE = /^[ \t]*#[ \t]*include[ \t]+"PcForwarding\.h"/;
  const RE_ROS_DEFINE = /^[ \t]*#[ \t]*define[ \t]+ENABLE_ROS2D2\b/;
  const RE_SHADOW_DEFINE = /^[ \t]*(?:\/\/[ \t]*)?#[ \t]*define[ \t]+SHADOW_(DEBUG|VERBOSE)\b/;
  const RE_FOOT_DRIVE = /^[ \t]*footMotorDrive[ \t]*\([ \t]*\)[ \t]*;/;

  function eolOf(text) {
    return /\r\n/.test(text) ? "\r\n" : "\n";
  }

  /** Index just past the "}" that closes the "{" at open, or -1. */
  function closeBrace(code, open) {
    let depth = 0;
    for (let i = open; i < code.length; i++) {
      if (code[i] === "{") depth++;
      else if (code[i] === "}" && --depth === 0) return i + 1;
    }
    return -1;
  }

  function lineAt(code, offset) {
    let n = 0;
    for (let i = 0; i < offset; i++) if (code[i] === "\n") n++;
    return n;
  }

  function notFound(where) {
    return { ok: false, reason: "anchor-not-found", where };
  }

  function patchClassic(files) {
    const list = (Array.isArray(files) ? files : []).filter(
      (f) => f && typeof f.name === "string" && typeof f.text === "string",
    );
    const det = InoDetect.detectSketch(list);
    if (det.variant !== "classic") return { ok: false, reason: "not-classic" };
    if (det.forwarding === true) return { ok: false, reason: "already-patched" };

    // Raw lines and the same lines without comments (stripComments keeps the line breaks).
    const docs = list
      .filter((f) => f.name !== "PcForwarding.h")
      .map((f) => {
        const raw = f.text.split(/\r?\n/);
        const codeText = InoDetect.stripComments(f.text).replace(/\r/g, "");
        let code = codeText.split("\n");
        if (code.length !== raw.length) code = raw.slice();
        return { name: f.name, eol: eolOf(f.text), raw, code, codeText: code.join("\n"), inserts: [], changed: false };
      });
    const pick = (re) => docs.find((d) => re.test(d.name));
    const fallback = (re, name) => (pick(re) || (docs.length === 1 ? docs[0] : null) || { name }).name;
    const linesMatching = (re, field) => {
      const out = [];
      for (const d of docs) d[field].forEach((l, i) => { if (re.test(l)) out.push({ d, i }); });
      return out;
    };

    // 1. #define ENABLE_PC_TO_MARCDUINO_FORWARDING 1 and #include "PcForwarding.h"
    const initWhere = fallback(/a_INIT/i, "a_INIT.ino");
    const hasInclude = linesMatching(RE_FWD_INCLUDE, "code").length > 0;
    const defs = linesMatching(RE_FWD_DEFINE, "code");
    if (defs.length > 1) return notFound(defs[0].d.name);
    if (defs.length === 1) {
      const { d, i } = defs[0];
      const line = d.raw[i].replace(/(ENABLE_PC_TO_MARCDUINO_FORWARDING[ \t]+)\S+/, (m, p) => p + "1");
      if (line !== d.raw[i]) { d.raw[i] = line; d.changed = true; }
      if (!/ENABLE_PC_TO_MARCDUINO_FORWARDING[ \t]+1\b/.test(d.raw[i])) return notFound(d.name);
      if (!hasInclude) d.inserts.push({ at: i + 1, lines: [DEFINE_LINES[2]] });
    } else {
      let anchor = null;
      const ros = linesMatching(RE_ROS_DEFINE, "code");
      if (ros.length > 1) return notFound(ros[0].d.name);
      if (ros.length === 1) anchor = ros[0];
      else {
        const sh = linesMatching(RE_SHADOW_DEFINE, "raw");
        const kinds = sh.map(({ d, i }) => RE_SHADOW_DEFINE.exec(d.raw[i])[1]);
        const unique = new Set(kinds).size === kinds.length;
        const oneFile = sh.every((s) => s.d === (sh[0] && sh[0].d));
        if (sh.length === 0 || !unique || !oneFile) return notFound(sh.length ? sh[0].d.name : initWhere);
        anchor = sh[sh.length - 1];
      }
      anchor.d.inserts.push({ at: anchor.i + 1, lines: hasInclude ? DEFINE_LINES.slice(0, 2) : DEFINE_LINES });
    }

    // 2. pcFwdReadUsb(); in loop(), right before footMotorDrive();
    const loops = [];
    for (const d of docs) {
      const re = /\bvoid\s+loop\s*\(\s*(?:void)?\s*\)\s*\{/g;
      let m;
      while ((m = re.exec(d.codeText)) !== null) loops.push({ d, open: m.index + m[0].length - 1 });
    }
    const mainWhere = fallback(/c_MAIN/i, "c_MAIN.ino");
    if (loops.length !== 1) return notFound(loops.length ? loops[0].d.name : mainWhere);
    {
      const { d, open } = loops[0];
      const end = closeBrace(d.codeText, open);
      if (end < 0) return notFound(d.name);
      const body = d.codeText.slice(open, end);
      if (!/\bpcFwdReadUsb\s*\(\s*\)\s*;/.test(body)) {
        const first = lineAt(d.codeText, open);
        const last = lineAt(d.codeText, end);
        const hits = [];
        for (let i = first; i <= last; i++) if (RE_FOOT_DRIVE.test(d.code[i])) hits.push(i);
        if (hits.length !== 1) return notFound(d.name);
        const indent = /^[ \t]*/.exec(d.raw[hits[0]])[0];
        d.inserts.push({
          at: hits[0],
          lines: ["#if ENABLE_PC_TO_MARCDUINO_FORWARDING && !ENABLE_ROS2D2", indent + "pcFwdReadUsb();", "#endif"],
        });
      }
    }

    // 3. ROS2D2 sketches: rosHandleLine() hands S1:/S3:/PING/HELP to pcFwdHandleLine() first
    const handlers = [];
    for (const d of docs) {
      const re = /\bvoid\s+rosHandleLine\s*\(\s*(?:const\s+)?char\s*\*\s*(\w+)\s*\)\s*\{/g;
      let m;
      while ((m = re.exec(d.codeText)) !== null) handlers.push({ d, open: m.index + m[0].length - 1, arg: m[1] });
    }
    if (handlers.length > 1) return notFound(handlers[0].d.name);
    if (handlers.length === 1) {
      const { d, open, arg } = handlers[0];
      const end = closeBrace(d.codeText, open);
      if (end < 0) return notFound(d.name);
      if (!/\bpcFwdHandleLine\b/.test(d.codeText.slice(open, end))) {
        const i = lineAt(d.codeText, open);
        if (!/\{[ \t]*$/.test(d.code[i])) return notFound(d.name);
        d.inserts.push({
          at: i + 1,
          lines: [
            "#if ENABLE_PC_TO_MARCDUINO_FORWARDING",
            "  if (pcFwdHandleLine(" + arg + ")) return;   // S1:, S3:, PING, HELP (PcForwarding.h)",
            "#endif",
          ],
        });
      }
    }

    // Apply, bottom-up so the line numbers stay valid.
    const out = new Map();
    for (const d of docs) {
      d.inserts.sort((a, b) => b.at - a.at);
      for (const ins of d.inserts) d.raw.splice(ins.at, 0, ...ins.lines);
      if (d.inserts.length) d.changed = true;
      if (d.changed) out.set(d.name, d.raw.join(d.eol));
    }
    const changed = [...out.keys()];
    const files2 = list.map((f) => (out.has(f.name) ? { name: f.name, text: out.get(f.name) } : { name: f.name, text: f.text }));
    const h = files2.find((f) => f.name === "PcForwarding.h");
    if (!h) {
      files2.push({ name: "PcForwarding.h", text: PC_FORWARDING_H });
      changed.push("PcForwarding.h");
    } else if (h.text !== PC_FORWARDING_H) {
      h.text = PC_FORWARDING_H;
      changed.push("PcForwarding.h");
    }
    return { ok: true, files: files2, changed };
  }

  const api = { PC_FORWARDING_H, patchClassic };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.InoPatch = api;
})(this);
