const test = require("node:test");
const assert = require("node:assert");
const C = require("../lib/droid-config.js");

function servo(id, board, channel) {
  return { id, name: id, kind: "servo", board, channel };
}

test("exampleConfig keeps today's ids and channels", () => {
  const z = C.toZones(C.exampleConfig());
  const s8 = z.find((x) => x.id === "s8");
  assert.equal(s8.mdPanel, "07");
  assert.equal(s8.target, "S1:");
  assert.equal(z.find((x) => x.id === "b1").target, "S3:");
  assert.deepEqual(z.find((x) => x.id === "holo_t").axes, []);
  assert.deepEqual(z.find((x) => x.id === "holo_f").axes, ["h", "v"]);
  assert.deepEqual(z.find((x) => x.id === "holo_r").axes, ["h", "v"]);
  assert.equal(z.length, 23);
  assert.equal(z.find((x) => x.id === "holo_r").holoId, "03");
  assert.equal(z.find((x) => x.id === "holo_f").maps.length, 2);
  assert.equal(z.find((x) => x.id === "s12").map, undefined);
  assert.equal(z.find((x) => x.id === "s0").label, "Panel 4 (Lower Panel)");
  assert.equal(z.find((x) => x.id === "s0").labelEs, "Panel 4 (panel inferior)");
});

test("exampleConfig is valid and empty config is empty", () => {
  assert.equal(C.validateConfig(C.exampleConfig()).ok, true);
  assert.deepEqual(C.emptyConfig(), { version: 1, markers: [] });
});

test("validateConfig rejects two servos on the same board and channel", () => {
  const cfg = C.exampleConfig();
  cfg.markers.push({ id: "m1", name: "X", kind: "servo", board: "S1:", channel: "07" });
  const r = C.validateConfig(cfg);
  assert.equal(r.ok, false);
  assert.ok(r.errors.some((e) => e.code === "duplicate-channel" && e.id === "m1" && e.other === "s8"));
});

test("same channel on different boards is fine", () => {
  const cfg = { version: 1, markers: [servo("a", "S1:", "07"), servo("b", "S3:", "07")] };
  assert.equal(C.validateConfig(cfg).ok, true);
});

test("channel accepts any two-digit number, rejects 00 and 3 digits", () => {
  for (const ch of ["13", "42"]) {
    assert.equal(C.validateConfig({ version: 1, markers: [servo("a", "S1:", ch)] }).ok, true, ch);
  }
  for (const ch of ["00", "100", "a1", "7", undefined]) {
    const r = C.validateConfig({ version: 1, markers: [servo("a", "S1:", ch)] });
    assert.equal(r.ok, false, String(ch));
    assert.ok(r.errors.some((e) => e.code === "bad-channel" && e.id === "a"));
  }
});

test("validateConfig flags missing name and bad kind", () => {
  const r = C.validateConfig({ version: 1, markers: [
    { id: "a", name: " ", kind: "servo", board: "S1:", channel: "01" },
    { id: "b", name: "B", kind: "wheel", board: "S1:", channel: "02" },
  ] });
  assert.ok(r.errors.some((e) => e.code === "missing-name" && e.id === "a"));
  assert.ok(r.errors.some((e) => e.code === "bad-kind" && e.id === "b"));
});

test("serialize -> parse round-trip keeps markers and pwmRanges", () => {
  const cfg = C.exampleConfig();
  cfg.markers.push({ id: "m1", name: "Camera", nameEs: "Cámara", kind: "servo", board: "S3:", channel: "42", pos3d: [0.1, 0.2, -0.3], part: "body" });
  const pwm = { "servo:7": { min: 1000, max: 2000 } };
  const out = C.parseConfig(C.serializeConfig(cfg, pwm));
  assert.deepEqual(out.config, cfg);
  assert.deepEqual(out.pwmRanges, pwm);
  assert.equal(JSON.parse(C.serializeConfig(cfg)).format, "shadowmd-droid-config");
  assert.equal(C.parseConfig(C.serializeConfig(cfg)).pwmRanges, null);
});

test("parseConfig rejects unknown version and invalid JSON", () => {
  assert.throws(() => C.parseConfig("{"), (e) => e.code === "bad-json");
  assert.throws(() => C.parseConfig('{"format":"shadowmd-droid-config","version":9,"markers":[]}'), (e) => e.code === "bad-version");
  assert.throws(() => C.parseConfig('{"foo":1}'), (e) => e.code === "bad-format");
});

test("validateConfig flags a duplicate that arrives by import", () => {
  const text = JSON.stringify({ format: "shadowmd-droid-config", version: 1, markers: [servo("a", "S3:", "04"), servo("b", "S3:", "04")] });
  assert.throws(() => C.parseConfig(text), (e) => e.code === "invalid" && e.errors.some((x) => x.code === "duplicate-channel"));
});

test("loadConfig returns null when nothing saved or storage throws", () => {
  assert.equal(C.loadConfig({ getItem: () => null }), null);
  assert.equal(C.loadConfig({ getItem: () => { throw new Error("denied"); } }), null);
  assert.equal(C.loadConfig({ getItem: () => "{not json" }), null);
  assert.doesNotThrow(() => C.saveConfig({ setItem: () => { throw new Error("full"); } }, C.emptyConfig()));
});

test("saveConfig then loadConfig round-trips under shadowmd_droid_config_v1", () => {
  const mem = {};
  const st = { getItem: (k) => (k in mem ? mem[k] : null), setItem: (k, v) => { mem[k] = v; } };
  const cfg = C.exampleConfig();
  C.saveConfig(st, cfg);
  assert.ok("shadowmd_droid_config_v1" in mem);
  assert.deepEqual(C.loadConfig(st), cfg);
});

test("toZones carries axes, pos3d, part and maps", () => {
  const cfg = { version: 1, markers: [
    { id: "a", name: "A", nameEs: "Á", kind: "servo", board: "S1:", channel: "13", pos3d: [1, 2, 3], part: "dome", map2d: [{ left: 1, top: 2, w: 3, h: 4 }] },
  ] };
  const [z] = C.toZones(cfg);
  assert.equal(z.mdPanel, "13");
  assert.deepEqual(z.pos3d, [1, 2, 3]);
  assert.equal(z.part, "dome");
  assert.deepEqual(z.map, { left: 1, top: 2, w: 3, h: 4 });
  assert.equal(z.labelEs, "Á");
});

test("newMarkerId never repeats", () => {
  const cfg = { version: 1, markers: [servo("m1", "S1:", "01"), servo("m2", "S1:", "02")] };
  assert.equal(C.newMarkerId(cfg, "servo"), "m3");
  assert.equal(C.newMarkerId(C.emptyConfig(), "holo"), "m1");
});
