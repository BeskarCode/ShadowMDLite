"use strict";
/** Fixtures and edits for the migrator tests (not a test file itself). */
const fs = require("node:fs");
const path = require("node:path");

const FIX = path.join(__dirname, "fixtures");

function readDir(name) {
  const dir = path.join(FIX, name);
  return fs
    .readdirSync(dir)
    .filter((f) => /\.(ino|h)$/.test(f))
    .sort()
    .map((f) => ({ name: f, text: fs.readFileSync(path.join(dir, f), "utf8") }));
}

const stockFiles = () => readDir("classic-stock");
const liteFiles = () => [{ name: "ShadowMDLite.ino", text: fs.readFileSync(path.join(FIX, "lite.ino"), "utf8") }];

/** Applies [regex, replacement] pairs; every regex must match exactly once in some file. */
function withEdits(files, edits) {
  const out = files.map((f) => ({ name: f.name, text: f.text }));
  for (const [re, rep] of edits) {
    const hits = out.filter((f) => re.test(f.text));
    if (hits.length !== 1) throw new Error("edit " + re + " matches " + hits.length + " files");
    const f = hits[0];
    const n = (f.text.match(new RegExp(re.source, re.flags.includes("g") ? re.flags : re.flags + "g")) || []).length;
    if (n !== 1) throw new Error("edit " + re + " matches " + n + " times");
    f.text = f.text.replace(re, rep);
  }
  return out;
}

/** btnUP as a custom (type 2) button: MP3 202, custom panel sequence with dome panel 3. */
const CUSTOM_UP = [
  [/^int btnUP_type = 1;/m, "int btnUP_type = 2;"],
  [/^int btnUP_cust_MP3_num = 0;/m, "int btnUP_cust_MP3_num = 202;"],
  [/^int btnUP_cust_panel = 0;/m, "int btnUP_cust_panel = 9;"],
  [/^boolean btnUP_use_DP3 = false;/m, "boolean btnUP_use_DP3 = true;"],
  [/^int btnUP_DP3_open_start_delay = 1;/m, "int btnUP_DP3_open_start_delay = 2;"],
  [/^int btnUP_DP3_stay_open_time = 5;/m, "int btnUP_DP3_stay_open_time = 3;"],
];

module.exports = { readDir, stockFiles, liteFiles, withEdits, CUSTOM_UP };
