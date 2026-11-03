// Flight viewer: run-log (README.md) -> three.js scene. World frame = the run's local ENU
// (x east, y north, z up, metres), so the camera's up vector is +z.
import * as THREE from 'three';
import { OrbitControls } from './vendor/OrbitControls.js';
import * as SV from './solver.js';

const Q = new URLSearchParams(location.search);
const RUN = Q.get('run') || 'run/';
const $ = (id) => document.getElementById(id);

// ------------------------------------------------------------------------------------------
// data

async function get(path, kind = 'text') {
  try {
    const r = await fetch(RUN + path, { cache: 'no-store' });
    if (!r.ok) return null;
    return kind === 'json' ? r.json() : kind === 'bin' ? r.arrayBuffer() : r.text();
  } catch { return null; }
}

function parseCSV(s) {
  if (!s) return [];
  const lines = s.split(/\r?\n/).filter((l) => l.length);
  if (!lines.length) return [];
  const h = lines[0].split(',');
  return lines.slice(1).map((l) => {
    const v = l.split(','), o = {};
    h.forEach((k, i) => {
      const x = v[i];
      o[k] = x === undefined || x === '' ? null : (isNaN(+x) ? x : +x);
    });
    return o;
  });
}

const fmtT = (t) => {
  if (t == null || !isFinite(t)) return '–';
  const s = Math.max(0, t), m = Math.floor(s / 60);
  return `${m}:${(s - 60 * m).toFixed(1).padStart(4, '0')}`;
};
const fmtTs = (t) => { const m = Math.floor(t / 60); return `${m}:${String(Math.round(t - 60 * m)).padStart(2, '0')}`; };
const f1 = (v) => (v == null ? '–' : (+v).toFixed(1));

// viridis, t in 0..1 -> THREE.Color (linear)
const VIR = ['#440154', '#482878', '#3e4989', '#31688e', '#26828e', '#1f9e89', '#35b779', '#6ece58', '#b5de2b', '#fde725']
  .map((c) => new THREE.Color(c));
function cmap(t, out = new THREE.Color()) {
  t = Math.min(1, Math.max(0, t)) * (VIR.length - 1);
  const i = Math.min(VIR.length - 2, Math.floor(t));
  return out.copy(VIR[i]).lerp(VIR[i + 1], t - i);
}

// ------------------------------------------------------------------------------------------
// scene

const S = {
  t0: 0, t1: 1, tMax: 1, fade: 0, msize: 1, opac: 1, fsize: 4, playing: false, popac: 0.85, prange: 70, pmax: 150,
  layers: {}, hover: null, pinned: null,
};
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
$('view').appendChild(renderer.domElement);
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x1b2026);
const camera = new THREE.PerspectiveCamera(40, 1, 0.3, 30000);
camera.up.set(0, 0, 1);
const controls = new OrbitControls(camera, renderer.domElement);
controls.mouseButtons = { LEFT: THREE.MOUSE.PAN, MIDDLE: THREE.MOUSE.DOLLY, RIGHT: THREE.MOUSE.ROTATE };
controls.touches = { ONE: THREE.TOUCH.PAN, TWO: THREE.TOUCH.DOLLY_ROTATE };
controls.screenSpacePanning = false; // pan along the ground plane
controls.zoomToCursor = true;
controls.enableDamping = true;
controls.dampingFactor = 0.15;
controls.maxPolarAngle = Math.PI * 0.47;
let dirty = true;
controls.addEventListener('change', () => { dirty = true; });

const hemi = new THREE.HemisphereLight(0xffffff, 0x445040, 1.6);
hemi.position.set(0, 0, 1);
scene.add(hemi);
const sun = new THREE.DirectionalLight(0xffffff, 1.6);
sun.position.set(-0.6, 0.7, 1.0);
scene.add(sun);

const groups = {};
for (const k of ['terrain', 'photos', 'path', 'ticks', 'frames', 'sightings', 'persons', 'solved', 'truth', 'drone', 'hl']) {
  groups[k] = new THREE.Group();
  scene.add(groups[k]);
}

let meta = {}, track = [], frames = [], sights = [], persons = [], truth = [], terr = null;
let heightAt = () => 0;
const labels = []; // {el, pos: Vector3, layer, visible()}

function addLabel(text, pos, cls, layer) {
  const el = document.createElement('div');
  el.className = 'lab ' + cls;
  el.textContent = text;
  $('labels').appendChild(el);
  const L = { el, pos, layer, on: true };
  labels.push(L);
  return L;
}

// ------------------------------------------------------------------------------------------
// terrain

async function buildTerrain(dir = 'terrain/') {
  const tj = await get(dir + 'terrain.json', 'json');
  if (!tj) return false;
  const buf = await get(dir + 'height.bin', 'bin');
  if (!buf) return false;
  for (const o of [...groups.terrain.children]) { groups.terrain.remove(o); o.geometry.dispose(); o.material.map?.dispose(); o.material.dispose(); }
  terr = tj;
  terr.dir = dir;
  const H = new Float32Array(buf), { nx, ny } = terr, [e0, n0, e1, n1] = terr.extent;
  const g = new THREE.PlaneGeometry(e1 - e0, n1 - n0, nx - 1, ny - 1);
  const p = g.attributes.position;
  for (let i = 0; i < p.count; i++) p.setZ(i, H[i]); // both row-major from the north-west corner
  g.translate((e0 + e1) / 2, (n0 + n1) / 2, 0);
  g.computeVertexNormals();
  const tex = await new THREE.TextureLoader().loadAsync(RUN + dir + 'color.jpg?' + (terr.key || '')).catch(() => null);
  if (tex) {
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.anisotropy = renderer.capabilities.getMaxAnisotropy();
  }
  const m = new THREE.MeshLambertMaterial({ map: tex, color: tex ? 0xffffff : 0x6b7a66 });
  const mesh = new THREE.Mesh(g, m);
  groups.terrain.add(mesh);
  heightAt = (e, n) => {
    const c = ((e - e0) / (e1 - e0)) * (nx - 1), r = ((n1 - n) / (n1 - n0)) * (ny - 1);
    const c0 = Math.max(0, Math.min(nx - 2, Math.floor(c))), r0 = Math.max(0, Math.min(ny - 2, Math.floor(r)));
    const fc = Math.min(1, Math.max(0, c - c0)), fr = Math.min(1, Math.max(0, r - r0));
    const h = (rr, cc) => H[rr * nx + cc];
    return h(r0, c0) * (1 - fr) * (1 - fc) + h(r0, c0 + 1) * (1 - fr) * fc + h(r0 + 1, c0) * fr * (1 - fc) + h(r0 + 1, c0 + 1) * fr * fc;
  };
  return true;
}
// lat/lon of the local frame: the map's origin (an anchor, view.py --anchor) or the log's
const origin = () => (terr?.origin?.lat != null ? terr.origin : meta.origin?.lat != null ? meta.origin : meta.anchor?.lat != null ? meta.anchor : null);
const latlon = (e, n) => { const o = origin(); return o ? SV.nedToLatLon(n, e, o.lat, o.lon) : null; };
const fmtLL = (e, n) => { const l = latlon(e, n); return l ? `${l[0].toFixed(6)}, ${l[1].toFixed(6)}` : 'local frame only (no GPS / anchor)'; };

async function setBackground(src) {
  $('bgMsg').textContent = `building ${src} …`;
  let r = null;
  try { r = await (await fetch(`api/terrain?source=${src}`, { cache: 'no-store' })).json(); } catch { r = null; }
  if (!r || !r.ok) { $('bgMsg').textContent = r ? `failed: ${r.error}` : 'needs view.py (the API); static server: use view.py --background'; return; }
  await buildTerrain(r.dir);
  $('bgMsg').textContent = `${terr.kind}${r.errors?.length ? ' (tried: ' + r.errors.join('; ').slice(0, 120) + ')' : ''} · ${terr.attribution || ''}`;
  setSub();
  resetPhotos();
  prepSolver();
  dirty = true;
}

// ------------------------------------------------------------------------------------------
// path: a flat ribbon (metres wide, reads like a map line) + a 1 px line (visible zoomed out)

let ribbon, line, ribAlpha, lineCol;
function buildPath() {
  const n = track.length;
  if (n < 2) return;
  const lp = new Float32Array(n * 3), lc = new Float32Array(n * 4);
  const rp = new Float32Array(n * 6), rc = new Float32Array(n * 8);
  const idx = [];
  const c = new THREE.Color();
  for (let i = 0; i < n; i++) {
    const a = track[i];
    lp.set([a.e, a.n, a.u], i * 3);
    cmap(a.t / S.tMax, c);
    lc.set([c.r, c.g, c.b, 1], i * 4);
    rc.set([c.r, c.g, c.b, 1, c.r, c.g, c.b, 1], i * 8);
    if (i < n - 1) idx.push(2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 1, 2 * i + 3, 2 * i + 2);
  }
  const lg = new THREE.BufferGeometry();
  lg.setAttribute('position', new THREE.BufferAttribute(lp, 3));
  lg.setAttribute('color', (lineCol = new THREE.BufferAttribute(lc, 4)));
  line = new THREE.Line(lg, new THREE.LineBasicMaterial({ vertexColors: true, transparent: true }));
  const rg = new THREE.BufferGeometry();
  rg.setAttribute('position', new THREE.BufferAttribute(rp, 3));
  rg.setAttribute('color', (ribAlpha = new THREE.BufferAttribute(rc, 4)));
  rg.setIndex(idx);
  ribbon = new THREE.Mesh(rg, new THREE.MeshBasicMaterial({ vertexColors: true, transparent: true, side: THREE.DoubleSide, depthWrite: false }));
  ribbon.renderOrder = 2;
  line.renderOrder = 3;
  groups.path.add(ribbon, line);
  sizeRibbon();
}
function sizeRibbon() {
  if (!ribbon) return;
  const w = 0.35 * S.msize, p = ribbon.geometry.attributes.position, n = track.length;
  for (let i = 0; i < n; i++) {
    const a = track[Math.max(0, i - 1)], b = track[Math.min(n - 1, i + 1)], m = track[i];
    let dx = b.e - a.e, dy = b.n - a.n;
    const L = Math.hypot(dx, dy) || 1;
    dx /= L; dy /= L;
    p.setXYZ(2 * i, m.e - dy * w, m.n + dx * w, m.u);
    p.setXYZ(2 * i + 1, m.e + dy * w, m.n - dx * w, m.u);
  }
  p.needsUpdate = true;
  ribbon.geometry.computeBoundingSphere();
}

// time marks along the path
const ticks = [];
function buildTicks() {
  if (track.length < 2) return;
  const steps = [5, 10, 15, 30, 60, 120, 300, 600, 1200];
  const step = steps.find((s) => S.tMax / s <= 40) || 1800;
  const geo = new THREE.SphereGeometry(1, 10, 8);
  const mat = new THREE.MeshBasicMaterial({ color: 0xffffff });
  let j = 0;
  for (let t = 0; t <= S.tMax + 1e-6; t += step) {
    while (j < track.length - 1 && track[j + 1].t < t) j++;
    const a = track[j], b = track[Math.min(track.length - 1, j + 1)];
    const w = b.t > a.t ? Math.min(1, Math.max(0, (t - a.t) / (b.t - a.t))) : 0;
    const pos = new THREE.Vector3(a.e + (b.e - a.e) * w, a.n + (b.n - a.n) * w, a.u + (b.u - a.u) * w);
    const dot = new THREE.Mesh(geo, mat);
    dot.position.copy(pos);
    groups.ticks.add(dot);
    ticks.push({ t, dot, label: addLabel(fmtTs(t), pos, 'tick', 'ticks') });
  }
  sizeTicks();
}
function sizeTicks() { for (const k of ticks) k.dot.scale.setScalar(0.3 * S.msize); }

// drone marker at the end of the time range
let drone;
function buildDrone() {
  const shape = new THREE.Shape();
  shape.moveTo(1.6, 0); shape.lineTo(-1, 0.9); shape.lineTo(-0.5, 0); shape.lineTo(-1, -0.9); shape.lineTo(1.6, 0);
  drone = new THREE.Mesh(new THREE.ShapeGeometry(shape), new THREE.MeshBasicMaterial({ color: 0xffffff, side: THREE.DoubleSide, depthTest: false }));
  const ring = new THREE.Mesh(new THREE.RingGeometry(1.9, 2.2, 32), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.5, depthTest: false }));
  drone.add(ring);
  drone.renderOrder = 10;
  groups.drone.add(drone);
}
function trackAt(t) {
  if (!track.length) return null;
  let lo = 0, hi = track.length - 1;
  if (t <= track[0].t) return track[0];
  if (t >= track[hi].t) return track[hi];
  while (hi - lo > 1) { const m = (lo + hi) >> 1; if (track[m].t <= t) lo = m; else hi = m; }
  const a = track[lo], b = track[hi], w = (t - a.t) / Math.max(1e-9, b.t - a.t);
  let dy = b.yaw - a.yaw; dy = ((dy + 540) % 360) - 180;
  return { t, e: a.e + (b.e - a.e) * w, n: a.n + (b.n - a.n) * w, u: a.u + (b.u - a.u) * w, yaw: a.yaw + dy * w };
}

// ------------------------------------------------------------------------------------------
// camera frustums: optical frame (x right, y down, z forward) rotated by the logged quaternion

const CAM = { w: 1280, h: 720, fx: 900, fy: 900, cx: 640, cy: 360 };
function camQuat(r) {
  return new THREE.Quaternion(r.cam_qx, r.cam_qy, r.cam_qz, r.cam_qw);
}
function frustumPts(r, d) {
  const q = camQuat(r), o = new THREE.Vector3(r.cam_e, r.cam_n, r.cam_u);
  const P = (u, v) => new THREE.Vector3(((u - CAM.cx) / CAM.fx) * d, ((v - CAM.cy) / CAM.fy) * d, d).applyQuaternion(q).add(o);
  const c = [P(0, 0), P(CAM.w, 0), P(CAM.w, CAM.h), P(0, CAM.h)];
  const up = P(CAM.w / 2, -0.25 * CAM.h);
  // apex->corners, base rectangle, the 'image top' tick
  return [o, c[0], o, c[1], o, c[2], o, c[3], c[0], c[1], c[1], c[2], c[2], c[3], c[3], c[0], c[0], up, up, c[1]];
}
const FV = 20; // vertices per frustum
let frusLines, frusCol;
function buildFrustums() {
  const list = frames;
  if (!list.length) return;
  const pos = new Float32Array(list.length * FV * 3), col = new Float32Array(list.length * FV * 4);
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', (frusCol = new THREE.BufferAttribute(col, 4)));
  frusLines = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, depthWrite: false }));
  frusLines.renderOrder = 4;
  groups.frames.add(frusLines);
  sizeFrustums();
}
function sizeFrustums() {
  if (!frusLines) return;
  const p = frusLines.geometry.attributes.position;
  frames.forEach((r, i) => frustumPts(r, S.fsize).forEach((v, k) => p.setXYZ(i * FV + k, v.x, v.y, v.z)));
  p.needsUpdate = true;
  frusLines.geometry.computeBoundingSphere();
}

// ------------------------------------------------------------------------------------------
// sightings: instanced spheres with a per-instance opacity

let sph, sphAlpha;
function buildSightings() {
  const n = sights.length;
  if (!n) return;
  const mat = new THREE.MeshLambertMaterial({ transparent: true, depthWrite: false });
  mat.onBeforeCompile = (sh) => {
    sh.vertexShader = sh.vertexShader
      .replace('#include <common>', '#include <common>\nattribute float instAlpha;\nvarying float vInstAlpha;')
      .replace('#include <begin_vertex>', '#include <begin_vertex>\nvInstAlpha = instAlpha;');
    sh.fragmentShader = sh.fragmentShader
      .replace('#include <common>', '#include <common>\nvarying float vInstAlpha;')
      .replace('vec4 diffuseColor = vec4( diffuse, opacity );', 'vec4 diffuseColor = vec4( diffuse, opacity * vInstAlpha );\nif (diffuseColor.a < 0.004) discard;');
  };
  const geo = new THREE.SphereGeometry(1, 16, 12);
  sphAlpha = new THREE.InstancedBufferAttribute(new Float32Array(n), 1);
  geo.setAttribute('instAlpha', sphAlpha);
  sph = new THREE.InstancedMesh(geo, mat, n);
  sph.renderOrder = 5;
  const c = new THREE.Color();
  sights.forEach((s, i) => {
    if (s.r != null) c.setRGB(s.r / 255, s.g / 255, s.b / 255, THREE.SRGBColorSpace);
    else c.set(0xff7043);
    sph.setColorAt(i, c);
  });
  groups.sightings.add(sph);
  sizeSightings();
}
function sizeSightings() {
  if (!sph) return;
  const m = new THREE.Matrix4(), r = 0.8 * S.msize;
  sights.forEach((s, i) => { m.makeScale(r, r, r).setPosition(s.e, s.n, s.u + r * 0.6); sph.setMatrixAt(i, m); });
  sph.instanceMatrix.needsUpdate = true;
  sph.computeBoundingSphere();
}
const sphPos = (s) => new THREE.Vector3(s.e, s.n, s.u + 0.8 * S.msize * 0.6);

// persons (clusters): instanced rings + labels; truth: pins + labels
let rings;
function buildPersons() {
  const n = persons.length;
  if (!n) return;
  rings = new THREE.InstancedMesh(new THREE.RingGeometry(0.75, 1, 40), new THREE.MeshBasicMaterial({ transparent: true, side: THREE.DoubleSide, depthWrite: false }), n);
  rings.renderOrder = 6;
  const on = new THREE.Color(0xff4fd8), off = new THREE.Color(0x8a8f96);
  persons.forEach((p, i) => {
    rings.setColorAt(i, p.pinged ? on : off);
    p.label = addLabel('', new THREE.Vector3(p.e, p.n, p.u + 1), 'person' + (p.pinged ? '' : ' off'), 'persons');
  });
  groups.persons.add(rings);
  sizePersons();
}
function sizePersons() {
  if (!rings) return;
  const m = new THREE.Matrix4(), r = 2.4 * S.msize;
  persons.forEach((p, i) => {
    m.makeScale(r, r, 1).setPosition(p.e, p.n, p.u + 0.25);
    rings.setMatrixAt(i, p.vis ? m : new THREE.Matrix4().makeScale(0, 0, 0));
    p.label.pos.set(p.e, p.n, p.u + r + 0.5);
  });
  rings.instanceMatrix.needsUpdate = true;
  rings.computeBoundingSphere();
}
let pins;
function buildTruth() {
  if (!truth.length) { $('side').querySelector('[data-layer=truth]').parentElement.classList.add('hidden'); return; }
  const g = new THREE.ConeGeometry(0.7, 2.2, 12);
  g.rotateX(-Math.PI / 2); // tip down (-z)
  pins = new THREE.InstancedMesh(g, new THREE.MeshLambertMaterial({ color: 0x36d46b }), truth.length);
  truth.forEach((t) => { t.label = addLabel(`${t.id} (${t.state})`, new THREE.Vector3(t.e, t.n, t.u + 3), 'truth', 'truth'); });
  groups.truth.add(pins);
  sizeTruth();
}
function sizeTruth() {
  if (!pins) return;
  const m = new THREE.Matrix4(), s = S.msize;
  truth.forEach((t, i) => {
    m.makeScale(s, s, s).setPosition(t.e, t.n, t.u + 1.1 * s + 1.8);
    pins.setMatrixAt(i, m);
    t.label.pos.set(t.e, t.n, t.u + 2.2 * s + 2);
  });
  pins.instanceMatrix.needsUpdate = true;
  pins.computeBoundingSphere();
}

// ------------------------------------------------------------------------------------------
// position solver (solver.js): weights + parameters -> solved persons, live error vs truth

const SP = JSON.parse(JSON.stringify(SV.DEFAULTS));
let prep = [], solved = { persons: [] }, solvScore = null, solvRings, solvDirty = true;
const SOLV_KEYS = ['clusterM', 'minSight', 'minConf', 'headMinPx', 'headH', 'bodyH', 'terrainSigma'];
function prepSolver() {
  SP.fy = CAM.fy;
  prep = SV.prepare(sights, CAM, terr ? heightAt : null, SP);
  solvDirty = true;
}
function runSolver() {
  if (!solvDirty) return;
  solvDirty = false;
  const t = performance.now();
  solved = SV.solve(prep, SP, S.t0, S.t1);
  solvScore = truth.length ? SV.score(solved.persons, truth, SP.matchM) : null;
  solved.ms = performance.now() - t;
  buildSolved();
  showSolverOut();
  if (S.pinned?.kind === 'q') showItem(S.pinned.i < solved.persons.length ? S.pinned : null);
  dirty = true;
}
function showSolverOut() {
  const pp = solved.persons, pin = pp.filter((p) => p.pinged).length;
  const src = {};
  for (const g of prep) {
    if (g.rTerrLog != null || g.rTerrMap != null) src.terrain = (src.terrain || 0) + 1;
    if (g.headPx != null) src.head = (src.head || 0) + 1;
    if (g.dDepth != null) src.depth = (src.depth || 0) + 1;
  }
  const L = [`${solved.used} sightings → ${pin} persons (+${pp.length - pin} cand.) · ${solved.ms.toFixed(0)} ms`,
    `have: ${prep.length} rays, ${src.terrain || 0} terrain, ${src.head || 0} head, ${src.depth || 0} depth`];
  const sc = solvScore;
  if (sc) {
    const f = (v) => (v == null ? '–' : v.toFixed(2));
    L.push(`<span class="${sc.found ? 'good' : 'bad'}">truth: found ${sc.found}/${sc.total} within ${SP.matchM} m</span>`);
    L.push(`error mean ${f(sc.mean)}  median ${f(sc.median)}  max ${f(sc.max)} m`);
    L.push(`<span class="${sc.falsePos ? 'bad' : 'good'}">false persons: ${sc.falsePos}</span>`);
  }
  $('solvOut').innerHTML = L.join('\n');
}
function buildSolved() {
  for (const o of [...groups.solved.children]) { groups.solved.remove(o); o.geometry?.dispose(); }
  for (const p of solved.persons) if (p.label) p.label.el.remove();
  for (let i = labels.length - 1; i >= 0; i--) if (labels[i].layer === 'solved') { labels[i].el.remove(); labels.splice(i, 1); }
  const pp = solved.persons;
  if (!pp.length) return;
  const geo = new THREE.RingGeometry(0.86, 1, 48);
  solvRings = new THREE.InstancedMesh(geo, new THREE.MeshBasicMaterial({ transparent: true, opacity: 0.95, side: THREE.DoubleSide, depthWrite: false, depthTest: false }), pp.length);
  solvRings.renderOrder = 7;
  const dotM = new THREE.InstancedMesh(new THREE.CircleGeometry(1, 16), new THREE.MeshBasicMaterial({ side: THREE.DoubleSide, depthTest: false }), pp.length);
  dotM.renderOrder = 8;
  const on = new THREE.Color(0x3fd6ff), off = new THREE.Color(0x6f8a94), m = new THREE.Matrix4(), q = new THREE.Quaternion(), z = new THREE.Vector3(0, 0, 1);
  pp.forEach((p, i) => {
    const k = 2.45; // 2 sigma (95 %) of a 2D Gaussian
    const a = Math.max(0.4, Math.min(60, k * p.ell.a)), b = Math.max(0.4, Math.min(60, k * p.ell.b));
    q.setFromAxisAngle(z, p.ell.ang);
    m.compose(new THREE.Vector3(p.e, p.n, p.u + 0.35), q, new THREE.Vector3(a, b, 1));
    solvRings.setMatrixAt(i, m);
    solvRings.setColorAt(i, p.pinged ? on : off);
    m.compose(new THREE.Vector3(p.e, p.n, p.u + 0.35), q, new THREE.Vector3(0.35 * S.msize, 0.35 * S.msize, 1));
    dotM.setMatrixAt(i, m);
    dotM.setColorAt(i, p.pinged ? on : off);
    p.label = addLabel(`S${p.id} · ${p.n_sight}${p.err != null && isFinite(p.err) && truth.length ? ` · ${p.err.toFixed(1)} m` : ''}`,
      new THREE.Vector3(p.e, p.n, p.u + 2), 'solved' + (p.pinged ? '' : ' off'), 'solved');
    p.label.show = p.pinged;
  });
  groups.solved.add(solvRings, dotM);
}
function bindSolverUI() {
  const fmt = { clusterM: (v) => `${v} m`, minSight: (v) => `${v}`, minConf: (v) => v.toFixed(2), headMinPx: (v) => `${v} px`,
    headH: (v) => `${v.toFixed(2)} m`, bodyH: (v) => `${v.toFixed(2)} m`, terrainSigma: (v) => `${v.toFixed(1)} m` };
  const set = () => {
    for (const k of Object.keys(SP.w)) { $('w_' + k).value = SP.w[k]; $('w_' + k + 'V').textContent = (+SP.w[k]).toFixed(1); }
    for (const k of SOLV_KEYS) { $(k).value = SP[k]; $(k + 'V').textContent = fmt[k](+SP[k]); }
    $('reTerrain').checked = SP.reTerrain; $('useTime').checked = SP.useTime;
  };
  for (const k of Object.keys(SP.w)) $('w_' + k).addEventListener('input', (e) => { SP.w[k] = +e.target.value; set(); solvDirty = true; });
  for (const k of SOLV_KEYS) $(k).addEventListener('input', (e) => { SP[k] = +e.target.value; set(); solvDirty = true; });
  $('reTerrain').addEventListener('change', (e) => { SP.reTerrain = e.target.checked; prepSolver(); });
  $('useTime').addEventListener('change', (e) => { SP.useTime = e.target.checked; solvDirty = true; });
  $('solvReset').onclick = () => { Object.assign(SP, JSON.parse(JSON.stringify(SV.DEFAULTS)), { fy: CAM.fy, reTerrain: SV.DEFAULTS.reTerrain || defaultReTerrain() }); set(); prepSolver(); };
  $('solvExport').onclick = exportSolved;
  $('solvFold').onclick = () => { const h = $('solvBody').classList.toggle('hidden'); $('solvFold').textContent = h ? '+' : '–'; };
  set();
}
const defaultReTerrain = () => !!terr && terr.kind !== 'flat' && meta.source === 'real';
function download(name, text, type) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], { type }));
  a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}
function exportSolved() {
  const rows = solved.persons.map((p) => {
    const ll = latlon(p.e, p.n);
    return { person: p.id, pinged: p.pinged ? 1 : 0, n_sight: p.n_sight, conf_max: +p.conf_max.toFixed(2),
      e: +p.e.toFixed(2), n: +p.n.toFixed(2), u: +p.u.toFixed(2), lat: ll ? +ll[0].toFixed(7) : '', lon: ll ? +ll[1].toFixed(7) : '',
      sigma_major_m: +p.ell.a.toFixed(2), sigma_minor_m: +p.ell.b.toFixed(2), ellipse_deg_from_east: +(p.ell.ang * 180 / Math.PI).toFixed(1),
      t_first: +p.t_first.toFixed(2), t_last: +p.t_last.toFixed(2),
      sources: Object.entries(p.src).map(([k, v]) => `${k}:${v}`).join(' '), truth: p.truth ?? '', err_m: p.err != null && isFinite(p.err) ? +p.err.toFixed(2) : '' };
  });
  const head = Object.keys(rows[0] || { person: 0 });
  download('persons_solved.csv', [head.join(','), ...rows.map((r) => head.map((k) => r[k]).join(','))].join('\n') + '\n', 'text/csv');
  const { fy, ...params } = SP;
  download('persons_solved.json', JSON.stringify({ run: meta.start_time, world: meta.world, origin: origin(), params,
    time_range: SP.useTime ? [S.t0, S.t1] : null, score: solvScore && { ...solvScore, per: solvScore.per }, persons: rows }, null, 1), 'application/json');
}

// ------------------------------------------------------------------------------------------
// drone photos on the ground (experimental): sampled frames, image grid -> ray/terrain hits ->
// textured meshes. Rays above the horizon or beyond 'photo range' are cut away.

let photos = [], photosBuilt = false;
const PG = [16, 9]; // grid vertices across, down
function resetPhotos() {
  for (const o of [...groups.photos.children]) { groups.photos.remove(o); o.geometry.dispose(); o.material.map?.dispose(); o.material.dispose(); }
  photos = []; photosBuilt = false;
  if (S.layers.photos) buildPhotos();
}
function buildPhotos() {
  if (photosBuilt || !frames.length) return;
  photosBuilt = true;
  const gnd0 = terr ? heightAt : () => meta.ground_u ?? 0;
  // frames from >= 5 m over the ground (takeoff / landing frames only smear)
  const all = frames.filter((f) => f.kind !== 'sighting' && f.cam_u - gnd0(f.cam_e, f.cam_n) >= 5);
  const pick = all.length <= S.pmax ? all : Array.from({ length: S.pmax }, (_, k) => all[Math.round((k * (all.length - 1)) / (S.pmax - 1))]);
  const gnd = terr ? heightAt : () => meta.ground_u ?? 0;
  const loader = new THREE.TextureLoader();
  pick.forEach((f, order) => {
    const [nu, nv] = PG, pos = new Float32Array(nu * nv * 3), uv = new Float32Array(nu * nv * 2), ok = new Uint8Array(nu * nv);
    const c = [f.cam_e, f.cam_n, f.cam_u];
    for (let j = 0; j < nv; j++) for (let i = 0; i < nu; i++) {
      const u = (i / (nu - 1)) * CAM.w, v = (j / (nv - 1)) * CAM.h, k = j * nu + i;
      const d = SV.pixelRay(f, CAM, u, v);
      const r = d[2] < -0.14 ? SV.rayTerrain(c, d, gnd, S.prange) : null; // >= 8 deg below the horizon
      uv.set([u / CAM.w, 1 - v / CAM.h], 2 * k);
      if (r != null) { pos.set([c[0] + d[0] * r, c[1] + d[1] * r, c[2] + d[2] * r + 0.15], 3 * k); ok[k] = 1; }
    }
    const idx = [];
    for (let j = 0; j < nv - 1; j++) for (let i = 0; i < nu - 1; i++) {
      const a = j * nu + i, b = a + 1, cc = a + nu, dd = cc + 1;
      if (ok[a] && ok[b] && ok[cc]) idx.push(a, cc, b);
      if (ok[b] && ok[cc] && ok[dd]) idx.push(b, cc, dd);
    }
    if (!idx.length) return;
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
    g.setIndex(idx);
    const mat = new THREE.MeshBasicMaterial({ transparent: true, opacity: S.popac, depthWrite: false, side: THREE.DoubleSide,
      polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -4 });
    const mesh = new THREE.Mesh(g, mat);
    mesh.renderOrder = 1 + order / 10000;
    mesh.userData.t = f.t;
    groups.photos.add(mesh);
    photos.push(mesh);
    mesh.visible = false; // until its photo is loaded (no white quads)
    loader.load(RUN + f.file, (tex) => { tex.colorSpace = THREE.SRGBColorSpace; mat.map = tex; mat.needsUpdate = true; mesh.userData.loaded = true; applyTime(); });
  });
  applyTime();
}

// scale bar + north arrow (any map, also the flat grid)
function drawScale() {
  const r = renderer.domElement.getBoundingClientRect();
  const dist = camera.position.distanceTo(controls.target);
  const mpp = (2 * dist * Math.tan(THREE.MathUtils.degToRad(camera.fov / 2))) / r.height;
  const steps = [1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 5000];
  const m = steps.find((s) => s / mpp >= 60) || 10000;
  $('sbar').style.width = `${Math.round(m / mpp)}px`;
  $('slab').textContent = m >= 1000 ? `${m / 1000} km` : `${m} m`;
  const a = screenOf(controls.target), b = screenOf(controls.target.clone().add(new THREE.Vector3(0, Math.max(1, dist * 0.05), 0)));
  if (a && b) $('northG').setAttribute('transform', `rotate(${(Math.atan2(b[0] - a[0], a[1] - b[1]) * 180) / Math.PI})`);
  $('cursorLL').textContent = fmtLL(controls.target.x, controls.target.y);
}

// ------------------------------------------------------------------------------------------
// time filter + fade

function fadeOf(t) {
  if (t < S.t0 - 1e-6 || t > S.t1 + 1e-6) return 0;
  if (!S.fade) return 1;
  const age = S.t1 - t;
  return age <= S.fade ? 1 : Math.max(0, 1 - (age - S.fade) / (0.5 * S.fade));
}
function applyTime() {
  if (line) {
    for (let i = 0; i < track.length; i++) {
      const a = fadeOf(track[i].t);
      lineCol.setW(i, a);
      ribAlpha.setW(2 * i, a); ribAlpha.setW(2 * i + 1, a);
    }
    lineCol.needsUpdate = ribAlpha.needsUpdate = true;
  }
  if (frusLines) {
    frames.forEach((r, i) => {
      const a = 0.85 * fadeOf(r.t) * (r.kind === 'sighting' ? (S.sframes ? 0.6 : 0) : 1);
      for (let k = 0; k < FV; k++) frusCol.setXYZW(i * FV + k, r.kind === 'sighting' ? 1 : 0.92, r.kind === 'sighting' ? 0.75 : 0.95, r.kind === 'sighting' ? 0.55 : 1, a);
      r.alpha = a;
    });
    frusCol.needsUpdate = true;
  }
  if (sph) {
    sights.forEach((s, i) => {
      s.alpha = Math.min(1, Math.min(0.5, Math.max(0, 0.5 * (s.conf ?? 1))) * S.opac) * fadeOf(s.t);
      sphAlpha.setX(i, s.alpha);
    });
    sphAlpha.needsUpdate = true;
  }
  if (rings) {
    const cnt = new Map();
    for (const s of sights) if (s.t >= S.t0 && s.t <= S.t1 && s.person != null) cnt.set(s.person, (cnt.get(s.person) || 0) + 1);
    const anySightings = sights.length > 0;
    for (const p of persons) {
      p.count = anySightings ? cnt.get(p.person) || 0 : p.n_sight;
      p.vis = anySightings ? p.count > 0 : (p.t_first ?? 0) <= S.t1;
      p.label.el.textContent = `P${p.person} · ${p.count}${p.count !== p.n_sight ? '/' + p.n_sight : ''}`;
    }
    sizePersons();
  }
  for (const m of photos) {
    const a = fadeOf(m.userData.t);
    m.visible = a > 0.02 && !!m.userData.loaded;
    m.material.opacity = S.popac * a;
  }
  if (SP.useTime) solvDirty = true;
  for (const k of ticks) { k.dot.visible = k.t >= S.t0 - 1e-6 && k.t <= S.t1 + 1e-6 && fadeOf(k.t) > 0.05; }
  if (drone) {
    const a = trackAt(S.t1);
    if (a) {
      drone.position.set(a.e, a.n, a.u + 0.3);
      drone.rotation.set(0, 0, Math.PI / 2 - THREE.MathUtils.degToRad(a.yaw));
      drone.scale.setScalar(S.msize);
    }
  }
  // slider + label
  const L = (S.t0 / S.tMax) * 100, R = (S.t1 / S.tMax) * 100;
  $('sel').style.left = `${L}%`; $('sel').style.width = `${Math.max(0, R - L)}%`;
  $('tlab').textContent = `${fmtT(S.t0)} – ${fmtT(S.t1)} / ${fmtT(S.tMax)}`;
  dirty = true;
}

// ------------------------------------------------------------------------------------------
// picking (screen space: nearest marker within a few px) + the info panel

const v3 = new THREE.Vector3();
function screenOf(p) {
  v3.copy(p).project(camera);
  if (v3.z > 1 || v3.z < -1) return null;
  const r = renderer.domElement.getBoundingClientRect();
  return [(v3.x * 0.5 + 0.5) * r.width + r.left, (-v3.y * 0.5 + 0.5) * r.height + r.top];
}
function pick(x, y) {
  let best = null, bd = 1e9;
  const test = (p, item, bias, rad = 12) => {
    const s = screenOf(p);
    if (!s) return;
    const d = Math.hypot(s[0] - x, s[1] - y) + bias;
    if (d < rad + bias && d < bd) { bd = d; best = item; }
  };
  if (S.layers.sightings) sights.forEach((s, i) => { if (s.alpha > 0.003) test(sphPos(s), { kind: 's', i }, 0); });
  if (S.layers.persons) persons.forEach((p, i) => { if (p.vis) test(new THREE.Vector3(p.e, p.n, p.u), { kind: 'p', i }, 2, 18); });
  if (S.layers.frames) frames.forEach((r, i) => { if (r.alpha > 0.02) test(new THREE.Vector3(r.cam_e, r.cam_n, r.cam_u), { kind: 'f', i }, 4); });
  if (S.layers.truth) truth.forEach((t, i) => test(new THREE.Vector3(t.e, t.n, t.u + 2), { kind: 't', i }, 6));
  if (S.layers.solved) solved.persons.forEach((p, i) => test(new THREE.Vector3(p.e, p.n, p.u + 0.35), { kind: 'q', i }, 1, 16));
  return best;
}

const hlMat = new THREE.LineBasicMaterial({ color: 0xffd23f, depthTest: false, transparent: true });
const hlMat2 = new THREE.LineBasicMaterial({ color: 0xffd23f, depthTest: false, transparent: true, opacity: 0.55 });
function clearHL() {
  for (const o of [...groups.hl.children]) { groups.hl.remove(o); o.geometry?.dispose(); }
}
function hlFrustum(r, d, mat = hlMat) {
  const g = new THREE.BufferGeometry().setFromPoints(frustumPts(r, d));
  const l = new THREE.LineSegments(g, mat);
  l.renderOrder = 20;
  groups.hl.add(l);
}
function hlLine(a, b, mat = hlMat) {
  const l = new THREE.Line(new THREE.BufferGeometry().setFromPoints([a, b]), mat);
  l.renderOrder = 20;
  groups.hl.add(l);
}
function hlRing(p, r) {
  const m = new THREE.Mesh(new THREE.RingGeometry(r * 1.05, r * 1.3, 40), new THREE.MeshBasicMaterial({ color: 0xffd23f, depthTest: false, side: THREE.DoubleSide }));
  m.position.copy(p);
  m.renderOrder = 21;
  groups.hl.add(m);
}
const hasCam = (r) => r && r.cam_qw != null && r.cam_e != null;

function showItem(it) {
  clearHL();
  const box = $('infoBox'), img = $('infoImg');
  if (!it) { $('info').classList.add('hidden'); dirty = true; return; }
  $('info').classList.remove('hidden');
  const rows = [];
  const row = (k, v) => rows.push(`<span class="k">${k}</span><span>${v}</span>`);
  let src = null, boxes = [];
  if (it.kind === 's') {
    const s = sights[it.i];
    const fr = s.frame != null ? frameById.get(s.frame) : null;
    $('infoTitle').textContent = `Sighting ${it.i}${s.person != null ? ' · person P' + s.person : ''}`;
    src = fr ? fr.file : s.crop;
    if (fr) boxes = [[s.u0, s.v0, s.u1, s.v1, s.conf]];
    if (fr && s.head_u0 != null) boxes.push([s.head_u0, s.head_v0, s.head_u1, s.head_v1, s.head_conf, 'head']);
    if (s.crop || s.r != null) {
      rows.push(`<div class="crop">${s.crop ? `<img src="${RUN + s.crop}" alt="crop">` : ''}${s.r != null ? `<span class="sw" style="background:rgb(${s.r},${s.g},${s.b})"></span><span class="muted">mean colour ${s.r},${s.g},${s.b}</span>` : ''}</div>`);
    }
    row('time', fmtT(s.t));
    row('confidence', `${(+s.conf).toFixed(2)} (opacity ${(100 * Math.min(0.5, 0.5 * s.conf)).toFixed(0)} %)`);
    row('method', `${s.method ?? '–'}, range ${f1(s.range_m)} m`);
    const est = [];
    if (s.d_gnd != null) est.push(`terrain ${f1(s.d_gnd)}`);
    if (s.d_depth != null) est.push(`depth ${f1(s.d_depth)}`);
    if (s.head_px != null) est.push(`head ${f1((SP.headH * CAM.fy) / s.head_px)} (${f1(s.head_px)} px, conf ${(+s.head_conf).toFixed(2)})`);
    est.push(`body ${f1((SP.bodyH * CAM.fy) / Math.max(1, s.v1 - s.v0))} (${s.v1 - s.v0} px)`);
    row('ranges', est.join(' · ') + ' m');
    row('position', `E ${f1(s.e)}  N ${f1(s.n)}  U ${f1(s.u)} m`);
    if (s.lat != null) row('lat, lon', `${(+s.lat).toFixed(6)}, ${(+s.lon).toFixed(6)}`);
    const cam = hasCam(s) ? s : fr;
    if (cam) {
      row('camera', `E ${f1(cam.cam_e)}  N ${f1(cam.cam_n)}  U ${f1(cam.cam_u)} m`);
      const o = new THREE.Vector3(cam.cam_e, cam.cam_n, cam.cam_u), p = sphPos(s);
      hlFrustum(cam, Math.max(S.fsize, o.distanceTo(p)));
      hlLine(o, p);
    }
    hlRing(sphPos(s), 0.8 * S.msize);
    if (fr) row('frame', `#${fr.id} ${fr.file}`);
  } else if (it.kind === 'f') {
    const r = frames[it.i];
    $('infoTitle').textContent = `Camera frame #${r.id}${r.kind === 'sighting' ? ' (sighting)' : ''}`;
    src = r.file;
    boxes = (sightsByFrame.get(r.id) || []).map((s) => [s.u0, s.v0, s.u1, s.v1, s.conf]);
    row('time', fmtT(r.t));
    row('drone', `E ${f1(r.e)}  N ${f1(r.n)}  U ${f1(r.u)} m`);
    row('attitude', `roll ${f1(r.roll)}°  pitch ${f1(r.pitch)}°  yaw ${f1(r.yaw)}°`);
    const g = heightAt(r.cam_e, r.cam_n);
    row('height', `${f1(r.cam_u - g)} m over the terrain`);
    if (boxes.length) row('detections', `${boxes.length}`);
    hlFrustum(r, Math.max(S.fsize * 1.5, 6));
  } else if (it.kind === 'p') {
    const p = persons[it.i];
    $('infoTitle').textContent = `Person P${p.person}${p.pinged ? '' : ' (candidate)'}`;
    const mine = sights.filter((s) => s.person === p.person && s.t >= S.t0 && s.t <= S.t1);
    const best = mine.reduce((a, s) => (!a || s.conf > a.conf ? s : a), null);
    const fr = best && best.frame != null ? frameById.get(best.frame) : null;
    src = fr ? fr.file : best?.crop;
    if (fr) boxes = [[best.u0, best.v0, best.u1, best.v1, best.conf]];
    row('sightings', `${mine.length} in range, ${p.n_sight} total`);
    row('conf max', (+p.conf_max).toFixed(2));
    row('position', `E ${f1(p.e)}  N ${f1(p.n)}  U ${f1(p.u)} m`);
    if (p.lat != null) row('lat, lon', `${(+p.lat).toFixed(6)}, ${(+p.lon).toFixed(6)}`);
    if (p.t_first != null) row('seen', `${fmtT(p.t_first)} – ${fmtT(p.t_last)}`);
    if (p.truth) row('nearest truth', `${p.truth}, ${p.err_m} m`);
    if (best) row('photo', `best sighting, conf ${(+best.conf).toFixed(2)} at ${fmtT(best.t)}`);
    for (const s of mine.slice(0, 300)) if (hasCam(s)) hlLine(new THREE.Vector3(s.cam_e, s.cam_n, s.cam_u), sphPos(s), hlMat2);
    if (best && hasCam(best)) hlFrustum(best, Math.max(S.fsize, best.range_m || 0));
    hlRing(new THREE.Vector3(p.e, p.n, p.u + 0.3), 2.4 * S.msize);
  } else if (it.kind === 'q') {
    const p = solved.persons[it.i];
    if (!p) { $('info').classList.add('hidden'); return; }
    $('infoTitle').textContent = `Solved person S${p.id}${p.pinged ? '' : ' (candidate)'}`;
    const best = p.members.reduce((a, g) => (!a || g.conf > a.conf ? g : a), null)?.s;
    const fr = best && best.frame != null ? frameById.get(best.frame) : null;
    src = fr ? fr.file : best?.crop;
    if (fr) boxes = [[best.u0, best.v0, best.u1, best.v1, best.conf]];
    if (fr && best.head_u0 != null) boxes.push([best.head_u0, best.head_v0, best.head_u1, best.head_v1, best.head_conf, 'head']);
    row('sightings', `${p.n_sight} (${fmtT(p.t_first)} – ${fmtT(p.t_last)})`);
    row('position', `E ${f1(p.e)}  N ${f1(p.n)}  U ${f1(p.u)} m`);
    row('lat, lon', fmtLL(p.e, p.n));
    row('uncertainty', `1σ ${p.ell.a.toFixed(2)} × ${p.ell.b.toFixed(2)} m (ring = 2σ)`);
    const per = {};
    p.cons.forEach((q, j) => {
      const e = [p.e - q.p[0], p.n - q.p[1], p.u - q.p[2]], al = e[0] * q.d[0] + e[1] * q.d[1] + e[2] * q.d[2];
      const ac = Math.sqrt(Math.max(0, e[0] ** 2 + e[1] ** 2 + e[2] ** 2 - al * al));
      const o = (per[q.src] ||= { n: 0, al: 0, ac: 0, out: 0 });
      o.n++; o.al += q.src === 'ray' ? 0 : al; o.ac += ac; o.out += p.hub[j] < 1 ? 1 : 0;
    });
    for (const [k, o] of Object.entries(per)) row(k, `${o.n}× ${k === 'ray' ? '' : `along ${(o.al / o.n).toFixed(1)} m, `}across ${(o.ac / o.n).toFixed(1)} m${o.out ? `, ${o.out} down-weighted` : ''}`);
    if (p.truth) row('nearest truth', `${p.truth}, ${p.err.toFixed(2)} m`);
    const P0 = new THREE.Vector3(p.e, p.n, p.u);
    for (const g of p.members.slice(0, 400)) { // the contributing rays, camera -> past the solution
      const c = new THREE.Vector3(...g.c), d = new THREE.Vector3(...g.d), r = P0.clone().sub(c).dot(d);
      hlLine(c, c.clone().addScaledVector(d, r + 3), hlMat2);
    }
    hlRing(new THREE.Vector3(p.e, p.n, p.u + 0.4), 1.2 * S.msize);
  } else if (it.kind === 't') {
    const t = truth[it.i];
    $('infoTitle').textContent = `Sim truth ${t.id}`;
    row('state', `${t.state}${t.zone ? ', ' + t.zone : ''}`);
    row('position', `E ${f1(t.e)}  N ${f1(t.n)}  U ${f1(t.u)} m`);
    const near = persons.map((p) => [Math.hypot(p.e - t.e, p.n - t.n), p]).sort((a, b) => a[0] - b[0])[0];
    if (near) row('nearest person', `P${near[1].person}, ${near[0].toFixed(1)} m`);
    hlRing(new THREE.Vector3(t.e, t.n, t.u + 0.3), 1.5 * S.msize);
  }
  $('infoBody').innerHTML = rows.join('');
  $('imgWrap').classList.toggle('hidden', !src);
  const ctx = box.getContext('2d');
  ctx.clearRect(0, 0, box.width, box.height);
  if (src) {
    const draw = () => {
      box.width = img.naturalWidth; box.height = img.naturalHeight;
      const k = img.naturalWidth / (CAM.w || img.naturalWidth);
      ctx.lineWidth = Math.max(2, img.naturalWidth / 320);
      ctx.font = `${Math.max(11, img.naturalWidth / 45)}px system-ui`;
      for (const [u0, v0, u1, v1, c, kind] of boxes) {
        const col = kind === 'head' ? '#3fd6ff' : '#ffd23f';
        ctx.strokeStyle = col;
        ctx.strokeRect(u0 * k, v0 * k, (u1 - u0) * k, (v1 - v0) * k);
        if (kind === 'head') continue;
        const lab = `person ${(+c).toFixed(2)}`, tw = ctx.measureText(lab).width;
        const y = Math.max(16, v0 * k);
        ctx.fillStyle = col; ctx.fillRect(u0 * k, y - 15, tw + 6, 15);
        ctx.fillStyle = '#000'; ctx.fillText(lab, u0 * k + 3, y - 3);
      }
    };
    if (img.getAttribute('src') !== RUN + src) {
      img.onload = draw;
      img.src = RUN + src;
    } else draw();
  }
  dirty = true;
}

let frameById = new Map(), sightsByFrame = new Map();
let pending = null;
renderer.domElement.addEventListener('pointermove', (e) => {
  if (e.buttons) return;
  pending = [e.clientX, e.clientY];
});
let downAt = null;
renderer.domElement.addEventListener('pointerdown', (e) => { downAt = [e.clientX, e.clientY]; });
renderer.domElement.addEventListener('pointerup', (e) => {
  if (!downAt || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4 || e.button !== 0) return;
  const it = pick(e.clientX, e.clientY);
  S.pinned = it;
  showItem(it);
});
function hoverTick() {
  if (!pending) return;
  const [x, y] = pending;
  pending = null;
  const it = pick(x, y);
  renderer.domElement.style.cursor = it ? 'pointer' : '';
  const key = (o) => (o ? o.kind + o.i : '');
  if (S.pinned) return;
  if (key(it) !== key(S.hover)) { S.hover = it; showItem(it); }
}
$('infoClose').onclick = () => { S.pinned = S.hover = null; showItem(null); };

// ------------------------------------------------------------------------------------------
// views

function bounds() {
  const pts = track.length ? track : sights.length ? sights : [{ e: 0, n: 0, u: 0 }];
  let e0 = Infinity, n0 = Infinity, e1 = -Infinity, n1 = -Infinity, u = 0;
  for (const p of pts) { e0 = Math.min(e0, p.e); e1 = Math.max(e1, p.e); n0 = Math.min(n0, p.n); n1 = Math.max(n1, p.n); u += p.u; }
  return { c: new THREE.Vector3((e0 + e1) / 2, (n0 + n1) / 2, heightAt((e0 + e1) / 2, (n0 + n1) / 2)), w: Math.max(40, e1 - e0 + 40), h: Math.max(40, n1 - n0 + 40) };
}
function setView(kind) {
  const b = bounds(), asp = camera.aspect, f = Math.tan(THREE.MathUtils.degToRad(camera.fov / 2));
  const dist = (Math.max(b.h, b.w / asp) / 2 / f) * 1.1;
  const tgt = kind === 'keep' ? controls.target.clone() : b.c;
  controls.target.copy(tgt);
  if (kind === 'tilt') camera.position.set(tgt.x, tgt.y - dist * 0.75, tgt.z + dist * 0.7);
  else camera.position.set(tgt.x, tgt.y - dist * 0.0005, tgt.z + dist); // top-down, north up
  camera.near = Math.max(0.2, dist / 2000);
  camera.far = dist * 20 + 5000;
  camera.updateProjectionMatrix();
  controls.update();
  dirty = true;
}
$('topdown').onclick = () => setView('top');
$('tilt').onclick = () => setView('tilt');
$('fit').onclick = () => setView('top');

function resize() {
  const w = window.innerWidth, h = window.innerHeight;
  renderer.setSize(w, h);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  if (typeof placeSolver === 'function') placeSolver();
  dirty = true;
}
window.addEventListener('resize', resize);

// ------------------------------------------------------------------------------------------
// UI

function bindRange(id, key, fmt, after) {
  const el = $(id), out = $(id + 'V');
  const f = () => { S[key] = +el.value; out.textContent = fmt(S[key]); after && after(); applyTime(); };
  el.addEventListener('input', f);
  f();
}
function initUI() {
  bindRange('fade', 'fade', (v) => (v ? `${v} s` : 'off'));
  bindRange('msize', 'msize', (v) => `${v.toFixed(1)}×`, () => { sizeRibbon(); sizeTicks(); sizeSightings(); sizePersons(); sizeTruth(); solvDirty = true; if (S.pinned) showItem(S.pinned); });
  bindRange('opac', 'opac', (v) => `${v.toFixed(1)}×`);
  bindRange('fsize', 'fsize', (v) => `${v} m`, () => sizeFrustums());
  bindRange('popac', 'popac', (v) => v.toFixed(2));
  bindRange('prange', 'prange', (v) => `${v} m`, () => { if (photosBuilt) resetPhotos(); });
  bindRange('pmax', 'pmax', (v) => `${v}`, () => { if (photosBuilt) resetPhotos(); });
  $('photosOn').addEventListener('change', (e) => { if (e.target.checked) buildPhotos(); });
  $('bg').addEventListener('change', (e) => setBackground(e.target.value));
  for (const cb of document.querySelectorAll('[data-layer]')) {
    const k = cb.dataset.layer;
    const f = () => { S.layers[k] = cb.checked; groups[k].visible = cb.checked; dirty = true; };
    cb.addEventListener('change', f);
    f();
  }
  $('sframes').addEventListener('change', (e) => { S.sframes = e.target.checked; applyTime(); });
  const t0 = $('t0'), t1 = $('t1');
  for (const el of [t0, t1]) { el.max = S.tMax.toFixed(1); }
  t0.value = 0; t1.value = S.tMax;
  const f = (e) => {
    let a = +t0.value, b = +t1.value;
    if (a > b) { if (e && e.target === t0) a = b; else b = a; t0.value = a; t1.value = b; }
    S.t0 = a; S.t1 = b;
    applyTime();
  };
  t0.addEventListener('input', f); t1.addEventListener('input', f);
  $('play').onclick = togglePlay;
  window.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' && e.target.type !== 'range' && e.target.type !== 'checkbox') return;
    if (e.code === 'Space') { e.preventDefault(); togglePlay(); }
    if (e.key === 'Escape') { S.pinned = S.hover = null; showItem(null); }
    if (e.key === 't' || e.key === 'T') setView('top');
  });
  $('legT').textContent = `0:00 → ${fmtTs(S.tMax)}`;
}
let lastT = 0;
function togglePlay() {
  S.playing = !S.playing;
  if (S.playing && S.t1 >= S.tMax - 0.05) { S.t1 = S.t0; $('t1').value = S.t1; }
  $('play').textContent = S.playing ? '❚❚' : '▶';
  lastT = performance.now();
}
function playTick(now) {
  if (!S.playing) return;
  const dt = (now - lastT) / 1000;
  lastT = now;
  S.t1 = Math.min(S.tMax, S.t1 + dt * +$('speed').value);
  $('t1').value = S.t1;
  if (S.t1 >= S.tMax) togglePlay();
  applyTime();
}

// HTML labels follow their 3D anchors
function placeLabels() {
  const r = renderer.domElement.getBoundingClientRect();
  for (const L of labels) {
    let on = S.layers[L.layer];
    if (on && L.layer === 'ticks') on = ticks.find((k) => k.label === L)?.dot.visible;
    if (on && L.layer === 'persons') on = persons.find((p) => p.label === L)?.vis;
    if (on && L.layer === 'solved') on = L.show;
    const s = on ? screenOf(L.pos) : null;
    if (!s || s[0] < r.left || s[0] > r.right || s[1] < r.top || s[1] > r.bottom) { L.el.style.display = 'none'; continue; }
    L.el.style.display = '';
    L.el.style.left = `${s[0]}px`;
    L.el.style.top = `${s[1]}px`;
  }
}

function loop(now) {
  requestAnimationFrame(loop);
  playTick(now);
  hoverTick();
  if (controls.update()) dirty = true;
  if (solvDirty && now - (S.lastSolve || 0) > 60) { S.lastSolve = now; runSolver(); }
  if (!dirty) return;
  dirty = false;
  renderer.render(scene, camera);
  placeLabels();
  drawScale();
}

// ------------------------------------------------------------------------------------------

async function main() {
  meta = (await get('meta.json', 'json')) || {};
  const [tr, fr, si, pe, tu] = await Promise.all(['track', 'frames', 'sightings', 'persons', 'truth'].map((n) => get(n + '.csv')));
  track = parseCSV(tr).filter((r) => r.e != null).sort((a, b) => a.t - b.t);
  frames = parseCSV(fr).filter((r) => hasCam(r));
  sights = parseCSV(si).filter((r) => r.e != null);
  persons = parseCSV(pe).filter((r) => r.e != null);
  truth = parseCSV(tu).filter((r) => r.e != null);
  if (!track.length && !sights.length) {
    $('err').textContent = `No track or sightings in ${RUN} (is it a flight run-log? see README.md)`;
    $('err').classList.remove('hidden');
  }
  Object.assign(CAM, meta.camera || {});
  frameById = new Map(frames.map((f) => [f.id, f]));
  for (const s of sights) if (s.frame != null) { if (!sightsByFrame.has(s.frame)) sightsByFrame.set(s.frame, []); sightsByFrame.get(s.frame).push(s); }
  S.tMax = Math.max(1, ...[track.at(-1)?.t, sights.at(-1)?.t, frames.at(-1)?.t].filter((v) => v != null));
  S.t1 = S.tMax;
  await buildTerrain();
  if (terr?.source && terr.source !== 'auto') $('bg').value = terr.source;
  SP.reTerrain = defaultReTerrain();
  prepSolver();
  bindSolverUI();
  buildPath(); buildTicks(); buildDrone(); buildFrustums(); buildSightings(); buildPersons(); buildTruth();
  $('title').textContent = `${meta.world || (meta.source === 'real' ? 'Real flight' : 'Flight')} · ${meta.start_time ? meta.start_time.replace('T', ' ').slice(0, 19) : ''}`;
  setSub();
  document.title = `Flight Viewer · ${meta.world || 'run'}`;
  initUI();
  resize();
  setView(Q.get('view') === 'tilt' ? 'tilt' : 'top');
  if (Q.has('t1')) { S.t1 = +Q.get('t1'); $('t1').value = S.t1; applyTime(); }
  if (Q.has('photos')) { $('photosOn').checked = true; $('photosOn').dispatchEvent(new Event('change')); }
  if (Q.has('nosolver')) $('solvFold').click();
  runSolver();
  const sel = Q.get('select'); // e.g. s:12, f:3, p:0, t:1, q:2 (testing / links)
  if (sel) { const [k, i] = sel.split(':'); S.pinned = { kind: k, i: +i }; showItem(S.pinned); }
  requestAnimationFrame(loop);
  window.FV = { S, SP, SV, setView, showItem, pick, applyTime, camera, controls, setBackground, prepSolver, solved: () => solved, solvScore: () => solvScore,
    solve: (over = {}) => { const P = { ...SP, ...over, w: { ...SP.w, ...(over.w || {}) } }; const r = SV.solve(prep, P, S.t0, S.t1); return { ...SV.score(r.persons, truth, P.matchM), per: undefined, n: r.persons.length }; },
    data: { track, frames, sights, persons, truth }, ready: true, counts: { track: track.length, frames: frames.length, sights: sights.length, persons: persons.length, truth: truth.length } };
}
function setSub() {
  const run = meta.search_test_out ? meta.search_test_out.split('/').pop() : '';
  const pinged = persons.filter((p) => p.pinged).length;
  const pos = meta.position ? `position: ${meta.position.source}${meta.position.global === false ? ' (local only)' : ''}` : '';
  $('sub').innerHTML = [
    `${meta.source || '?'} · ${fmtTs(S.tMax)} · ${track.length} track pts · ${frames.length} frames · ${sights.length} sightings · ${pinged} persons${persons.length > pinged ? ` (+${persons.length - pinged} candidates)` : ''}`,
    terr ? `background: ${terr.kind}${terr.anchored ? ' (anchored)' : ''} · ${terr.texture || ''}${terr.attribution ? ' · ' + terr.attribution : ''}` : 'no terrain',
    [pos, run ? `search_test: ${run}` : ''].filter(Boolean).join(' · '),
  ].filter(Boolean).join('<br>');
  placeSolver();
}
function placeSolver() { if (window.innerWidth > 760) $('solver').style.top = `${$('head').getBoundingClientRect().bottom + 8}px`; }
main().catch((e) => { console.error(e); $('err').textContent = 'Viewer error: ' + e.message; $('err').classList.remove('hidden'); });
