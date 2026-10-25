/*
 * R2-D2 in 3D for the serial console: replaces the flat blueprint.
 *
 * - The zones of the blueprint (dome panels, holos, body panels) are numbered markers on
 *   the model. Click one to select it, exactly like the blueprint hotspots.
 * - The selected zone glows and shows its name.
 * - The dome turns with the dome speed commands sent (DM:<speed>, ROS:DOME:<speed>).
 * - "Move marker": click on the model to put the selected zone's marker there (saved in
 *   this browser; "Export" copies them all to paste into ZONE_3D below).
 *
 * Plain script (no modules) on three.js r147: the console is opened as a local file, and
 * browsers refuse modules and separate model files there. The model is embedded in
 * vendor/r2d2-model.js.
 *
 * Talks to the console through window.R2Console (zones, selectZone, lang) and offers
 * window.R2View3D (setSelected, onCommand).
 */
(function () {
  "use strict";
  const HEIGHT = 3.8;                 // model height in scene units
  const MAX_SPEED = 80;               // ShadowMDLite clamps dome speeds to this
  const DEG_PER_S_AT_80 = 90;         // dome turn speed at 80 (measure it on your droid)
  const STALE_MS = 300;               // the Mega stops the dome after 300 ms without a speed
  const STORE = "shadowmd_r2_3d_markers";

  // Where each zone is on the droid, taken from the blueprint (yaw: degrees from the front,
  // + = R2's right; dome: polar = degrees from the top; body: h = height, 0 feet .. 1 top).
  // Markers are then dropped onto the model's surface. Fine-tune with "Move marker".
  const ZONE_3D = {
    s0: { part: "dome", yaw: -85, polar: 70 }, s1: { part: "dome", yaw: -71, polar: 70 },
    s2: { part: "dome", yaw: -57, polar: 70 }, s3: { part: "dome", yaw: -44, polar: 70 },
    s4: { part: "dome", yaw: 53, polar: 70 },  s5: { part: "dome", yaw: 77, polar: 70 },
    s8: { part: "dome", yaw: 105, polar: 41 }, s9: { part: "dome", yaw: 47, polar: 36 },
    s10: { part: "dome", yaw: -39, polar: 34 }, s11: { part: "dome", yaw: -96, polar: 39 },
    s12: { part: "dome", yaw: 0, polar: 2 },
    holo_f: { part: "dome", yaw: -28, polar: 56 }, holo_t: { part: "dome", yaw: -160, polar: 25 },
    holo_r: { part: "dome", yaw: 172, polar: 66 },
    b1: { part: "body", yaw: -30, h: 0.46 }, b2: { part: "body", yaw: 0, h: 0.66 },
    b3: { part: "body", yaw: 14, h: 0.6 },   b4: { part: "body", yaw: 62, h: 0.5 },
    b5: { part: "body", yaw: 61, h: 0.57 },  b6: { part: "body", yaw: 60, h: 0.63 },
    b7: { part: "body", yaw: -50, h: 0.49 }, b8: { part: "body", yaw: -54, h: 0.55 },
    b9: { part: "body", yaw: -56, h: 0.63 },
  };
  // Dome cut of this model, placed by eye in ROS2D2's web console (?tune=1)
  const CUT = { domeFrom: 0.748, pitch: -15.6, roll: 0 };
  const COLOURS = { dome: "#d9603b", holo: "#4a7fc1", body: "#5fa58f" };

  const TXT = {
    en: { move: "Move marker", moving: "Click on R2 to place it", export: "Export", copied: "Copied to the clipboard",
          front: "Dome to front", reset: "Reset markers", noGl: "3D not available in this browser: showing the blueprint." },
    es: { move: "Mover punto", moving: "Haz clic en R2 para colocarlo", export: "Exportar", copied: "Copiado al portapapeles",
          front: "Dome al frente", reset: "Restaurar puntos", noGl: "3D no disponible en este navegador: se muestra el plano." },
  };
  const t = (k) => (TXT[(window.R2Console && window.R2Console.lang()) || "en"] || TXT.en)[k];

  let renderer, scene, camera, controls, root, dome, markers = [], selected = -1, moving = false;
  let angle = 0, speed = 0, speedAt = 0;

  // ------------------------------------------------------------------ model and dome cut

  function b64ToBuffer(b64) {
    const bin = atob(b64);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
    return out.buffer;
  }

  const GET = ["getX", "getY", "getZ", "getW"];
  function toFloatTriangles(src) {
    const out = new THREE.BufferGeometry();
    const index = src.index;
    const count = index ? index.count : src.attributes.position.count;
    for (const [name, a] of Object.entries(src.attributes)) {
      if (!["position", "normal", "uv", "color"].includes(name)) continue;
      const arr = new Float32Array(count * a.itemSize);
      for (let i = 0; i < count; i++) {
        const v = index ? index.getX(i) : i;
        for (let c = 0; c < a.itemSize; c++) arr[i * a.itemSize + c] = a[GET[c]](v);
      }
      out.setAttribute(name, new THREE.BufferAttribute(arr, a.itemSize));
    }
    return out;
  }

  // Split at a plane; triangles crossing it are clipped, so the cut is a clean line
  function cutByPlane(geo, origin, normal) {
    const names = Object.keys(geo.attributes);
    const attrs = names.map((n) => geo.attributes[n]);
    const pi = names.indexOf("position");
    const out = [names.map(() => []), names.map(() => [])];
    const vertex = (i) => attrs.map((a) => Array.from(a.array.subarray(i * a.itemSize, (i + 1) * a.itemSize)));
    const lerp = (va, vb, k) => va.map((arr, n) => arr.map((x, c) => x + (vb[n][c] - x) * k));
    const emit = (side, tri) => tri.forEach((v) => v.forEach((arr, n) => { for (const x of arr) out[side][n].push(x); }));
    const count = geo.attributes.position.count;
    for (let i = 0; i < count; i += 3) {
      const v = [vertex(i), vertex(i + 1), vertex(i + 2)];
      const d = v.map((vv) => (vv[pi][0] - origin.x) * normal.x + (vv[pi][1] - origin.y) * normal.y + (vv[pi][2] - origin.z) * normal.z);
      const up = d.map((x) => x > 0);
      const n = up.filter(Boolean).length;
      if (n === 0 || n === 3) { emit(n ? 1 : 0, v); continue; }
      const lone = up.findIndex((a) => (n === 1 ? a : !a));
      const a = v[lone], b = v[(lone + 1) % 3], c = v[(lone + 2) % 3];
      const da = d[lone], db = d[(lone + 1) % 3], dc = d[(lone + 2) % 3];
      const ab = lerp(a, b, da / (da - db)), ac = lerp(a, c, da / (da - dc));
      const side = n === 1 ? 1 : 0;
      emit(side, [a, ab, ac]);
      emit(1 - side, [ab, b, c]);
      emit(1 - side, [ab, c, ac]);
    }
    return out.map((lists) => {
      if (!lists[0].length) return null;
      const g = new THREE.BufferGeometry();
      names.forEach((nm, k) => g.setAttribute(nm, new THREE.BufferAttribute(new Float32Array(lists[k]), attrs[k].itemSize)));
      g.computeBoundingSphere();
      return g;
    });
  }

  function fitCircle(pts) {
    if (pts.length < 8) return null;
    let n = 0, sx = 0, sz = 0, sxx = 0, szz = 0, sxz = 0, sxr = 0, szr = 0, sr = 0;
    for (const [x, z] of pts) {
      const r = x * x + z * z;
      n++; sx += x; sz += z; sxx += x * x; szz += z * z; sxz += x * z; sxr += x * r; szr += z * r; sr += r;
    }
    const A = new THREE.Matrix3().set(2 * sxx, 2 * sxz, sx, 2 * sxz, 2 * szz, sz, 2 * sx, 2 * sz, n);
    if (Math.abs(A.determinant()) < 1e-12) return null;
    const s = new THREE.Vector3(sxr, szr, sr).applyMatrix3(A.invert());
    return { x: s.x, z: s.y, r: Math.sqrt(s.z + s.x * s.x + s.y * s.y) };
  }
  function robustCircle(pts) {
    let fit = fitCircle(pts);
    for (let k = 0; k < 2 && fit; k++) {
      const near = pts.filter(([a, b]) => Math.abs(Math.hypot(a - fit.x, b - fit.z) - fit.r) < fit.r * 0.12);
      fit = fitCircle(near) || fit;
    }
    return fit;
  }

  function buildDroid(gltfScene) {
    gltfScene.updateMatrixWorld(true);
    const box = new THREE.Box3().setFromObject(gltfScene);
    const size = box.getSize(new THREE.Vector3());
    const k = HEIGHT / size.y;
    const norm = new THREE.Matrix4().makeScale(k, k, k).multiply(new THREE.Matrix4().makeTranslation(
      -(box.min.x + box.max.x) / 2, -box.min.y, -(box.min.z + box.max.z) / 2));
    const meshes = [];
    gltfScene.traverse((o) => {
      if (!o.isMesh) return;
      const g = toFloatTriangles(o.geometry);
      g.applyMatrix4(new THREE.Matrix4().multiplyMatrices(norm, o.matrixWorld));
      meshes.push([o.material, g]);
    });
    const cutY = HEIGHT * CUT.domeFrom;
    // Body axis: circle fitted just below the dome
    const pts = [];
    for (const [, g] of meshes) {
      const p = g.attributes.position;
      for (let i = 0; i < p.count; i++) {
        const y = p.getY(i);
        if (y > cutY - HEIGHT * 0.12 && y < cutY - HEIGHT * 0.02) pts.push([p.getX(i), p.getZ(i)]);
      }
    }
    const axis = robustCircle(pts) || { x: 0, z: 0 };
    const origin = new THREE.Vector3(axis.x, cutY, axis.z);
    const deg = THREE.MathUtils.degToRad;
    const normal = new THREE.Vector3(Math.sin(deg(CUT.roll)), 1, Math.sin(deg(CUT.pitch))).normalize();

    const body = [], top = [];
    for (const [mat, g] of meshes) {
      const [lo, hi] = cutByPlane(g, origin, normal);
      if (lo) body.push(new THREE.Mesh(lo, mat));
      if (hi) top.push(new THREE.Mesh(hi, mat));
    }
    // Spin about the centre of the dome's own base (the outline left on the plane)
    const u = new THREE.Vector3(1, 0, 0).projectOnPlane(normal).normalize();
    const w = new THREE.Vector3().crossVectors(normal, u);
    const onPlane = [], v = new THREE.Vector3();
    for (const m of top) {
      const p = m.geometry.attributes.position;
      for (let i = 0; i < p.count; i++) {
        v.fromBufferAttribute(p, i).sub(origin);
        if (Math.abs(v.dot(normal)) < HEIGHT * 1e-4) onPlane.push([v.dot(u), v.dot(w)]);
      }
    }
    const c = robustCircle(onPlane);
    const centre = c ? origin.clone().addScaledVector(u, c.x).addScaledVector(w, c.z) : origin;

    const r = new THREE.Group();
    body.forEach((m) => r.add(m));
    const tilt = new THREE.Group();
    tilt.position.copy(centre);
    tilt.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), normal);
    const d = new THREE.Group();
    tilt.add(d);
    const toLocal = new THREE.Matrix4().compose(centre, tilt.quaternion, new THREE.Vector3(1, 1, 1)).invert();
    top.forEach((m) => { m.geometry.applyMatrix4(toLocal); m.geometry.computeBoundingSphere(); d.add(m); });
    r.add(tilt);
    return { root: r, dome: d, body, top, axis };
  }

  // ------------------------------------------------------------------ markers

  function zoneNumber(z) {
    if (z.kind === "holo") return { holo_f: "F", holo_t: "T", holo_r: "R" }[z.id] || "H";
    if (z.id === "s12") return "C";
    const m = z.label.match(/(\d+)/);
    return m ? m[1] : "?";
  }
  function zoneColour(z) {
    if (z.kind === "holo") return COLOURS.holo;
    return z.id.startsWith("b") ? COLOURS.body : COLOURS.dome;
  }

  function badgeTexture(text, colour, big) {
    const s = 128;
    const cv = document.createElement("canvas");
    cv.width = cv.height = s;
    const g = cv.getContext("2d");
    g.beginPath();
    g.arc(s / 2, s / 2, s / 2 - 6, 0, Math.PI * 2);
    g.fillStyle = colour;
    g.fill();
    g.lineWidth = big ? 10 : 6;
    g.strokeStyle = big ? "#3fb950" : "#ffffff";
    g.stroke();
    g.fillStyle = "#ffffff";
    g.font = "bold 64px system-ui, sans-serif";
    g.textAlign = "center";
    g.textBaseline = "middle";
    g.fillText(text, s / 2, s / 2 + 3);
    const tex = new THREE.CanvasTexture(cv);
    tex.anisotropy = 4;
    return tex;
  }

  function labelSprite(text) {
    const cv = document.createElement("canvas");
    const g = cv.getContext("2d");
    g.font = "600 40px system-ui, sans-serif";
    const w = Math.ceil(g.measureText(text).width) + 40;
    cv.width = w; cv.height = 64;
    g.font = "600 40px system-ui, sans-serif";
    g.fillStyle = "rgba(22,27,34,0.92)";
    g.fillRect(0, 0, w, 64);
    g.strokeStyle = "#3fb950"; g.lineWidth = 4; g.strokeRect(2, 2, w - 4, 60);
    g.fillStyle = "#e6edf3"; g.textBaseline = "middle"; g.fillText(text, 20, 34);
    const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), depthTest: false }));
    sp.scale.set(0.15 * w / 64, 0.15, 1);       // about 4 % of R2's height
    sp.center.set(0.5, -0.9);
    sp.renderOrder = 11;
    return sp;
  }

  // Drop a marker onto the surface: a ray from outside, aimed at the dome centre or the body axis
  const ray = new THREE.Raycaster();
  function surfacePoint(spec, droid) {
    const deg = THREE.MathUtils.degToRad;
    // + yaw is R2's right; the front is +Z, so the right is -X
    const yaw = deg(spec.yaw);
    if (spec.part === "dome") {
      // In the dome's own frame (its base may lean), from its centre outwards
      const p = deg(spec.polar);
      const local = new THREE.Vector3(-Math.sin(yaw) * Math.sin(p), Math.cos(p), Math.cos(yaw) * Math.sin(p));
      const centre = droid.dome.getWorldPosition(new THREE.Vector3());
      const dir = local.transformDirection(droid.dome.matrixWorld);
      ray.set(centre.clone().addScaledVector(dir, 5), dir.clone().negate());
      const hit = ray.intersectObjects(droid.top, false)[0];
      return hit ? hit.point.addScaledVector(dir, 0.04) : centre.addScaledVector(dir, 1.1);
    }
    const y = spec.h * HEIGHT;
    const dir = new THREE.Vector3(-Math.sin(yaw), 0, Math.cos(yaw));
    const from = new THREE.Vector3(droid.axis.x, y, droid.axis.z).addScaledVector(dir, 5);
    ray.set(from, dir.clone().negate());
    const hit = ray.intersectObjects(droid.body, false)[0];
    return hit ? hit.point.addScaledVector(dir, 0.04) : from.addScaledVector(dir, -4);
  }

  function loadStored() { try { return JSON.parse(localStorage.getItem(STORE) || "{}"); } catch (e) { return {}; } }
  function saveStored(map) { try { localStorage.setItem(STORE, JSON.stringify(map)); } catch (e) { /* private window */ } }

  function placeMarkers(droid) {
    const zones = window.R2Console.zones;
    const stored = loadStored();
    droid.root.updateMatrixWorld(true);
    markers = zones.map((z, i) => {
      const spec = ZONE_3D[z.id] || { part: "body", yaw: 0, h: 0.5 };
      const parent = spec.part === "dome" ? droid.dome : droid.root;
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: badgeTexture(zoneNumber(z), zoneColour(z), false), depthTest: true }));
      sp.scale.set(0.2, 0.2, 1);
      sp.renderOrder = 10;
      sp.userData = { zone: i, spec };
      if (stored[z.id]) sp.position.fromArray(stored[z.id]);
      else {
        // surfacePoint works in the droid's frame; dome markers live in the turning dome
        const p = surfacePoint(spec, droid);
        sp.position.copy(spec.part === "dome" ? droid.dome.worldToLocal(p) : p);
      }
      parent.add(sp);
      return sp;
    });
  }

  function setSelected(i) {
    if (!markers.length) { selected = i; return; }
    const zones = window.R2Console.zones;
    markers.forEach((m, k) => {
      const z = zones[k];
      const on = k === i;
      m.material.map = badgeTexture(zoneNumber(z), zoneColour(z), on);
      m.material.needsUpdate = true;
      m.scale.setScalar(on ? 0.3 : 0.2);
      m.material.depthTest = !on;          // the selected one shows even behind the droid
      m.renderOrder = on ? 12 : 10;
      const old = m.getObjectByName("label");
      if (old) m.remove(old);
      if (on) {
        const lab = labelSprite(window.R2Console.label(z));
        lab.name = "label";
        lab.scale.multiplyScalar(1 / 0.3);   // undo the marker's own scale
        m.add(lab);
      }
    });
    selected = i;
  }

  // ------------------------------------------------------------------ dome speed

  function onCommand(line) {
    const m = String(line).match(/(?:^|[^A-Z])(?:DM|ROS:DOME):\s*(-?\d+)/);
    if (!m) return;
    speed = Math.max(-MAX_SPEED, Math.min(MAX_SPEED, parseInt(m[1], 10)));
    speedAt = performance.now();
  }

  // ------------------------------------------------------------------ scene

  function init() {
    const box = document.getElementById("r2view");
    const canvas = box.querySelector("canvas");
    try {
      renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
    } catch (e) {
      fallback();
      return;
    }
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.outputEncoding = THREE.sRGBEncoding;
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(32, 1, 0.1, 100);
    camera.position.set(4.5, 4.4, 6.8);
    scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new THREE.RoomEnvironment(), 0.04).texture;
    scene.add(new THREE.HemisphereLight(0xdde8ff, 0x202530, 0.9));
    const sun = new THREE.DirectionalLight(0xffffff, 1.6);
    sun.position.set(4, 8, 6);
    scene.add(sun);
    const floor = new THREE.Mesh(new THREE.CircleGeometry(2.6, 64), new THREE.MeshBasicMaterial({ color: 0x161b22 }));
    floor.rotation.x = -Math.PI / 2;
    scene.add(floor);

    controls = new THREE.OrbitControls(camera, canvas);
    controls.target.set(0, 2.0, 0);
    controls.enablePan = false;
    controls.minDistance = 3;
    controls.maxDistance = 14;
    controls.update();

    new THREE.GLTFLoader().parse(b64ToBuffer(window.R2D2_MODEL_GLB_BASE64), "", (gltf) => {
      const droid = buildDroid(gltf.scene);
      root = droid.root;
      dome = droid.dome;
      scene.add(root);
      placeMarkers(droid);
      setSelected(selected >= 0 ? selected : window.R2Console.selected());
      window.__r2droid = droid;
    }, (err) => { console.error("r2view3d", err); fallback(); });

    canvas.addEventListener("pointerdown", (e) => { canvas._down = [e.clientX, e.clientY]; });
    canvas.addEventListener("pointerup", (e) => {
      const d = canvas._down;
      if (!d || Math.hypot(e.clientX - d[0], e.clientY - d[1]) > 5) return;   // that was a drag
      click(e);
    });
    buildTools(box);
    requestAnimationFrame(frame);
  }

  function pick(e, objects) {
    const r = renderer.domElement.getBoundingClientRect();
    const p = new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    ray.setFromCamera(p, camera);
    return ray.intersectObjects(objects, false)[0];
  }

  function click(e) {
    const droid = window.__r2droid;
    if (!droid) return;
    if (moving && selected >= 0) {
      const hit = pick(e, droid.body.concat(droid.top));
      if (!hit) return;
      const m = markers[selected];
      const n = hit.face ? hit.face.normal.clone().transformDirection(hit.object.matrixWorld) : new THREE.Vector3();
      const p = hit.point.addScaledVector(n, 0.04);
      m.position.copy(m.userData.spec.part === "dome" ? droid.dome.worldToLocal(p) : root.worldToLocal(p));
      const stored = loadStored();
      stored[window.R2Console.zones[selected].id] = m.position.toArray().map((x) => +x.toFixed(4));
      saveStored(stored);
      setMoving(false);
      return;
    }
    const hit = pick(e, markers);
    if (hit) window.R2Console.selectZone(hit.object.userData.zone);
  }

  function setMoving(on) {
    moving = on;
    const b = document.getElementById("r2move");
    b.classList.toggle("on", on);
    b.textContent = on ? t("moving") : t("move");
  }

  function buildTools(box) {
    const bar = document.createElement("div");
    bar.className = "r2view-tools";
    bar.innerHTML = '<button type="button" id="r2front"></button><button type="button" id="r2move"></button>' +
                    '<button type="button" id="r2export"></button><button type="button" id="r2reset"></button>';
    box.appendChild(bar);
    const credit = document.createElement("div");
    credit.className = "r2view-credit";
    credit.innerHTML = 'Model: "R2-D2" (<a href="https://sketchfab.com/3d-models/r2-d2-0f9760c4e566442ca80e2fcf7e2faa0d" target="_blank" rel="noopener">Sketchfab</a>) by ivan.vladimirov08, <a href="http://creativecommons.org/licenses/by/4.0/" target="_blank" rel="noopener">CC BY 4.0</a>';
    box.appendChild(credit);
    const texts = () => {
      document.getElementById("r2front").textContent = t("front");
      document.getElementById("r2export").textContent = t("export");
      document.getElementById("r2reset").textContent = t("reset");
      setMoving(moving);
    };
    texts();
    document.getElementById("r2front").addEventListener("click", () => { angle = 0; speed = 0; });
    document.getElementById("r2move").addEventListener("click", () => setMoving(!moving));
    document.getElementById("r2export").addEventListener("click", async () => {
      const json = JSON.stringify(loadStored(), null, 1);
      try { await navigator.clipboard.writeText(json); alertTip(t("copied")); } catch (e) { console.log(json); }
    });
    document.getElementById("r2reset").addEventListener("click", () => {
      saveStored({});
      const droid = window.__r2droid;
      markers.forEach((m) => m.parent.remove(m));
      placeMarkers(droid);
      setSelected(selected);
    });
    window.addEventListener("r2-lang", texts);
  }

  function alertTip(text) {
    const b = document.getElementById("r2export");
    const old = b.textContent;
    b.textContent = text;
    setTimeout(() => { b.textContent = old; }, 1800);
  }

  function fallback() {
    document.getElementById("r2view").style.display = "none";
    document.getElementById("blueprintImg").style.display = "";
    document.getElementById("hotspots").style.display = "";
    console.warn(t("noGl"));
  }

  let last = performance.now();
  function frame(now) {
    const dt = (now - last) / 1000;
    last = now;
    const live = now - speedAt < STALE_MS ? speed : 0;
    angle = (angle + (live / MAX_SPEED) * DEG_PER_S_AT_80 * dt + 540) % 360 - 180;
    if (dome) dome.rotation.y = -THREE.MathUtils.degToRad(angle);
    const c = renderer.domElement;
    const w = c.clientWidth, h = c.clientHeight;
    if (w && h && (c.width !== Math.round(w * renderer.getPixelRatio()) || c.height !== Math.round(h * renderer.getPixelRatio()))) {
      renderer.setSize(w, h, false);
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
    }
    controls.update();
    renderer.render(scene, camera);
    requestAnimationFrame(frame);
  }

  window.R2View3D = { setSelected, onCommand };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
