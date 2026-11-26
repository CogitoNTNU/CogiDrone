#!/usr/bin/env python3
"""Merge many ground-clutter prop instances into one static Gazebo model.

Gazebo has no mesh LOD/instancing, so tens of thousands of separate visuals
kill performance. This module bakes instances offline into per-chunk meshes:
one visual per (chunk, material) - there are only 3 materials (rock / plant /
wood texture atlases) - plus one merged collision mesh per chunk for solid
kinds. Pure numpy; runs with /usr/bin/python3 or the simtools venv.

Python API
----------
    from props_builder import build_props
    build_props(instances, out_dir, model_name, chunk_size=100.0,
                collision_kinds=("rock", "outcrop", "log"))

instances: iterable of dicts
    kind     'rock' | 'boulder' | 'outcrop' | 'fern' | 'bilberry' | 'heather' |
             'juniper' | 'log' | 'stump' | 'branch'
             (aliases: 'stone'->rock, 'blueberry'/'shrub'->bilberry, 'branches'->branch)
    variant  optional int (taken modulo the number of variants of that kind) or
             variant name (e.g. 'boulder_moss_c'); default: hash of x, y
    x, y, z  position; z is the ground height at (x, y)
    scale    optional uniform multiplier (default 1.0; library sizes are natural sizes)
    sx, sy, sz optional extra per-axis multipliers (e.g. stretched outcrops)
    yaw      optional rad (default: hash of x, y)
    roll, pitch optional rad (align to slope)
    sink     optional metres pushed into the ground along world -Z (default 0)

Collision groups: 'rock' covers rock+boulder, 'log' covers log+stump; plants and
branches never collide.

CLI
---
    python3 props_builder.py --demo 20000 --out /tmp/props_demo --name props_demo
"""
import argparse
import json
import math
import os
import shutil
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glb import read_glb, write_glb  # noqa: E402

DEFAULT_LIB = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            "..", "..", "assets", "props"))
ALIASES = {"stone": "rock", "blueberry": "bilberry", "shrub": "bilberry", "branches": "branch",
           "lowshrub": "heather"}
COLLISION_GROUP = {"rock": "rock", "boulder": "rock", "outcrop": "outcrop", "log": "log", "stump": "log"}

_LIB_CACHE = {}


class Library:
    def __init__(self, lib_dir):
        self.dir = lib_dir
        with open(os.path.join(lib_dir, "props_library.json")) as f:
            meta = json.load(f)
        self.materials = meta["materials"]
        self.mat_names = list(self.materials)
        self.variants = []          # list of dicts with arrays
        self.by_kind = {}
        self.by_name = {}
        for name, v in sorted(meta["variants"].items()):
            g = read_glb(os.path.join(lib_dir, v["mesh"]))
            ent = dict(v, name=name, V=g["positions"].astype(np.float32), N=g["normals"].astype(np.float32),
                       UV=g["uvs"].astype(np.float32), F=g["indices"].astype(np.uint32),
                       mat=self.mat_names.index(v["material"]))
            if "hull" in v:
                ent["HV"] = np.array(v["hull"]["v"], np.float32)
                ent["HF"] = np.array(v["hull"]["f"], np.uint32)
            ent["size"] = float(np.max(np.array(v["bounds"][1]) - np.array(v["bounds"][0])))
            idx = len(self.variants)
            self.variants.append(ent)
            self.by_kind.setdefault(v["kind"], []).append(idx)
            self.by_name[name] = idx

    @classmethod
    def get(cls, lib_dir=None):
        lib_dir = os.path.abspath(lib_dir or DEFAULT_LIB)
        if lib_dir not in _LIB_CACHE:
            _LIB_CACHE[lib_dir] = cls(lib_dir)
        return _LIB_CACHE[lib_dir]


def _hash01(x, y, salt):
    h = np.sin(x * 12.9898 + y * 78.233 + salt * 37.719) * 43758.5453
    return h - np.floor(h)


def _instances_to_arrays(instances, lib):
    """dict list -> structured numpy arrays (vectorised downstream)."""
    inst = instances if isinstance(instances, list) else list(instances)
    n = len(inst)
    var = np.empty(n, np.int32)
    cols = {k: np.zeros(n, np.float64) for k in ("x", "y", "z", "yaw", "roll", "pitch", "sink")}
    for k in ("scale", "sx", "sy", "sz"):
        cols[k] = np.ones(n, np.float64)
    has_yaw = np.zeros(n, bool)
    var_req = np.full(n, -1, np.int64)
    kinds = {}
    kind_idx = np.empty(n, np.int32)
    for i, d in enumerate(inst):
        k = d["kind"]
        k = ALIASES.get(k, k)
        if k not in kinds:
            if k not in lib.by_kind:
                raise ValueError(f"unknown prop kind {d['kind']!r}; have {sorted(lib.by_kind)}")
            kinds[k] = len(kinds)
        kind_idx[i] = kinds[k]
        for c in ("x", "y", "z"):
            cols[c][i] = d[c]
        for c in ("roll", "pitch", "sink", "scale", "sx", "sy", "sz"):
            if c in d and d[c] is not None:
                cols[c][i] = d[c]
        if d.get("yaw") is not None:
            cols["yaw"][i] = d["yaw"]
            has_yaw[i] = True
        v = d.get("variant")
        if v is not None:
            if isinstance(v, str):
                var_req[i] = -2 - lib.by_name[v]   # explicit variant by name
            else:
                var_req[i] = int(v)
    kind_names = list(kinds)
    hv = _hash01(cols["x"], cols["y"], 1.0)
    for ki, k in enumerate(kind_names):
        m = kind_idx == ki
        lst = np.array(lib.by_kind[k])
        req = var_req[m]
        auto = (hv[m] * len(lst)).astype(np.int64) % len(lst)
        sel = np.where(req >= 0, req % len(lst), auto)
        out = lst[sel]
        named = req <= -2
        out[named] = -2 - req[named]
        var[m] = out
    yaw_auto = _hash01(cols["x"], cols["y"], 2.0) * 2 * math.pi
    cols["yaw"] = np.where(has_yaw, cols["yaw"], yaw_auto)
    return var, cols


def _rotations(roll, pitch, yaw):
    cr, sr = np.cos(roll), np.sin(roll)
    cp, sp = np.cos(pitch), np.sin(pitch)
    cy, sy = np.cos(yaw), np.sin(yaw)
    R = np.empty((len(roll), 3, 3))
    R[:, 0, 0] = cy * cp
    R[:, 0, 1] = cy * sp * sr - sy * cr
    R[:, 0, 2] = cy * sp * cr + sy * sr
    R[:, 1, 0] = sy * cp
    R[:, 1, 1] = sy * sp * sr + cy * cr
    R[:, 1, 2] = sy * sp * cr - cy * sr
    R[:, 2, 0] = -sp
    R[:, 2, 1] = cp * sr
    R[:, 2, 2] = cp * cr
    return R


def _transform_variant(ent, M, Nm, T, with_attr=True, hull=False):
    """Batch-transform one variant for k instances. M, Nm: (k,3,3); T: (k,3)."""
    if hull:
        V, F = ent["HV"], ent["HF"]
    else:
        V, F = ent["V"], ent["F"]
    k, nv = len(M), len(V)
    P = (np.einsum("kij,vj->kvi", M, V) + T[:, None, :]).astype(np.float32).reshape(-1, 3)
    Fo = (F[None, :, :] + (np.arange(k, dtype=np.uint32) * nv)[:, None, None]).reshape(-1, 3)
    if not with_attr:
        return P, Fo
    Nn = np.einsum("kij,vj->kvi", Nm, ent["N"])
    Nn /= np.linalg.norm(Nn, axis=2, keepdims=True) + 1e-12
    UV = np.broadcast_to(ent["UV"], (k,) + ent["UV"].shape).reshape(-1, 2)
    return P, Nn.astype(np.float32).reshape(-1, 3), UV, Fo


def build_props(instances, out_dir, model_name, chunk_size=100.0, collision_kinds=("rock", "outcrop", "log"),
                lib_dir=None, texture_mode="copy", texture_uri_prefix=None, plant_shadows=True,
                min_collision_size=0.35, verbose=True):
    """Bake prop instances into one static Gazebo model directory `out_dir`.

    texture_mode: 'copy'   copy the atlas textures into out_dir/materials/textures (self-contained)
                  'shared' reference textures at `texture_uri_prefix` (e.g. 'model://props/textures')
    plant_shadows: let plant cards cast shadows (nicer close-up, costs GPU in the shadow pass).
    min_collision_size: skip collision for solid props smaller than this (m, largest dimension).
    Returns a stats dict.
    """
    t0 = time.time()
    lib = Library.get(lib_dir)
    var, c = _instances_to_arrays(instances, lib)
    n = len(var)
    S = np.stack([c["sx"], c["sy"], c["sz"]], 1) * c["scale"][:, None]
    R = _rotations(c["roll"], c["pitch"], c["yaw"])
    M = R * S[:, None, :]                      # R @ diag(S)
    Nm = R / S[:, None, :]                     # inverse-transpose for normals
    cs = float(chunk_size)
    ci = np.floor(c["x"] / cs).astype(np.int64)
    cj = np.floor(c["y"] / cs).astype(np.int64)
    centre = np.stack([(ci + 0.5) * cs, (cj + 0.5) * cs, np.zeros(n)], 1)
    T = np.stack([c["x"], c["y"], c["z"] - c["sink"]], 1) - centre
    mat = np.array([lib.variants[v]["mat"] for v in range(len(lib.variants))])[var]

    os.makedirs(os.path.join(out_dir, "meshes"), exist_ok=True)
    for f in os.listdir(os.path.join(out_dir, "meshes")):
        if f.endswith(".glb"):
            os.remove(os.path.join(out_dir, "meshes", f))

    # textures
    tex_uri = {}
    for mname, m in lib.materials.items():
        for key in ("albedo", "normal"):
            src = os.path.join(lib.dir, m[key])
            if texture_mode == "copy":
                dst_rel = os.path.join("materials", "textures", os.path.basename(src))
                dst = os.path.join(out_dir, dst_rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                if not (os.path.exists(dst) and os.path.getsize(dst) == os.path.getsize(src)
                        and os.path.getmtime(dst) >= os.path.getmtime(src)):
                    shutil.copy2(src, dst)
                tex_uri[(mname, key)] = f"model://{model_name}/{dst_rel}"
            else:
                assert texture_uri_prefix, "texture_mode='shared' needs texture_uri_prefix"
                tex_uri[(mname, key)] = f"{texture_uri_prefix.rstrip('/')}/{os.path.basename(src)}"

    coll_groups = set(collision_kinds or ())
    kinds = np.array([lib.variants[v]["kind"] for v in range(len(lib.variants))])
    v_coll = np.array([COLLISION_GROUP.get(k) in coll_groups and "HV" in lib.variants[i]
                       for i, k in enumerate(kinds)])
    v_size = np.array([e["size"] for e in lib.variants])
    inst_coll = v_coll[var] & (v_size[var] * S.max(1) >= min_collision_size)

    chunk_key = (ci - ci.min()) * (cj.max() - cj.min() + 1) + (cj - cj.min())
    order = np.lexsort((var, mat, chunk_key))
    visuals, collisions = [], []
    stats = {"instances": n, "tris": {m: 0 for m in lib.mat_names}, "coll_tris": 0, "chunks": 0, "files": 0}
    bounds = np.unique(chunk_key[order], return_index=True)
    starts = list(bounds[1]) + [n]
    for b in range(len(bounds[0])):
        sl = order[starts[b]:starts[b + 1]]
        i0 = sl[0]
        tag = f"c{ci[i0]:+04d}_{cj[i0]:+04d}".replace("+", "p").replace("-", "m")
        cx, cy = centre[i0, 0], centre[i0, 1]
        stats["chunks"] += 1
        for mi, mname in enumerate(lib.mat_names):
            sel = sl[mat[sl] == mi]
            if len(sel) == 0:
                continue
            Ps, Ns, UVs, Fs, off = [], [], [], [], 0
            for v in np.unique(var[sel]):
                s2 = sel[var[sel] == v]
                P, Nn, UV, F = _transform_variant(lib.variants[v], M[s2], Nm[s2], T[s2])
                Ps.append(P), Ns.append(Nn), UVs.append(UV), Fs.append(F + off)
                off += len(P)
            fname = f"{tag}_{mname}.glb"
            F = np.concatenate(Fs)
            write_glb(os.path.join(out_dir, "meshes", fname), np.concatenate(Ps), np.concatenate(Ns),
                      np.concatenate(UVs), F, name=f"{tag}_{mname}")
            stats["tris"][mname] += len(F)
            stats["files"] += 1
            visuals.append((f"{tag}_{mname}", fname, mname, cx, cy))
        csel = sl[inst_coll[sl]]
        if len(csel):
            Ps, Fs, off = [], [], 0
            for v in np.unique(var[csel]):
                s2 = csel[var[csel] == v]
                P, F = _transform_variant(lib.variants[v], M[s2], None, T[s2], with_attr=False, hull=True)
                Ps.append(P), Fs.append(F + off)
                off += len(P)
            P = np.concatenate(Ps)
            F = np.concatenate(Fs)
            fname = f"{tag}_collision.glb"
            write_glb(os.path.join(out_dir, "meshes", fname), P, np.tile([0, 0, 1], (len(P), 1)),
                      np.zeros((len(P), 2)), F, name=f"{tag}_collision")
            stats["coll_tris"] += len(F)
            stats["files"] += 1
            collisions.append((f"{tag}_collision", fname, cx, cy))

    _write_sdf(out_dir, model_name, lib, visuals, collisions, tex_uri, plant_shadows)
    stats["seconds"] = round(time.time() - t0, 1)
    stats["mesh_mb"] = round(sum(os.path.getsize(os.path.join(out_dir, "meshes", f))
                                 for f in os.listdir(os.path.join(out_dir, "meshes"))) / 1e6, 1)
    with open(os.path.join(out_dir, "props_stats.json"), "w") as f:
        json.dump(stats, f, indent=1)
    if verbose:
        print(f"[props_builder] {n} instances -> {stats['chunks']} chunks, {len(visuals)} visuals, "
              f"{len(collisions)} collisions, tris {stats['tris']} coll {stats['coll_tris']}, "
              f"{stats['mesh_mb']} MB, {stats['seconds']} s", flush=True)
    return stats


def _write_sdf(out_dir, model_name, lib, visuals, collisions, tex_uri, plant_shadows):
    L = ['<?xml version="1.0"?>', '<sdf version="1.9">', f'  <model name="{model_name}">',
         "    <static>true</static>", '    <link name="props">']
    for name, fname, cx, cy in collisions:
        L += [f'      <collision name="{name}">', f"        <pose>{cx:.3f} {cy:.3f} 0 0 0 0</pose>",
              f"        <geometry><mesh><uri>model://{model_name}/meshes/{fname}</uri></mesh></geometry>",
              "      </collision>"]
    for name, fname, mname, cx, cy in visuals:
        m = lib.materials[mname]
        shadows = "true" if (m.get("cast_shadows", True) and (plant_shadows or not m["alpha"])) else "false"
        L += [f'      <visual name="{name}">', f"        <pose>{cx:.3f} {cy:.3f} 0 0 0 0</pose>",
              f"        <cast_shadows>{shadows}</cast_shadows>",
              f"        <geometry><mesh><uri>model://{model_name}/meshes/{fname}</uri></mesh></geometry>",
              "        <material>",
              "          <ambient>1 1 1 1</ambient><diffuse>1 1 1 1</diffuse><specular>0.2 0.2 0.2 1</specular>",
              "          <pbr><metal>",
              f"            <albedo_map>{tex_uri[(mname, 'albedo')]}</albedo_map>",
              f"            <normal_map>{tex_uri[(mname, 'normal')]}</normal_map>",
              f"            <roughness>{m['roughness']}</roughness><metalness>0</metalness>",
              "          </metal></pbr>",
              "        </material>", "      </visual>"]
    L += ["    </link>", "  </model>", "</sdf>", ""]
    with open(os.path.join(out_dir, "model.sdf"), "w") as f:
        f.write("\n".join(L))
    with open(os.path.join(out_dir, "model.config"), "w") as f:
        f.write(f"""<?xml version="1.0"?>
<model>
  <name>{model_name}</name>
  <version>1.0</version>
  <sdf version="1.9">model.sdf</sdf>
  <description>Merged ground-clutter props (generated by sim/tools/props/props_builder.py)</description>
</model>
""")


# --------------------------------------------------------------------------
# demo scatter
def _value_noise(x, y, scale, seed):
    """Smooth 2D value noise in [0,1] (bilinear on a hashed lattice)."""
    gx, gy = x / scale, y / scale
    x0, y0 = np.floor(gx), np.floor(gy)
    fx, fy = gx - x0, gy - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def h(a, b):
        return _hash01(a, b, seed)
    return (h(x0, y0) * (1 - fx) * (1 - fy) + h(x0 + 1, y0) * fx * (1 - fy) +
            h(x0, y0 + 1) * (1 - fx) * fy + h(x0 + 1, y0 + 1) * fx * fy)


# Habitat mixes (fraction of instances by kind) used by the demo and documented as defaults.
HABITATS = {
    # dense spruce forest floor: moss + bilberry carpet, ferns in wet hollows, dead wood
    "spruce": {"bilberry": 0.72, "fern": 0.12, "heather": 0.02, "juniper": 0.003, "rock": 0.045,
               "boulder": 0.015, "outcrop": 0.001, "stump": 0.01, "log": 0.008, "branch": 0.03},
    # open pine on rocky slope: heather/lichen, juniper, boulders and bare rock
    "pine_rock": {"bilberry": 0.22, "heather": 0.45, "juniper": 0.03, "fern": 0.01, "rock": 0.17,
                  "boulder": 0.06, "outcrop": 0.012, "stump": 0.01, "log": 0.008, "branch": 0.02},
}


def random_instance(kind, x, y, z, rng, lib=None):
    """One instance dict with natural size/sink ranges for `kind` (variant chosen here)."""
    lib = lib or Library.get()
    names = [lib.variants[i]["name"] for i in lib.by_kind[kind]]
    v = names[rng.integers(len(names))]
    d = {"kind": kind, "variant": v, "x": float(x), "y": float(y), "z": float(z),
         "yaw": float(rng.uniform(0, 2 * math.pi))}
    if kind == "rock":
        d["scale"] = float(rng.uniform(1.5, 4.0) if v == "rock_small" else rng.uniform(0.25, 0.9))
        d["sink"] = float(rng.uniform(0.0, 0.08))
    elif kind == "boulder":
        d["scale"] = float(rng.uniform(0.6, 1.5))
        d["sink"] = float(rng.uniform(0.05, 0.3))
    elif kind == "outcrop":
        d["scale"] = float(rng.uniform(1.0, 2.2))
        d["sink"] = float(rng.uniform(0.3, 1.0) * d["scale"])
    elif kind == "fern":
        d["scale"] = float(rng.uniform(0.8, 1.5))
    elif kind in ("bilberry", "heather"):
        d["scale"] = float(rng.uniform(0.8, 1.3))
    elif kind == "juniper":
        d["scale"] = float(rng.uniform(0.6, 1.2))
    elif kind == "log":
        d["scale"] = float(rng.uniform(1.0, 2.5))
        d["sink"] = 0.05
    elif kind == "stump":
        d["scale"] = float(rng.uniform(0.6, 1.1))
    elif kind == "branch":
        d["scale"] = float(rng.uniform(0.8, 1.6))
    return d


def demo_instances(n, size=None, seed=0, z=0.0, density=0.3):
    """Scatter n instances on a flat square centred at 0 with two habitats.

    West/low-noise areas get the 'spruce' mix, the rest 'pine_rock'. Bilberry forms patches,
    ferns follow a 'wet' field, rocks cluster. `density` = instances per m^2 when size is None."""
    rng = np.random.default_rng(seed)
    lib = Library.get()
    if size is None:
        size = math.sqrt(n / density)
    hab = lambda x, y: 0.65 * _value_noise(x, y, 70.0, 21) + 0.35 * np.clip(x / size + 0.5, 0, 1)  # noqa: E731
    fields = {
        "bilberry": (lambda x, y: 0.6 * _value_noise(x, y, 5.0, 13) + 0.4 * _value_noise(x, y, 18.0, 14), 0.42),
        "heather": (lambda x, y: 0.6 * _value_noise(x, y, 4.0, 15) + 0.4 * _value_noise(x, y, 15.0, 16), 0.40),
        "fern": (lambda x, y: _value_noise(x, y, 22.0, 11), 0.58),
        "rock": (lambda x, y: _value_noise(x, y, 9.0, 17), 0.55),
        "boulder": (lambda x, y: _value_noise(x, y, 14.0, 18), 0.55),
    }
    kinds_sp, w_sp = zip(*HABITATS["spruce"].items())
    kinds_pr, w_pr = zip(*HABITATS["pine_rock"].items())
    w_sp = np.array(w_sp) / sum(w_sp)
    w_pr = np.array(w_pr) / sum(w_pr)
    out = []
    while len(out) < n:
        m = 4096
        P = rng.uniform(-size / 2, size / 2, (m, 2))
        h = hab(P[:, 0], P[:, 1]) > 0.5
        for (x, y), pine in zip(P, h):
            kinds, w = (kinds_pr, w_pr) if pine else (kinds_sp, w_sp)
            k = kinds[rng.choice(len(kinds), p=w)]
            f = fields.get(k)
            # rejection keeps the kind mix while clustering it in its field (bounded retries)
            if f is not None:
                ok = False
                for _ in range(12):
                    if f[0](x, y) > f[1] + rng.uniform(-0.05, 0.05):
                        ok = True
                        break
                    x, y = rng.uniform(-size / 2, size / 2, 2)
                    if (hab(x, y) > 0.5) != pine:
                        continue
                if not ok:
                    continue
            out.append(random_instance(k, x, y, z, rng, lib))
            if len(out) >= n:
                break
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--demo", type=int, default=0, help="scatter N demo instances on a plane")
    ap.add_argument("--size", type=float, default=None, help="demo square side (m)")
    ap.add_argument("--density", type=float, default=0.3, help="demo instances per m^2 (if no --size)")
    ap.add_argument("--instances", help="JSON file with a list of instance dicts")
    ap.add_argument("--out", required=True, help="output model directory")
    ap.add_argument("--name", default="props", help="Gazebo model name (dir basename should match)")
    ap.add_argument("--chunk", type=float, default=100.0)
    ap.add_argument("--no-collision", action="store_true")
    ap.add_argument("--no-plant-shadows", action="store_true")
    ap.add_argument("--lib", default=None)
    ap.add_argument("--seed", type=int, default=0)
    a = ap.parse_args()
    if a.demo:
        inst = demo_instances(a.demo, a.size, a.seed, density=a.density)
        with open(os.path.join(os.path.dirname(os.path.abspath(a.out)), a.name + "_instances.json"), "w") as f:
            json.dump(inst, f)
    elif a.instances:
        with open(a.instances) as f:
            inst = json.load(f)
    else:
        ap.error("need --demo N or --instances FILE")
    build_props(inst, a.out, a.name, a.chunk, () if a.no_collision else ("rock", "outcrop", "log"),
                lib_dir=a.lib, plant_shadows=not a.no_plant_shadows)


if __name__ == "__main__":
    main()
