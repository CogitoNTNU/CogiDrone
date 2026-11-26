#!/usr/bin/env python3
"""Forest chunk builder: merge tens of thousands of tree instances into a few hundred meshes.

Gazebo has no LOD and pays per entity/visual, so trees are baked offline into spatial chunks:
one static model, one link, per chunk one .glb per material (bark, leaf/tint, far/tint), plus
trunk collisions for near-tier trees only.

Tried and rejected: one visual per tree on shared library meshes. Gazebo then keeps 1/3 of the
RAM (merged chunks are unique geometry, ~300 B per vertex per Gazebo process), but 23k separate
items made the dense Bymarka world render at RTF 0.2 on the AGX Orin (merged: 1.0). And past ~32k
visuals with their own material Ogre's uint16 render-state ref count wraps and Gazebo crashes.

    from forest_builder import build_forest
    stats = build_forest(instances, out_dir, "bymarka_forest", chunk_size=100.0)

instances: iterable of dicts
    species  'spruce' | 'pine' | 'birch' | 'alder' | 'juniper'
    x, y, z  trunk base position in the world/model frame (z = ground height), metres
    height   tree height in metres (the library trees are 1.0 m tall and get scaled by this)
    yaw      rotation about Z in radians                               (optional, default random)
    tint     (r, g, b) foliage colour multiplier, ~0.7..1.2           (optional, default random)
    variant  int, which library variant of the species                (optional, default random)
    seed     int, used instead of the instance index for the random defaults (optional)
    tier     'near' | 'far'                                           (optional, overrides the call)

Only numpy (and Pillow is not even needed here). Python 3.8+.
Run `python3 forest_builder.py --demo 5000` for a test forest.
"""
import argparse
import json
import math
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from common import ASSET_DIR, TEX_DIR  # noqa: E402
from glb import write_glb  # noqa: E402

SPECIES = ["spruce", "pine", "birch", "alder", "juniper"]

# Foliage colour classes. Gazebo ignores vertex colours, so per-instance tint is quantised to
# a few material variants (SDF <diffuse> multiplies the albedo map). Index 0 is neutral.
# SDF colours must be <= 1, so the atlas holds the brightest look and tints only darken/shift.
TINTS = np.array([[1.00, 1.00, 1.00], [0.78, 0.82, 0.78], [0.97, 0.93, 0.76], [0.84, 0.90, 0.94]])


class Library:
    def __init__(self, asset_dir=ASSET_DIR):
        lib = np.load(os.path.join(asset_dir, "library.npz"))
        self.meta = json.load(open(os.path.join(asset_dir, "trees.json")))["species"]
        self.tpl = {}
        for sp in SPECIES:
            for v in range(len(self.meta[sp])):
                for k in ("bark", "leaf", "far"):
                    self.tpl[(sp, v, k)] = tuple(lib[f"{sp}_{v}_{k}_{a}"] for a in ("pos", "nrm", "uv", "idx"))

    def nvar(self, sp):
        return len(self.meta[sp])


def _to_arrays(instances, lib, tier, near_center, near_radius, seed, lean_deg):
    """Instances -> dict of numpy columns, random defaults filled in deterministically."""
    sp, x, y, z, h, yaw, tint, var, tr, sd = [], [], [], [], [], [], [], [], [], []
    for i, d in enumerate(instances):
        sp.append(SPECIES.index(d["species"]))
        x.append(d["x"]); y.append(d["y"]); z.append(d.get("z", 0.0)); h.append(d["height"])
        yaw.append(d.get("yaw", np.nan))
        t = d.get("tint")
        tint.append(t if t is not None else (np.nan, np.nan, np.nan))
        var.append(d["variant"] if d.get("variant") is not None else -1)
        tr.append({"near": 0, "far": 1}.get(d.get("tier"), -1))
        sd.append(d.get("seed", i))
    n = len(sp)
    A = {"sp": np.array(sp, np.int32), "x": np.array(x, np.float64), "y": np.array(y, np.float64),
         "z": np.array(z, np.float64), "h": np.array(h, np.float64), "yaw": np.array(yaw, np.float64),
         "tint": np.array(tint, np.float64).reshape(n, 3), "var": np.array(var, np.int32),
         "tier": np.array(tr, np.int32), "seed": np.array(sd, np.int64)}
    # per-instance deterministic random numbers from (seed, instance seed)
    rs = np.random.default_rng(np.random.SeedSequence([seed]))
    salt = rs.integers(0, 2 ** 31, 8)
    def urand(k):
        v = (A["seed"] * 2654435761 + salt[k]) % 2 ** 32
        v = (v ^ (v >> 13)) * 1274126177 % 2 ** 32
        return (v ^ (v >> 16)) / 2 ** 32
    m = np.isnan(A["yaw"]); A["yaw"][m] = urand(0)[m] * 2 * np.pi
    nv = np.array([lib.nvar(s) for s in SPECIES])
    m = A["var"] < 0
    A["var"][m] = (urand(1)[m] * nv[A["sp"][m]]).astype(np.int32)
    A["var"] = np.minimum(A["var"], nv[A["sp"]] - 1)
    # tint class: nearest TINTS entry; random (weighted to neutral) if not given
    t = A["tint"]; m = np.isnan(t[:, 0])
    cls = np.argmin(((t[:, None, :] - TINTS[None]) ** 2).sum(-1), axis=1)
    r = urand(2)
    cls[m] = np.searchsorted(np.cumsum([0.4, 0.2, 0.2, 0.2]), r[m] * 0.9999)
    A["tcls"] = cls.astype(np.int32)
    # small random lean (rad) about a random horizontal axis
    A["lean"] = np.radians(lean_deg) * urand(3) ** 2
    A["lean_az"] = urand(4) * 2 * np.pi
    # tier
    if tier in ("near", "far"):
        default = 0 if tier == "near" else 1
        A["tier"][A["tier"] < 0] = default
    else:  # 'auto': near inside near_radius around near_center
        cx, cy = near_center
        d = np.hypot(A["x"] - cx, A["y"] - cy)
        auto = np.where(d <= (near_radius if near_radius is not None else np.inf), 0, 1)
        A["tier"] = np.where(A["tier"] < 0, auto, A["tier"])
    return A


def _rotations(yaw, lean, lean_az):
    """R = Rz(yaw) then tilt by `lean` about horizontal axis at lean_az. Returns (n,3,3)."""
    cz, sz = np.cos(yaw), np.sin(yaw)
    n = len(yaw)
    Rz = np.zeros((n, 3, 3)); Rz[:, 0, 0] = cz; Rz[:, 0, 1] = -sz; Rz[:, 1, 0] = sz; Rz[:, 1, 1] = cz; Rz[:, 2, 2] = 1
    ax = np.stack([np.cos(lean_az), np.sin(lean_az), np.zeros(n)], 1)
    K = np.zeros((n, 3, 3))
    K[:, 0, 2] = ax[:, 1]; K[:, 1, 2] = -ax[:, 0]; K[:, 2, 0] = -ax[:, 1]; K[:, 2, 1] = ax[:, 0]
    s = np.sin(lean)[:, None, None]; c = (1 - np.cos(lean))[:, None, None]
    Rl = np.eye(3)[None] + s * K + c * (K @ K)
    return Rl @ Rz


def _merge(tpl_list, R, scale, trans):
    """Instance templates (lists, same length as R) -> merged float32 arrays."""
    P, N, U, I = [], [], [], []
    off = 0
    # group identical templates so the transform is one einsum per group
    keys = {}
    for i, t in enumerate(tpl_list):
        keys.setdefault(id(t), (t, []))[1].append(i)
    for t, ids in keys.values():
        ids = np.array(ids)
        pos, nrm, uv, idx = t
        k = len(ids)
        Ri = R[ids]
        p = np.einsum("kij,nj->kni", Ri * scale[ids][:, None, None], pos) + trans[ids][:, None, :]
        nn = np.einsum("kij,nj->kni", Ri, nrm)
        P.append(p.reshape(-1, 3)); N.append(nn.reshape(-1, 3))
        U.append(np.broadcast_to(uv, (k,) + uv.shape).reshape(-1, 2))
        I.append((idx[None] + (off + np.arange(k) * len(pos))[:, None, None]).reshape(-1, 3))
        off += k * len(pos)
    return (np.concatenate(P).astype(np.float32), np.concatenate(N).astype(np.float32),
            np.concatenate(U).astype(np.float32), np.concatenate(I).astype(np.uint32))


def _prism_mesh(cx, cy, cz, r, hgt, sides=6):
    """Open n-gon prisms (collision trunks) -> pos, idx."""
    n = len(cx)
    a = np.arange(sides) * 2 * np.pi / sides
    ring = np.stack([np.cos(a), np.sin(a)], 1)  # (s,2)
    bot = np.zeros((n, sides, 3)); top = np.zeros((n, sides, 3))
    bot[..., 0] = cx[:, None] + r[:, None] * ring[:, 0]; bot[..., 1] = cy[:, None] + r[:, None] * ring[:, 1]
    bot[..., 2] = cz[:, None]
    top[...] = bot; top[..., 2] = (cz + hgt)[:, None]
    pos = np.concatenate([bot, top], 1).reshape(-1, 3)
    j = np.arange(sides); j1 = (j + 1) % sides
    tri = np.concatenate([np.stack([j, j1, j1 + sides], 1), np.stack([j, j1 + sides, j + sides], 1)])
    # caps
    cap_b = np.stack([np.zeros(sides - 2, int), np.arange(2, sides), np.arange(1, sides - 1)], 1)
    cap_t = np.stack([np.full(sides - 2, sides), np.arange(1, sides - 1) + sides, np.arange(2, sides) + sides], 1)
    tri = np.concatenate([tri, cap_b, cap_t])
    idx = (tri[None] + (np.arange(n) * 2 * sides)[:, None, None]).reshape(-1, 3)
    return pos.astype(np.float32), idx.astype(np.uint32)


def _material_xml(model_name, tex, tint, double_sided, rough):
    r, g, b = tint
    return f"""<material>
          <ambient>{r:.3f} {g:.3f} {b:.3f} 1</ambient><diffuse>{r:.3f} {g:.3f} {b:.3f} 1</diffuse>
          <specular>0.02 0.02 0.02 1</specular>
          <double_sided>{'true' if double_sided else 'false'}</double_sided>
          <pbr><metal>
            <albedo_map>model://{model_name}/materials/textures/{tex}</albedo_map>
            <roughness>{rough}</roughness><metalness>0.0</metalness>
          </metal></pbr>
        </material>"""


def build_forest(instances, out_dir, model_name, chunk_size=100.0, tier="near", near_center=(0.0, 0.0),
                 near_radius=None, collision="cylinders", collide_far=False, tints=True, lean_deg=2.5,
                 cast_shadows="leaf", seed=0, asset_dir=ASSET_DIR, verbose=True):
    """Write a Gazebo model `out_dir/model_name` (model.config, model.sdf, meshes/, materials/).

    tier        default tier for instances without a 'tier' key: 'near', 'far' or 'auto'
                ('auto' = near within near_radius metres of near_center, far outside).
    collision   'cylinders' (one SDF cylinder per near tree trunk), 'mesh' (one merged
                collision mesh of 6-sided trunk prisms per chunk) or 'none'.
    collide_far also give far-tier trees trunk collisions (off by default).
    cast_shadows 'leaf' (default: only near-tier foliage casts shadows, the big render cost
                saver), 'all'/True, or 'none'/False.
    tints       use the foliage tint classes (TINTS) as separate materials; False = one material.
    Returns a stats dict (counts, triangles, files, seconds)."""
    t0 = time.time()
    lib = Library(asset_dir)
    A = _to_arrays(instances, lib, tier, near_center, near_radius, seed, lean_deg)
    n = len(A["sp"])
    root = os.path.join(out_dir, model_name)
    mdir = os.path.join(root, "meshes"); tdir = os.path.join(root, "materials", "textures")
    if os.path.isdir(mdir):
        shutil.rmtree(mdir)
    os.makedirs(mdir); os.makedirs(tdir, exist_ok=True)
    for f in ("bark.jpg", "foliage.png", "far.png"):
        shutil.copy(os.path.join(asset_dir, "textures", f), os.path.join(tdir, f))
    R = _rotations(A["yaw"], A["lean"], A["lean_az"])
    cx = np.floor(A["x"] / chunk_size).astype(np.int64); cy = np.floor(A["y"] / chunk_size).astype(np.int64)
    key = (cx - cx.min()) * 1_000_003 + (cy - cy.min()) if n else cx
    order = np.argsort(key, kind="stable")
    bounds = np.flatnonzero(np.diff(key[order])) + 1
    groups = np.split(order, bounds) if n else []
    tcls = A["tcls"] if tints else np.zeros(n, np.int32)
    gl_mats = {"bark": {"image": "../materials/textures/bark.jpg"},
               "leaf": {"image": "../materials/textures/foliage.png", "alpha": "MASK", "double_sided": True},
               "far": {"image": "../materials/textures/far.png", "alpha": "MASK", "double_sided": True}}
    visuals, collisions = [], []
    tris = {"bark": 0, "leaf": 0, "far": 0, "collision": 0}
    files = 0
    for g in groups:
        ix, iy = cx[g[0]], cy[g[0]]
        org = np.array([(ix + 0.5) * chunk_size, (iy + 0.5) * chunk_size, float(np.median(A["z"][g]))])
        trans = np.stack([A["x"][g], A["y"][g], A["z"][g]], 1) - org
        name = f"c{ix}_{iy}".replace("-", "m")
        near = A["tier"][g] == 0
        jobs = []
        if near.any():
            jobs.append(("bark", "bark", near, 0))
            for c in np.unique(tcls[g][near]):
                jobs.append(("leaf", f"leaf{c}", near & (tcls[g] == c), c))
        if (~near).any():
            for c in np.unique(tcls[g][~near]):
                jobs.append(("far", f"far{c}", (~near) & (tcls[g] == c), c))
        for kind, label, sel, c in jobs:
            ids = np.flatnonzero(sel)
            tpl = [lib.tpl[(SPECIES[A["sp"][g[i]]], int(A["var"][g[i]]), kind)] for i in ids]
            P, N, U, I = _merge(tpl, R[g[ids]], A["h"][g[ids]], trans[ids])
            fn = f"{name}_{label}.glb"
            write_glb(os.path.join(mdir, fn), [dict(pos=P, nrm=N, uv=U, idx=I, material=kind)], gl_mats)
            files += 1
            tris[kind] += len(I)
            tex = {"bark": "bark.jpg", "leaf": "foliage.png", "far": "far.png"}[kind]
            tint = (1.0, 1.0, 1.0) if kind == "bark" else tuple(TINTS[c])
            shadow = cast_shadows is True or cast_shadows == "all" or (cast_shadows == "leaf" and kind == "leaf")
            visuals.append(f"""      <visual name="{name}_{label}">
        <pose>{org[0]:.3f} {org[1]:.3f} {org[2]:.3f} 0 0 0</pose>
        <cast_shadows>{'true' if shadow else 'false'}</cast_shadows>
        <geometry><mesh><uri>model://{model_name}/meshes/{fn}</uri></mesh></geometry>
        {_material_xml(model_name, tex, tint, kind != 'bark', 0.95 if kind == 'bark' else 0.85)}
      </visual>""")
        # trunk collisions
        cs = near if not collide_far else np.ones_like(near)
        cs &= A["sp"][g] != SPECIES.index("juniper")
        if collision != "none" and cs.any():
            ids = g[cs]
            r = np.array([lib.meta[SPECIES[s]][v]["trunk_r"] for s, v in zip(A["sp"][ids], A["var"][ids])]) * A["h"][ids]
            r = np.maximum(r * 0.9, 0.05)
            hh = A["h"][ids] * 0.8
            if collision == "cylinders":
                for k, i in enumerate(ids):
                    collisions.append(
                        f'      <collision name="t{i}"><pose>{A["x"][i]:.2f} {A["y"][i]:.2f} {A["z"][i] + hh[k] / 2:.2f} 0 0 0</pose>'
                        f'<geometry><cylinder><radius>{r[k]:.3f}</radius><length>{hh[k]:.2f}</length></cylinder></geometry></collision>')
            else:
                tr = trans[cs]
                P, I = _prism_mesh(tr[:, 0], tr[:, 1], tr[:, 2], r, hh)
                fn = f"{name}_coll.glb"
                write_glb(os.path.join(mdir, fn), [dict(pos=P, nrm=np.zeros_like(P), uv=np.zeros((len(P), 2), np.float32),
                                                        idx=I, material="c")], {"c": {}})
                files += 1
                tris["collision"] += len(I)
                collisions.append(f"""      <collision name="{name}_coll">
        <pose>{org[0]:.3f} {org[1]:.3f} {org[2]:.3f} 0 0 0</pose>
        <geometry><mesh><uri>model://{model_name}/meshes/{fn}</uri></mesh></geometry>
      </collision>""")
    n_near = int((A["tier"] == 0).sum())
    sdf = f"""<?xml version="1.0"?>
<!-- Generated by sim/tools/trees/forest_builder.py: {n} trees ({n_near} near, {n - n_near} far)
     in {len(groups)} chunks of {chunk_size:g} m. Do not edit. Tree assets: see sim/assets/trees/ATTRIBUTION.md -->
<sdf version="1.9">
  <model name="{model_name}">
    <static>true</static>
    <link name="trees">
{chr(10).join(visuals)}
{chr(10).join(collisions)}
    </link>
  </model>
</sdf>
"""
    open(os.path.join(root, "model.sdf"), "w").write(sdf)
    open(os.path.join(root, "model.config"), "w").write(f"""<?xml version="1.0"?>
<model>
  <name>{model_name}</name>
  <version>1.0</version>
  <sdf version="1.9">model.sdf</sdf>
  <author><name>CogiDrone sim/tools/trees/forest_builder.py</name></author>
  <description>{n} merged trees in {len(groups)} chunks (generated). Textures CC0 (Poly Haven, ambientCG).</description>
</model>
""")
    stats = {"trees": n, "near": n_near, "far": n - n_near, "chunks": len(groups), "visuals": len(visuals),
             "collisions": len(collisions), "triangles": tris, "files": files,
             "mesh_mb": round(sum(os.path.getsize(os.path.join(mdir, f)) for f in os.listdir(mdir)) / 1e6, 1),
             "seconds": round(time.time() - t0, 1)}
    if verbose:
        print(json.dumps(stats))
    return stats


# ---------------------------------------------------------------------------- demo / CLI
HEIGHTS = {"spruce": (14, 30), "pine": (12, 24), "birch": (8, 19), "alder": (6, 14), "juniper": (1.0, 3.5)}
MIX = {"spruce": 0.60, "pine": 0.20, "birch": 0.12, "alder": 0.05, "juniper": 0.03}


def demo_instances(n, density=0.03, seed=1, clearing=0.0):
    """n trees on flat ground, square area sized for `density` trees/m^2 (0.03 = 300/ha),
    centred on the origin; species in patches (pine/birch clumps inside spruce)."""
    rng = np.random.default_rng(seed)
    side = math.sqrt(n / density)
    pts = rng.uniform(-side / 2, side / 2, (int(n * 1.3), 2))
    if clearing > 0:
        pts = pts[np.hypot(pts[:, 0], pts[:, 1]) > clearing]
    pts = pts[:n]
    # species patches from low-frequency noise
    k = rng.normal(size=(6, 2)) / 150.0; ph = rng.uniform(0, 6.28, 6)
    f = sum(np.sin(pts @ k[i] + ph[i]) for i in range(6)) / 2.4
    u = rng.random(len(pts))
    out = []
    for i, (x, y) in enumerate(pts):
        if f[i] > 0.75:
            sp = "pine" if u[i] < 0.7 else "spruce"
        elif f[i] < -0.8:
            sp = "birch" if u[i] < 0.6 else ("alder" if u[i] < 0.8 else "spruce")
        else:
            sp = rng.choice(list(MIX), p=list(MIX.values()))
        lo, hi = HEIGHTS[sp]
        out.append({"species": sp, "x": float(x), "y": float(y), "z": 0.0, "height": float(rng.uniform(lo, hi))})
    return out


def gallery_instances(y=0.0, x0=-60.0):
    """One instance of every library variant in a row (for screenshots), 20 m tall, juniper 3 m."""
    lib = json.load(open(os.path.join(ASSET_DIR, "trees.json")))["species"]
    out, x = [], x0
    for sp in SPECIES:
        for v in range(len(lib[sp])):
            h = 3.0 if sp == "juniper" else 20.0
            out.append({"species": sp, "variant": v, "x": x, "y": y, "z": 0.0, "height": h, "yaw": 0.0,
                        "tint": (1, 1, 1), "tier": "near"})
            x += 10.0
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--demo", type=int, required=True, help="number of random trees")
    ap.add_argument("--out", default=os.path.expanduser("~/cogidrone_deps/simassets-cache/forest_demo"))
    ap.add_argument("--name", default="forest_demo")
    ap.add_argument("--chunk", type=float, default=100.0)
    ap.add_argument("--near-radius", type=float, default=None, help="near tier inside this radius, far outside")
    ap.add_argument("--tier", default=None, choices=["near", "far", "auto"])
    ap.add_argument("--collision", default="cylinders", choices=["cylinders", "mesh", "none"])
    ap.add_argument("--density", type=float, default=0.03)
    ap.add_argument("--clearing", type=float, default=0.0, help="keep a tree-free circle of this radius")
    ap.add_argument("--shadows", default="leaf", choices=["leaf", "all", "none"])
    ap.add_argument("--gallery", action="store_true", help="add one tree of every variant in a row at y=0")
    a = ap.parse_args()
    inst = demo_instances(a.demo, a.density, clearing=a.clearing)
    if a.gallery:
        inst += gallery_instances()
    tier = a.tier or ("auto" if a.near_radius else "near")
    build_forest(inst, a.out, a.name, a.chunk, tier=tier, near_radius=a.near_radius, collision=a.collision,
                 cast_shadows=a.shadows)


if __name__ == "__main__":
    main()
