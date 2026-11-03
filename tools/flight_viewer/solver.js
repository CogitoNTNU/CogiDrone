// Post-flight person position solver (no three.js: plain arrays, testable on its own).
//
// Every sighting gives, from the camera centre c along the unit feet ray d (box bottom centre):
//   ray      a line constraint: the person is somewhere on c + s d       (multi-view triangulation)
//   terrain  where the feet ray meets the terrain (logged, or re-intersected with the viewer map)
//   head     head height * fy / head px   (Muhammad's depth_head.py), range to the head
//   body     person height * fy / box px  (same file, the whole-body variant), range to the body
//   depth    depth-camera median range to the body (valid to ~10 m)
// Ranges to the head / body centre are moved to the feet along the ray for a standing person
// (r_feet = r_head - 1.6 d_up, r_body - 0.85 d_up; d_up < 0 looking down).
// Each estimate is a 3x3 information matrix: along-ray 1/sigma_a^2, across 1/sigma_c^2
// (sigma_c = range * angular noise); a ray has only the across part. The user weights scale
// these. A person = weighted least squares over its sightings, a few IRLS rounds (Huber),
// sightings are clustered on their own fused estimates, then re-assigned to the solved points.

export const DEFAULTS = {
  w: { ray: 1, terrain: 1, head: 0.3, body: 0.2, depth: 3 },
  clusterM: 4, minSight: 3, minConf: 0.4, headMinPx: 6, headH: 0.23, bodyH: 1.7,
  pxSigma: 3, attDeg: 0.5, terrainSigma: 1.0, relHead: 0.2, relBody: 0.15, relDepth: 0.03,
  reTerrain: false, maxRange: 150, matchM: 5, useTime: true,
};

// --- small linear algebra (row-major 3x3 as arrays of 9) ------------------------------------
const Z9 = () => [0, 0, 0, 0, 0, 0, 0, 0, 0];
function addOuter(M, d, a, b) { // M += a*I + (b - a) d d^T  (b along d, a across)
  for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) M[3 * i + j] += (i === j ? a : 0) + (b - a) * d[i] * d[j];
}
function mulV(M, v) { return [0, 1, 2].map((i) => M[3 * i] * v[0] + M[3 * i + 1] * v[1] + M[3 * i + 2] * v[2]); }
function inv3(m) {
  const [a, b, c, d, e, f, g, h, i] = m;
  const A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
  const det = a * A + b * B + c * C;
  if (!isFinite(det) || Math.abs(det) < 1e-18) return null;
  const k = 1 / det;
  return [A * k, -(b * i - c * h) * k, (b * f - c * e) * k, B * k, (a * i - c * g) * k, -(a * f - c * d) * k,
    C * k, -(a * h - b * g) * k, (a * e - b * d) * k];
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const norm = (a) => Math.hypot(a[0], a[1], a[2]);
const hyp2 = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);

export function quatRot(qw, qx, qy, qz, v) { // rotate v by unit quaternion (w, x, y, z)
  const ix = qw * v[0] + qy * v[2] - qz * v[1], iy = qw * v[1] + qz * v[0] - qx * v[2];
  const iz = qw * v[2] + qx * v[1] - qy * v[0], iw = -qx * v[0] - qy * v[1] - qz * v[2];
  return [ix * qw + iw * -qx + iy * -qz - iz * -qy, iy * qw + iw * -qy + iz * -qx - ix * -qz,
    iz * qw + iw * -qz + ix * -qy - iy * -qx];
}
export function pixelRay(s, cam, u, v) {
  const d = quatRot(s.cam_qw, s.cam_qx, s.cam_qy, s.cam_qz, [(u - cam.cx) / cam.fx, (v - cam.cy) / cam.fy, 1]);
  const n = norm(d);
  return [d[0] / n, d[1] / n, d[2] / n];
}

// ray -> terrain: march + bisection on heightAt(e, n); null = no hit within maxR
export function rayTerrain(c, d, heightAt, maxR = 150) {
  if (!heightAt) return null;
  let t0 = 0, t = 0.5;
  if (c[2] - heightAt(c[0], c[1]) < 0) return null;
  while (t <= maxR) {
    const p = [c[0] + d[0] * t, c[1] + d[1] * t, c[2] + d[2] * t];
    if (p[2] <= heightAt(p[0], p[1])) {
      let a = t0, b = t;
      for (let k = 0; k < 12; k++) {
        const m = (a + b) / 2, q = [c[0] + d[0] * m, c[1] + d[1] * m, c[2] + d[2] * m];
        if (q[2] <= heightAt(q[0], q[1])) b = m; else a = m;
      }
      return b;
    }
    t0 = t;
    t += Math.max(0.5, t * 0.02);
  }
  return null;
}

// Per-sighting geometry, computed once (and again when the terrain changes).
export function prepare(sights, cam, heightAt, P) {
  const out = [];
  for (let i = 0; i < sights.length; i++) {
    const s = sights[i];
    if (s.cam_qw == null || s.cam_e == null) continue;
    const c = [s.cam_e, s.cam_n, s.cam_u];
    const df = s.ray_f_e != null ? [s.ray_f_e, s.ray_f_n, s.ray_f_u] : pixelRay(s, cam, (s.u0 + s.u1) / 2, s.v1);
    const g = { i, s, c, d: df, t: s.t, conf: s.conf ?? 1, boxH: Math.max(1, s.v1 - s.v0) };
    // logged terrain hit (v2 gnd_*, v1: the position of a 'ground' sighting)
    let gp = s.gnd_e != null ? [s.gnd_e, s.gnd_n, s.gnd_u] : s.method === 'ground' ? [s.e, s.n, s.u] : null;
    g.rTerrLog = gp ? Math.max(0.5, dot(sub(gp, c), df)) : null;
    g.rTerrMap = P.reTerrain || g.rTerrLog == null ? rayTerrain(c, df, heightAt, P.maxRange) : null;
    g.dDepth = s.d_depth ?? (s.method === 'depth' ? s.range_m : null);
    g.headPx = s.head_px ?? null;
    g.headConf = s.head_conf ?? null;
    g.logged = [s.e, s.n, s.u];
    out.push(g);
  }
  return out;
}

// Constraints of one sighting: [{src, p, d, a (across info), b (along info)}]
function constraints(g, P, rGuess) {
  const W = P.w, out = [], up = g.d[2];
  const angVar = (P.pxSigma / P.fy) ** 2 + (P.attDeg * Math.PI / 180) ** 2;
  const across = (r) => 1 / (Math.max(0.3, r) ** 2 * angVar);
  const pt = (src, w, rf, sa) => {
    if (!(w > 0) || !(rf > 0.3) || !isFinite(rf)) return;
    out.push({ src, p: [g.c[0] + g.d[0] * rf, g.c[1] + g.d[1] * rf, g.c[2] + g.d[2] * rf], d: g.d, r: rf,
      a: w * across(rf), b: w / (sa * sa), w });
  };
  const rT = P.reTerrain && g.rTerrMap != null ? g.rTerrMap : g.rTerrLog ?? g.rTerrMap;
  if (rT != null) pt('terrain', W.terrain, rT, Math.min(50, P.terrainSigma / Math.max(0.05, -up)) + 0.02 * rT);
  if (g.headPx != null && g.headPx >= P.headMinPx) {
    const rh = (P.headH * P.fy) / g.headPx - 1.6 * up;
    pt('head', W.head, rh, P.relHead * rh + 0.2);
  }
  const rb = (P.bodyH * P.fy) / g.boxH - 0.85 * up;
  pt('body', W.body, rb, P.relBody * rb + 0.2);
  if (g.dDepth != null) { const rd = g.dDepth - 0.85 * up; pt('depth', W.depth, rd, P.relDepth * rd + 0.05); }
  if (W.ray > 0) { const r = rGuess ?? (out[0]?.r ?? 30); out.push({ src: 'ray', p: g.c, d: g.d, r, a: W.ray * across(r), b: 0, w: W.ray }); }
  return out;
}

// Weighted least squares over constraints (+ a weak prior), IRLS with a Huber weight.
function wls(cons, prior, priorSigma = 30, rounds = 3) {
  let x = prior.slice(), H, ok = true, chi = 0;
  const hub = cons.map(() => 1);
  for (let it = 0; it < rounds; it++) {
    H = Z9(); const rhs = [0, 0, 0];
    const pw = 1 / priorSigma ** 2;
    addOuter(H, [0, 0, 1], pw, pw);
    for (let k = 0; k < 3; k++) rhs[k] += pw * prior[k];
    cons.forEach((q, j) => {
      const M = Z9();
      addOuter(M, q.d, q.a * hub[j], q.b * hub[j]);
      for (let k = 0; k < 9; k++) H[k] += M[k];
      const mp = mulV(M, q.p);
      for (let k = 0; k < 3; k++) rhs[k] += mp[k];
    });
    const Hi = inv3(H);
    if (!Hi) { ok = false; break; }
    x = mulV(Hi, rhs);
    chi = 0;
    cons.forEach((q, j) => { // normalised residual -> Huber weight (k = 2.5)
      const e = sub(x, q.p), al = dot(e, q.d), ac = Math.max(0, dot(e, e) - al * al);
      const m2 = q.a * ac + q.b * al * al;
      chi += m2 * hub[j];
      const m = Math.sqrt(m2 / q.w); // without the user weight: outliers by their own sigma
      hub[j] = m <= 2.5 ? 1 : 2.5 / m;
    });
  }
  return { x, H, ok, chi, hub };
}

function fuseOne(g, P) {
  const cons = constraints(g, P, null);
  if (!cons.some((q) => q.b > 0)) return g.logged.slice(); // ray only: keep the flight's estimate
  return wls(cons, cons.find((q) => q.b > 0).p, 30, 2).x;
}

function solveCluster(members, P) {
  const est = members.map((g) => g.fused);
  const m0 = [0, 1, 2].map((k) => est.reduce((a, e) => a + e[k], 0) / est.length);
  let cons = [], owner = [];
  for (const g of members) for (const q of constraints(g, P, norm(sub(m0, g.c)))) { cons.push(q); owner.push(g); }
  const r = wls(cons, m0, 20, 4);
  if (!r.ok) return { x: m0, cov: null, cons, owner, hub: cons.map(() => 1) };
  // covariance, scaled by the reduced chi^2 when the sightings disagree more than their sigmas
  let dof = -3;
  for (const q of cons) dof += q.b > 0 ? 3 : 2;
  const s2 = dof > 0 ? Math.max(1, r.chi / dof) : 1;
  // sightings of one pass share their errors (pose, attitude, terrain, posture): count at most
  // ~4 independent looks, or the ellipse shrinks to centimetres with 100 sightings
  const C = inv3(r.H).map((v) => v * s2 * Math.max(1, members.length / 4));
  return { x: r.x, cov: C, cons, owner, hub: r.hub };
}

// Horizontal 1-sigma ellipse of a 3x3 covariance: {a, b, ang (rad, from east)}
export function ellipse(C) {
  if (!C) return { a: 1, b: 1, ang: 0 };
  const xx = C[0], xy = C[1], yy = C[4], tr = (xx + yy) / 2, df = Math.sqrt(((xx - yy) / 2) ** 2 + xy * xy);
  return { a: Math.sqrt(Math.max(1e-6, tr + df)), b: Math.sqrt(Math.max(1e-6, tr - df)), ang: 0.5 * Math.atan2(2 * xy, xx - yy) };
}

export function solve(prep, P, t0 = -Infinity, t1 = Infinity) {
  const use = prep.filter((g) => g.conf >= P.minConf && (!P.useTime || (g.t >= t0 - 1e-6 && g.t <= t1 + 1e-6)));
  for (const g of use) g.fused = fuseOne(g, P);
  // 1) greedy clustering in time order on the fused estimates (like the flight: running mean)
  let cl = [];
  for (const g of [...use].sort((a, b) => a.t - b.t)) {
    let best = null, bd = Infinity;
    for (const c of cl) { const d = hyp2(c.m, g.fused); if (d < bd) { bd = d; best = c; } }
    if (best && bd <= P.clusterM) {
      best.g.push(g);
      const n = best.g.length;
      best.m = best.m.map((v, k) => v + (g.fused[k] - v) / n);
    } else cl.push({ g: [g], m: g.fused.slice() });
  }
  // 2) solve, 3) re-assign every sighting to the nearest solved point (within the radius), solve again
  for (let pass = 0; pass < 2; pass++) {
    const sol = cl.map((c) => solveCluster(c.g, P));
    if (pass === 1) { cl.forEach((c, k) => Object.assign(c, sol[k])); break; }
    const next = sol.map((s) => ({ g: [], m: s.x }));
    for (const g of use) {
      let best = null, bd = Infinity;
      for (const c of next) { const d = hyp2(c.m, g.fused); if (d < bd) { bd = d; best = c; } }
      if (best && bd <= P.clusterM) best.g.push(g);
      else next.push({ g: [g], m: g.fused.slice() });
    }
    cl = next.filter((c) => c.g.length);
  }
  const persons = cl.map((c, k) => {
    const n = c.g.length, srcN = {};
    for (const q of c.cons) srcN[q.src] = (srcN[q.src] || 0) + 1;
    return {
      id: k, e: c.x[0], n: c.x[1], u: c.x[2], n_sight: n, pinged: n >= P.minSight, ell: ellipse(c.cov), cov: c.cov,
      conf_max: Math.max(...c.g.map((g) => g.conf)), t_first: Math.min(...c.g.map((g) => g.t)), t_last: Math.max(...c.g.map((g) => g.t)),
      members: c.g, cons: c.cons, owner: c.owner, hub: c.hub, src: srcN,
    };
  }).sort((a, b) => a.t_first - b.t_first);
  persons.forEach((p, k) => { p.id = k; });
  return { persons, used: use.length };
}

// Against sim truth: every truth -> nearest pinged person (found within matchM)
export function score(persons, truth, matchM = 5) {
  const pp = persons.filter((p) => p.pinged);
  const errs = [], per = [];
  for (const t of truth) {
    let bd = Infinity, bp = null;
    for (const p of pp) { const d = Math.hypot(p.e - t.e, p.n - t.n); if (d < bd) { bd = d; bp = p; } }
    per.push({ id: t.id, err: bd, person: bp?.id });
    if (bd <= matchM) errs.push(bd);
  }
  for (const p of pp) {
    let bd = Infinity, bt = null;
    for (const t of truth) { const d = Math.hypot(p.e - t.e, p.n - t.n); if (d < bd) { bd = d; bt = t; } }
    p.truth = bt?.id ?? null; p.err = bd;
  }
  const s = [...errs].sort((a, b) => a - b);
  return {
    found: errs.length, total: truth.length, persons: pp.length,
    falsePos: pp.filter((p) => !(p.err <= matchM)).length,
    mean: s.length ? s.reduce((a, b) => a + b, 0) / s.length : null,
    median: s.length ? s[Math.floor((s.length - 1) / 2)] : null, max: s.length ? s[s.length - 1] : null, per,
  };
}

// PX4's azimuthal equidistant projection (local north/east -> lat/lon)
export function nedToLatLon(x, y, lat0, lon0) {
  const R = 6371000, la0 = lat0 * Math.PI / 180, xr = x / R, yr = y / R, c = Math.hypot(xr, yr);
  if (c === 0) return [lat0, lon0];
  const sc = Math.sin(c), cc = Math.cos(c);
  const lat = Math.asin(cc * Math.sin(la0) + (xr * sc * Math.cos(la0)) / c);
  const lon = lon0 * Math.PI / 180 + Math.atan2(yr * sc, c * Math.cos(la0) * cc - xr * Math.sin(la0) * sc);
  return [lat * 180 / Math.PI, lon * 180 / Math.PI];
}
