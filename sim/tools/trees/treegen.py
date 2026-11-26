#!/usr/bin/env python3
"""Generate the tree library: near-tier meshes (bark + alpha-card foliage), far-tier impostors.

Every tree is Z-up, in metres, origin at the trunk base, normalised to a height of 1.0 m
(the forest builder scales each instance by its height). Run textures.py first.

Outputs (sim/assets/trees/):
  meshes/<species>_<v>.glb       near tier, 2 primitives (bark, leaf), for single use / inspection
  meshes/<species>_<v>_far.glb   far tier (crossed impostor cards)
  textures/far.png               impostor atlas baked here with raster.py
  library.npz, trees.json        raw arrays + metadata read by forest_builder.py
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

from common import ASSET_DIR, MESH_DIR, TEX_DIR
from glb import write_glb
from raster import downsample, rasterize

ATLAS = json.load(open(os.path.join(ASSET_DIR, "atlas.json")))
FW, FH = ATLAS["foliage_size"]
UP = np.array([0.0, 0.0, 1.0])

SPECIES = {"spruce": 3, "pine": 3, "birch": 3, "alder": 2, "juniper": 2}

# glTF materials used by the per-tree glbs (textures referenced relative to meshes/)
MATERIALS = {
    "bark": {"image": "../textures/bark.jpg", "alpha": "OPAQUE", "double_sided": False, "roughness": 0.95},
    "leaf": {"image": "../textures/foliage.png", "alpha": "MASK", "double_sided": True, "roughness": 0.8},
    "far": {"image": "../textures/far.png", "alpha": "MASK", "double_sided": True, "roughness": 0.9},
}


def unit(v):
    v = np.asarray(v, np.float64)
    n = np.linalg.norm(v, axis=-1, keepdims=True)
    return v / np.maximum(n, 1e-12)


def frect(name):
    """Sprite rect -> (u0, v0, u1, v1) in glTF uv (v down), inset by one texel."""
    x, y, w, h = ATLAS["foliage"][name]
    return ((x + 1.5) / FW, (y + 1.5) / FH, (x + w - 1.5) / FW, (y + h - 1.5) / FH)


class Mesh:
    def __init__(self):
        self.parts = {"bark": [], "leaf": []}

    def add(self, mat, pos, nrm, uv, idx):
        self.parts[mat].append((np.asarray(pos, np.float64), np.asarray(nrm, np.float64),
                                np.asarray(uv, np.float64), np.asarray(idx, np.int64)))

    def arrays(self, mat):
        P, N, U, I = [], [], [], []
        off = 0
        for p, n, u, i in self.parts[mat]:
            P.append(p); N.append(n); U.append(u); I.append(i + off)
            off += len(p)
        if not P:
            return np.zeros((0, 3)), np.zeros((0, 3)), np.zeros((0, 2)), np.zeros((0, 3), np.int64)
        return np.concatenate(P), np.concatenate(N), np.concatenate(U), np.concatenate(I)

    # ------------------------------------------------------------------ primitives
    def tube(self, pts, radii, sides, species, v0=None, v1=None):
        """Generalised cylinder along pts. Bark uv: column of `species`, v from height."""
        pts = np.asarray(pts, np.float64)
        K = len(pts)
        tang = np.gradient(pts, axis=0)
        tang = unit(tang)
        ref = np.array([1.0, 0, 0]) if abs(tang[0, 2]) > 0.9 else UP
        n = unit(np.cross(tang[0], ref))
        frames = []
        for k in range(K):
            n = unit(n - tang[k] * np.dot(n, tang[k]))
            frames.append((n, np.cross(tang[k], n)))
        u0, u1 = ATLAS["bark_u"][species]
        u0 += 1.0 / 1024; u1 -= 1.0 / 1024
        if v0 is None:
            vv = np.clip(pts[:, 2], 0, 1)
        else:
            seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))]
            vv = v0 + (v1 - v0) * seg / max(seg[-1], 1e-9)
        P, N, U = [], [], []
        for k in range(K):
            a, b = frames[k]
            for j in range(sides + 1):
                ang = 2 * math.pi * j / sides
                d = math.cos(ang) * a + math.sin(ang) * b
                P.append(pts[k] + radii[k] * d); N.append(d)
                U.append((u0 + (u1 - u0) * j / sides, 1.0 - 0.998 * vv[k]))
        idx = []
        for k in range(K - 1):
            for j in range(sides):
                a = k * (sides + 1) + j; b = a + sides + 1
                idx += [(a, a + 1, b + 1), (a, b + 1, b)]
        self.add("bark", P, N, U, idx)

    def strip(self, pts, side, width, sprite, tcenter=0.5):
        """Card bent along pts; sprite s axis (left->right) follows pts, t axis (top->bottom)
        follows `side` vectors (K,3). width = card width (world)."""
        pts = np.asarray(pts); side = unit(np.asarray(side))
        u0, v0, u1, v1 = frect(sprite)
        K = len(pts)
        seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(pts, axis=0), axis=1))]
        s = seg / seg[-1]
        tang = unit(np.gradient(pts, axis=0))
        P, N, U = [], [], []
        for k in range(K):
            cn = unit(np.cross(tang[k], side[k]))
            for t in (0.0, 1.0):
                P.append(pts[k] + (t - tcenter) * width * side[k]); N.append(cn)
                U.append((u0 + (u1 - u0) * s[k], v0 + (v1 - v0) * t))
        idx = []
        for k in range(K - 1):
            a = 2 * k
            idx += [(a, a + 2, a + 3), (a, a + 3, a + 1)]
        self.add("leaf", P, N, U, idx)

    def quad(self, anchor_pos, ax_r, ax_up, sprite, anchor=(0.5, 0.5)):
        """Flat card. ax_r spans the sprite width (left->right), ax_up its height (bottom->top).
        `anchor` (sx, sy; sprite coords, y down) is placed at anchor_pos."""
        u0, v0, u1, v1 = frect(sprite)
        ax_r = np.asarray(ax_r, np.float64); ax_up = np.asarray(ax_up, np.float64)
        cn = unit(np.cross(ax_r, ax_up))
        P, U = [], []
        for sx, sy in ((0, 0), (1, 0), (1, 1), (0, 1)):
            P.append(anchor_pos + (sx - anchor[0]) * ax_r + (anchor[1] - sy) * ax_up)
            U.append((u0 + (u1 - u0) * sx, v0 + (v1 - v0) * sy))
        self.add("leaf", P, [cn] * 4, U, [(0, 1, 2), (0, 2, 3)])


def horiz(az):
    return np.array([math.cos(az), math.sin(az), 0.0])


def trunk_path(rng, top, wobble, n=8, lean=(0.0, 0.0)):
    z = np.linspace(0, top, n) ** 1.0
    z = np.r_[0, 0.02, np.linspace(0.06, top, n - 2)]
    off = np.cumsum(rng.normal(0, wobble, (n, 2)), axis=0)
    off -= off[0]
    off[:, 0] += lean[0] * z; off[:, 1] += lean[1] * z
    return np.c_[off, z]


def along(pts, z):
    """Point on a (monotone in z) polyline at height z."""
    return np.array([np.interp(z, pts[:, 2], pts[:, 0]), np.interp(z, pts[:, 2], pts[:, 1]), z])


# ====================================================================== species
def spruce(v):
    """Norway spruce: narrow dark cone of drooping branch sprays, upturned tips, bare lower
    trunk with dead branch stubs in stand-grown variants."""
    rng = np.random.default_rng(100 + v)
    m = Mesh()
    hc = [0.20, 0.36, 0.50][v]
    Rm = [0.18, 0.16, 0.14][v]
    tp = trunk_path(rng, 0.97, 0.0015, 9)
    rad = 0.0105 * (1 - tp[:, 2]) ** 0.85 + 0.006 * np.exp(-tp[:, 2] / 0.012)
    m.tube(tp, rad, 7, "spruce")
    nw = 22
    zs = hc + (0.965 - hc) * ((np.arange(nw) + rng.random(nw) * 0.5) / nw) ** 1.05
    for zw in zs:
        t = (zw - hc) / (1 - hc)
        L = Rm * (1 - t) ** 0.9 * (0.82 + 0.18 * min(1.0, t / 0.1)) + 0.012
        nb = 6 if t < 0.65 else 5
        a0 = rng.uniform(0, 2 * math.pi)
        base_c = along(tp, zw)
        elev = math.radians(-24 + 50 * t ** 1.4)
        for b in range(nb):
            az = a0 + 2 * math.pi * b / nb + rng.normal(0, 0.22)
            Lb = L * rng.uniform(0.85, 1.12)
            d = horiz(az)
            perp = horiz(az + math.pi / 2)
            s = np.array([0.0, 0.5, 1.0])
            droop = 0.28 * (1 - t) * Lb
            upturn = 0.16 * (1 - t) * Lb
            pts = base_c + d * 0.004 + np.outer(s * Lb * math.cos(elev), d) + np.outer(
                s * Lb * math.sin(elev) - droop * s ** 2 + upturn * s ** 3, UP)
            spr = f"spruce_spray{rng.integers(3)}"
            for roll in (math.radians(32 + rng.normal(0, 6)), math.radians(-32 + rng.normal(0, 6))):
                side = perp * math.cos(roll) + UP * math.sin(roll)
                m.strip(pts, np.tile(side, (3, 1)), Lb * 0.42, spr)
    # dense inner core (3 crossed cards) so the crown is never see-through
    for k in range(3):
        a = k * math.pi / 3 + rng.uniform(0, 0.5)
        m.quad(np.array([0, 0, hc + 0.02]), horiz(a) * 2 * Rm * 0.62, UP * (0.94 - hc), "spruce_core", (0.5, 1.0))
    for k in range(2):
        a = k * math.pi / 2 + 0.3
        m.quad(along(tp, 0.90), horiz(a) * 0.05, UP * 0.115, "spruce_leader", (0.5, 1.0))
    # dead branches on the bare stem
    for zd in np.arange(0.07, hc - 0.02, 0.03):
        az = rng.uniform(0, 2 * math.pi)
        L = rng.uniform(0.03, 0.07) * (0.5 + zd / max(hc, 0.1))
        d = horiz(az)
        pts = along(tp, zd) + np.outer([0, 1], d * L) + np.outer([0, -0.2 * L], UP)
        roll = rng.uniform(-1, 1)
        side = horiz(az + math.pi / 2) * math.cos(roll) + UP * math.sin(roll)
        m.strip(pts, np.tile(side, (2, 1)), L * 0.25, "dead_branch")
    return m, dict(crown_base=hc, crown_radius=Rm, crown_center=(hc + 1) / 2, trunk_r=0.0105)


def pine_clump(m, rng, P, S, spr):
    """Needle tuft: one up-facing tilted card + two vertical crossed cards."""
    a = rng.uniform(0, 2 * math.pi)
    tilt = math.radians(rng.uniform(10, 30))
    r = horiz(a)
    f = horiz(a + math.pi / 2) * math.cos(tilt) + UP * math.sin(tilt)
    m.quad(P + UP * S * 0.08, r * S * 1.1, f * S * 1.1, spr, (0.5, 0.55))
    for k in range(2):
        b = a + math.pi / 4 + k * math.pi / 2
        m.quad(P - UP * S * 0.25, horiz(b) * S, UP * S * 0.9, spr, (0.5, 0.78))


def pine(v):
    """Scots pine: long straight bare trunk (orange upper bark), few strong limbs carrying
    clumpy needle tufts; v0 younger rounded-conical crown, v1 old flat umbrella, v2 windswept."""
    rng = np.random.default_rng(200 + v)
    m = Mesh()
    hc = [0.42, 0.55, 0.50][v]
    R = [0.17, 0.22, 0.20][v]
    top = [0.96, 0.93, 0.92][v]
    tp = trunk_path(rng, top, [0.004, 0.006, 0.01][v], 8)
    rad = 0.0115 * (1 - tp[:, 2] / 1.08) ** 0.9 + 0.005 * np.exp(-tp[:, 2] / 0.012)
    m.tube(tp, rad, 6, "pine")
    tufts = [f"pine_tuft{i}" for i in range(3)]
    nl = [20, 17, 19][v]
    wind = rng.uniform(0, 2 * math.pi)
    clumps = []
    for i in range(nl):
        zs = hc + (top - 0.04 - hc) * (i + rng.random() * 0.7) / nl
        az = i * 2.39996 + rng.normal(0, 0.3)
        if v == 2 and rng.random() < 0.6:  # windswept: most limbs on the lee side
            az = wind + rng.normal(0, 0.6)
        S0 = along(tp, zs)
        frac = (zs - hc) / (top - hc)
        if v == 0:
            rt = R * (1.05 - 0.75 * frac) * rng.uniform(0.75, 1.05)
            zt = zs + rng.uniform(0.04, 0.09)
        elif v == 1:
            rt = R * rng.uniform(0.55, 1.0) * (1.0 - 0.35 * frac)
            zt = max(zs + 0.05, rng.uniform(0.88, 0.97))
        else:
            lee = 0.5 + 0.5 * math.cos(az - wind)
            rt = R * rng.uniform(0.5, 0.95) * (0.6 + 0.6 * lee)
            zt = max(zs + 0.04, rng.uniform(0.84, 0.95))
        tip = np.array([S0[0] + rt * math.cos(az), S0[1] + rt * math.sin(az), zt])
        mid = S0 + 0.5 * (tip - S0) + UP * 0.02
        pts = np.array([S0, mid, tip])
        L = np.linalg.norm(tip - S0)
        m.tube(pts, [0.0035 * (0.5 + L / R), 0.0022, 0.001], 3, "pine", 0.75, 0.95)
        clumps.append(tip)
        for s in (0.3, 0.45, 0.6, 0.75, 0.9):
            clumps.append(pts[0] + s * (tip - pts[0]) + UP * 0.015 + rng.normal(0, 0.014, 3))
        for k in range(3):  # sub-limbs
            s = rng.uniform(0.4, 0.75)
            p = pts[0] + s * (tip - pts[0])
            d = unit(horiz(az + rng.choice([-1, 1]) * rng.uniform(0.5, 1.1)) + UP * rng.uniform(0.2, 0.7))
            q = p + d * L * rng.uniform(0.3, 0.45)
            m.tube(np.array([p, q]), [0.0016, 0.0007], 3, "pine", 0.8, 0.95)
            clumps.append(q)
            clumps.append(p + 0.5 * (q - p) + rng.normal(0, 0.01, 3))
    for k in range(5):
        clumps.append(np.array(along(tp, top)) + np.r_[rng.normal(0, 0.02, 2), rng.uniform(-0.04, 0.03)])
    S = R * 0.62
    for P in clumps:
        pine_clump(m, rng, np.asarray(P), S * rng.uniform(0.8, 1.15), tufts[rng.integers(3)])
    # a few dead branch stubs
    for zd in np.arange(0.25, hc - 0.03, 0.06):
        if rng.random() < 0.5:
            continue
        az = rng.uniform(0, 2 * math.pi)
        L = rng.uniform(0.03, 0.05)
        d = horiz(az)
        pts = along(tp, zd) + np.outer([0, 1], d * L) + np.outer([0, 0.1 * L], UP)
        m.strip(pts, np.tile(horiz(az + 1.57), (2, 1)), L * 0.25, "dead_branch")
    zc = (hc + 1) / 2 + 0.05
    return m, dict(crown_base=hc, crown_radius=R, crown_center=zc, trunk_r=0.0115)


def leafy_clump(m, rng, P, S, spr):
    """Broadleaf spray: one vertical card (twigs hang down from the anchor) crossed with one
    card tilted ~55 deg towards horizontal, so the crown is full from the side and from above."""
    a = rng.uniform(0, 2 * math.pi)
    tilt = rng.normal(0, 0.2)
    m.quad(P, horiz(a) * S, unit(UP + horiz(a + math.pi / 2) * tilt) * S, spr, (0.5, 0.4))
    b = a + math.pi / 2 + rng.normal(0, 0.3)
    lay = math.radians(rng.uniform(45, 65))
    upv = UP * math.cos(lay) + horiz(b + math.pi / 2) * math.sin(lay)
    m.quad(P, horiz(b) * S, upv * S, spr, (0.5, 0.45))


def broadleaf(m, rng, tp, hc, R, nlimb, spr, S, species, weep, top_z, clumps_per=5, elev=(50, 70), lenf=1.2):
    for i in range(nlimb):
        zs = hc + (top_z - 0.06 - hc) * (i + rng.random() * 0.8) / nlimb
        az = i * 2.39996 + rng.normal(0, 0.25)
        frac = (zs - hc) / (top_z - hc)
        L = R * lenf * (1 - frac) ** 0.5 * rng.uniform(0.8, 1.1) + 0.04
        e = math.radians(rng.uniform(*elev) + 10 * frac)
        S0 = along(tp, zs)
        d = horiz(az)
        p1 = S0 + (d * math.cos(e) + UP * math.sin(e)) * L * 0.5
        p2 = p1 + (d * math.cos(e * 0.45) + UP * math.sin(e * 0.45)) * L * 0.5 - UP * weep * L
        pts = np.array([S0, p1, p2])
        m.tube(pts, [0.0032 * (0.5 + L / R), 0.0018, 0.0008], 3, species, 0.6, 0.9)
        for c in range(clumps_per):
            s = 0.35 + 0.65 * (c + rng.random() * 0.5) / clumps_per
            q = pts[0] + (pts[1] - pts[0]) * min(1, 2 * s) + (pts[2] - pts[1]) * max(0, 2 * s - 1)
            leafy_clump(m, rng, q + rng.normal(0, 0.012, 3), S * rng.uniform(0.85, 1.15), spr[rng.integers(len(spr))])


def birch(v):
    """Birch: white stem with black base, steep limbs with drooping tips, light open oval crown."""
    rng = np.random.default_rng(300 + v)
    m = Mesh()
    hc = [0.30, 0.42, 0.34][v]
    R = [0.16, 0.14, 0.19][v]
    tp = trunk_path(rng, 0.97, 0.006, 8, lean=rng.normal(0, 0.02, 2))
    rad = 0.0085 * (1 - tp[:, 2]) ** 0.9 + 0.004 * np.exp(-tp[:, 2] / 0.015) + 0.0006
    m.tube(tp, rad, 6, "birch")
    spr = ["birch0", "birch1", "birch2"]
    broadleaf(m, rng, tp, hc, R, [16, 14, 18][v], spr, 0.10, "birch", [0.18, 0.3, 0.22][v], 0.97)
    for k in range(7):
        P = along(tp, rng.uniform(0.84, 0.97)) + np.r_[rng.normal(0, 0.03, 2), 0]
        leafy_clump(m, rng, P, 0.09, spr[k % 3])
    return m, dict(crown_base=hc, crown_radius=R, crown_center=(hc + 1) / 2, trunk_r=0.0085)


def alder(v):
    """Grey alder: 2-3 stems from one base, rounded dense dark-green crown."""
    rng = np.random.default_rng(400 + v)
    m = Mesh()
    hc = [0.28, 0.35][v]
    R = [0.22, 0.19][v]
    nst = [3, 2][v]
    spr = ["alder0", "alder1"]
    for s in range(nst):
        az = s * 2 * math.pi / nst + rng.uniform(0, 1)
        lean = 0.12 * horiz(az)[:2]
        top = rng.uniform(0.85, 0.97)
        tp = trunk_path(rng, top, 0.005, 7, lean=lean)
        rad = 0.0065 * (1 - tp[:, 2]) ** 0.9 + 0.002 * np.exp(-tp[:, 2] / 0.015) + 0.0005
        m.tube(tp, rad, 4, "alder")
        broadleaf(m, rng, tp, hc, R * 0.8, [8, 9][v], spr, 0.13, "alder", 0.05, top, clumps_per=4,
                  elev=(35, 60), lenf=1.0)
        for k in range(3):
            P = along(tp, top - rng.uniform(0, 0.06)) + np.r_[rng.normal(0, 0.03, 2), 0]
            leafy_clump(m, rng, P, 0.12, spr[k % 2])
    return m, dict(crown_base=hc, crown_radius=R, crown_center=(hc + 1) / 2, trunk_r=0.0065)


def juniper(v):
    """Common juniper: dense columnar (v0) or broader multi-lobed (v1) prickly shrub."""
    rng = np.random.default_rng(500 + v)
    m = Mesh()
    R = [0.085, 0.15][v]
    spr = ["juniper0", "juniper1"]
    tp = np.array([[0, 0, 0], [0, 0, 0.12], [0, 0, 0.3]])
    m.tube(tp, [0.012, 0.009, 0.005], 5, "pine")
    for k in range(3):
        a = k * math.pi / 3
        m.quad(np.array([0, 0, 0.0]), horiz(a) * 2 * R * 0.85, UP * 1.0, spr[k % 2], (0.5, 1.0))
    n = [26, 34][v]
    for k in range(n):
        z = rng.uniform(0.08, 0.92)
        env = R * math.sin(math.pi * min(1.0, z ** 0.85)) ** 0.7
        r = env * math.sqrt(rng.random()) * 0.8
        a = rng.uniform(0, 2 * math.pi)
        P = np.array([r * math.cos(a), r * math.sin(a), z])
        S = rng.uniform(0.13, 0.2) * (0.6 + 0.6 * env / R)
        b = rng.uniform(0, 2 * math.pi)
        upv = unit(UP + horiz(b + 1.57) * rng.normal(0, 0.4))
        m.quad(P, horiz(b) * S, upv * S, spr[k % 2])
    return m, dict(crown_base=0.05, crown_radius=R, crown_center=0.5, trunk_r=0.012)


BUILDERS = {"spruce": spruce, "pine": pine, "birch": birch, "alder": alder, "juniper": juniper}


def leaf_normals(pos, cn, meta):
    """Volumetric foliage normals: mostly outward from the crown centre axis + upward, a bit of
    the card normal (sign flipped to face outward). Gives round, sun-lit crowns from alpha cards."""
    zc = meta["crown_center"]; R = meta["crown_radius"]
    hh = max(1.0 - meta["crown_base"], 0.2) / 2
    d = np.c_[pos[:, 0] / R, pos[:, 1] / R, (pos[:, 2] - zc) / hh * 0.6]
    d = unit(d)
    c = cn * np.sign(np.sum(cn * d, axis=1, keepdims=True) + 1e-9)
    return unit(d * 0.8 + c * 0.25 + UP * 0.35)


# ====================================================================== impostors
def bake_views(arrs, texs, hw, zmax, cell=(256, 512), ss=4):
    """Orthographic side view (looking along +Y) and top view of one near tree."""
    P = np.concatenate([arrs["bark"][0], arrs["leaf"][0]])
    N = np.concatenate([arrs["bark"][1], arrs["leaf"][1]])
    U = np.concatenate([arrs["bark"][2], arrs["leaf"][2]])
    nb = len(arrs["bark"][0])
    T = np.concatenate([arrs["bark"][3], arrs["leaf"][3] + nb])
    tid = np.r_[np.zeros(len(arrs["bark"][3]), int), np.ones(len(arrs["leaf"][3]), int)]
    sun = unit(np.array([0.3, -0.5, 0.8]))
    lam = np.clip(N @ sun, 0, 1)
    out = {}
    W, H = cell[0] * ss, cell[1] * ss
    # side
    xy = np.c_[(P[:, 0] + hw) / (2 * hw) * W, (1 - P[:, 2] / zmax) * H]
    front = (P[:, 1] - P[:, 1].min()) / max(np.ptp(P[:, 1]), 1e-6)
    shade = (0.55 + 0.45 * lam) * (1.0 - 0.25 * front)
    col, _ = rasterize(xy, P[:, 1], U, shade, T, texs, tid, W, H)
    out["side"] = downsample(col, ss)
    # top (image y = -world y)
    Wt = cell[0] * ss
    xy = np.c_[(P[:, 0] + hw) / (2 * hw) * Wt, (1 - (P[:, 1] + hw) / (2 * hw)) * Wt]
    shade = (0.55 + 0.45 * lam) * (0.7 + 0.3 * np.clip(P[:, 2] / zmax, 0, 1))
    col, _ = rasterize(xy, -P[:, 2], U, shade, T, texs, tid, Wt, Wt)
    out["top"] = downsample(col, ss)
    return out


def finish_sprite(rgba, boost=1.6, gain=0.85):
    """Coverage -> alpha with a boost so thin far trees survive the 0.5 alpha test. `gain`
    compensates for the impostor cards being lit flat (no self-shadowing inside the crown)."""
    a = np.clip(rgba[..., 3] * boost, 0, 1)
    return np.dstack([rgba[..., :3] * gain, a])


def far_mesh(hw, zmax, side_uv, top_uv, top_z):
    """3 vertical cards at 60 deg + 1 horizontal card. 8 triangles."""
    P, N, U, I = [], [], [], []
    for k in range(3):
        a = k * math.pi / 3
        r = horiz(a) * hw
        u0, v0, u1, v1 = side_uv
        base = len(P)
        for sx, sz in ((0, 0), (1, 0), (1, 1), (0, 1)):
            p = (sx * 2 - 1) * r + UP * zmax * sz
            P.append(p)
            N.append(unit(unit(r) * (sx * 2 - 1) * 0.5 + UP * 0.6 + horiz(a + math.pi / 2) * 0.0))
            U.append((u0 + (u1 - u0) * sx, v1 - (v1 - v0) * sz))
        I += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    u0, v0, u1, v1 = top_uv
    base = len(P)
    for sx, sy in ((0, 0), (1, 0), (1, 1), (0, 1)):
        P.append(np.array([(sx * 2 - 1) * hw, (sy * 2 - 1) * hw, top_z]))
        N.append(UP)
        U.append((u0 + (u1 - u0) * sx, v1 - (v1 - v0) * sy))
    I += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    return np.array(P), np.array(N), np.array(U), np.array(I)


def main():
    os.makedirs(MESH_DIR, exist_ok=True)
    fol = np.asarray(Image.open(os.path.join(TEX_DIR, "foliage.png")).convert("RGBA")).astype(np.float32) / 255
    bark = np.asarray(Image.open(os.path.join(TEX_DIR, "bark.jpg")).convert("RGBA")).astype(np.float32) / 255
    texs = [bark, fol]
    lib, meta_all = {}, {}
    far_cells = []
    for sp, nv in SPECIES.items():
        meta_all[sp] = []
        for v in range(nv):
            m, meta = BUILDERS[sp](v)
            arrs = {k: list(m.arrays(k)) for k in ("bark", "leaf")}
            arrs["leaf"][1] = leaf_normals(arrs["leaf"][0], arrs["leaf"][1], meta)
            allp = np.concatenate([arrs["bark"][0], arrs["leaf"][0]])
            hw = float(np.abs(allp[:, :2]).max()) * 1.02
            zmax = float(allp[:, 2].max()) * 1.005
            tris = len(arrs["bark"][3]) + len(arrs["leaf"][3])
            views = bake_views(arrs, texs, hw, zmax)
            far_cells.append((sp, v, views))
            meta.update(hw=hw, zmax=zmax, tris_near=tris, tris_far=8,
                        bark_tris=len(arrs["bark"][3]), leaf_tris=len(arrs["leaf"][3]))
            meta_all[sp].append(meta)
            for k in ("bark", "leaf"):
                for name, a in zip(("pos", "nrm", "uv", "idx"), arrs[k]):
                    lib[f"{sp}_{v}_{k}_{name}"] = a.astype(np.uint32 if name == "idx" else np.float32)
            print(f"{sp}_{v}: {tris} tris (bark {meta['bark_tris']}, leaf {meta['leaf_tris']}), "
                  f"width {2 * hw:.2f}, height {zmax:.2f}", flush=True)
    # far atlas: side cells 256x512 in rows 0-1, top cells 256x256 in rows below
    FA = np.zeros((2048, 2048, 4), np.float32)
    for i, (sp, v, views) in enumerate(far_cells):
        sx, sy = (i % 8) * 256, (i // 8) * 512
        g = 0.72 if sp in ("birch", "alder") else 0.85
        FA[sy:sy + 512, sx:sx + 256] = finish_sprite(views["side"], gain=g)
        tx, ty = (i % 8) * 256, 1024 + (i // 8) * 256
        FA[ty:ty + 256, tx:tx + 256] = finish_sprite(views["top"], gain=g)
        meta = meta_all[sp][v]
        side_uv = ((sx + 1) / 2048, (sy + 1) / 2048, (sx + 255) / 2048, (sy + 511) / 2048)
        top_uv = ((tx + 1) / 2048, (ty + 1) / 2048, (tx + 255) / 2048, (ty + 255) / 2048)
        top_z = meta["crown_base"] + 0.45 * (meta["zmax"] - meta["crown_base"])
        P, N, U, I = far_mesh(meta["hw"], meta["zmax"], side_uv, top_uv, top_z)
        for name, a in zip(("pos", "nrm", "uv", "idx"), (P, N, U, I)):
            lib[f"{sp}_{v}_far_{name}"] = a.astype(np.uint32 if name == "idx" else np.float32)
    from textures import bleed
    img = Image.fromarray((np.clip(FA, 0, 1) * 255).astype(np.uint8), "RGBA")
    bleed(img).save(os.path.join(TEX_DIR, "far.png"), optimize=True)
    np.savez_compressed(os.path.join(ASSET_DIR, "library.npz"), **lib)
    json.dump({"reference_height_m": 1.0, "species": meta_all}, open(os.path.join(ASSET_DIR, "trees.json"), "w"),
              indent=1)
    # per-tree glbs
    for sp, nv in SPECIES.items():
        for v in range(nv):
            prims = [dict(pos=lib[f"{sp}_{v}_{k}_pos"], nrm=lib[f"{sp}_{v}_{k}_nrm"], uv=lib[f"{sp}_{v}_{k}_uv"],
                          idx=lib[f"{sp}_{v}_{k}_idx"], material=k) for k in ("bark", "leaf")]
            write_glb(os.path.join(MESH_DIR, f"{sp}_{v}.glb"), prims, MATERIALS)
            k = "far"
            write_glb(os.path.join(MESH_DIR, f"{sp}_{v}_far.glb"),
                      [dict(pos=lib[f"{sp}_{v}_{k}_pos"], nrm=lib[f"{sp}_{v}_{k}_nrm"], uv=lib[f"{sp}_{v}_{k}_uv"],
                            idx=lib[f"{sp}_{v}_{k}_idx"], material=k)], MATERIALS)
    # preview sheet (side views on grey) for quick checks
    sheet = Image.new("RGB", (len(far_cells) * 200, 512), (150, 160, 170))
    for i, (sp, v, views) in enumerate(far_cells):
        mt = meta_all[sp][v]
        im = Image.fromarray((np.clip(views["side"], 0, 1) * 255).astype(np.uint8), "RGBA")
        im = im.resize((max(8, int(512 * 2 * mt["hw"] / mt["zmax"])), 512), Image.LANCZOS)
        sheet.paste(im, (i * 200 + 100 - im.width // 2, 0), im)
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ASSET_DIR, "preview.jpg")
    sheet.save(out, quality=85)
    print("wrote", ASSET_DIR, "preview", out)


if __name__ == "__main__":
    main()
