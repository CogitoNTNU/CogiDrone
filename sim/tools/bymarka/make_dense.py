#!/usr/bin/env python3
"""Generate the dense Bymarka world: the same real terrain as bymarka.sdf, but a smaller search
area around the launch clearing with a forest as dense as the real one.

The laser data (1 m canopy height model) only resolves the dominant crowns, about 260 trees/ha in
closed forest. Real spruce/pine forest here has 600-1200 stems/ha plus saplings. So inside the
search area the canopy is filled in (Poisson-disk sampling, spacing from the local canopy height,
height from the canopy height model at that spot), young trees are added under and between the
crowns, and the ground clutter (bilberry, ferns, rocks, logs) is denser.

    python3 sim/tools/bymarka/make_world.py      # once: terrain, lakes, backdrop (shared)
    python3 sim/tools/bymarka/make_dense.py      # forest + props + people -> worlds/bymarka_dense.sdf
    python3 sim/tools/bymarka/make_dense.py --crops-only   # just the smaller crop worlds (below)

Crop worlds (CROPS): the same trees, props and people (same seeds), but only a square of the search
area gets the detailed forest and ground clutter. Every tree outside it is still there, as a cheap
far-tier impostor (8 triangles, no collision), so the horizon is the same forest, not a cliff edge.
Each crop adds a few people so a flight always passes several. The world origin (= drone spawn) stays
at the launch clearing, which every crop contains.

  bymarka_dense_q  300 x 300 m (1/4 of the dense area)
  bymarka_dense_s  150 x 150 m (1/16): trail through bog and forest, forest edge, lake shore

Writes (generated, git-ignored): sim/models/<world>_{forest,far,props}/, sim/worlds/<world>.sdf,
sim/worlds/<world>_people.yaml for bymarka_dense and every crop.
"""

import argparse
import math
import os
import shutil
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_world as mw  # noqa: E402

# trees/ and props/ both have their own glb.py: import one, drop it from the module cache, then the other.
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "props"))
from props_builder import build_props  # noqa: E402
sys.modules.pop("glb")
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "trees"))
from forest_builder import build_forest  # noqa: E402

NAME = "bymarka_dense"
CORE = mw.CORE
log = mw.log

# Crop worlds: suffix -> box (x0, y0, x1, y1) in world metres (east, north of the launch spot),
# forest chunk size, and the extra people as (zone, state) on top of the dense world's own people
# inside the box. Chosen from a map of the area: the launch clearing sits on open bog between two
# trails; west is a dense spruce stand on a ridge, east the shore of Kobberdammen. Stock YOLO finds
# the animated people (walking, waving) far more often than the static ones, so each crop has some.
CROPS = {
    "q": ((-150.0, -130.0, 150.0, 170.0), 25.0,
          [("open", "standing"), ("forest", "standing"), ("shore", "sitting"), ("open", "waving")]),
    "s": ((-80.0, -60.0, 70.0, 90.0), 25.0,
          [("open", "standing"), ("forest", "lying"), ("shore", "sitting"), ("forest", "standing"),
           ("rock", "sitting"), ("open", "walking"), ("open", "waving")]),
}


def window(origin, half):
    """1 m pixel window (r0, r1, c0, c1) of a +-half m square around the origin."""
    ox, oy, _ = origin
    cr, cc = CORE[3] - oy, ox - CORE[0]
    return (int(max(cr - half, 0)), int(min(cr + half, 2000)), int(max(cc - half, 0)), int(min(cc + half, 2000)))


class Grid:
    """Points with a per-point exclusion radius; fast 'is anything closer than d' queries."""

    def __init__(self, cell=2.0):
        self.cell, self.cells = cell, {}

    def add(self, x, y):
        self.cells.setdefault((int(x // self.cell), int(y // self.cell)), []).append((x, y))

    def free(self, x, y, d):
        k = int(math.ceil(d / self.cell))
        i, j = int(x // self.cell), int(y // self.cell)
        d2 = d * d
        for a in range(i - k, i + k + 1):
            for b in range(j - k, j + k + 1):
                for px, py in self.cells.get((a, b), ()):
                    if (px - x) ** 2 + (py - y) ** 2 < d2:
                        return False
        return True


def spacing(h):
    """Typical distance between neighbouring stems of height h (m) in a closed boreal stand:
    ~2 m in young thickets, ~3 m at 8 m height (700/ha), ~4.2 m for 15 m+ trees (400/ha)."""
    return np.clip(0.16 * h + 1.8, 2.0, 4.2)


def densify(m, origin, base, half, seed, spacing_mul=1.0, understory=True):
    """Extra tree instances inside the +-half m square (world frame, metres).
    base: the laser-detected trees (make_world.tree_instances), kept as they are."""
    rng = np.random.default_rng(seed)
    ox, oy, oz = origin
    r0, r1, c0, c1 = window(origin, half)
    chm = m["chm"][r0:r1, c0:c1]
    canopy = m["canopy"][r0:r1, c0:c1]
    osm = m["osm"]
    blocked = (m["water"][r0:r1, c0:c1] | (osm["building"][r0:r1, c0:c1] > 0.5)
               | (np.maximum(osm["path"], np.maximum(osm["track"], osm["road"]))[r0:r1, c0:c1] > 0.4))
    grid = Grid()
    inb = (np.abs(base["x"]) < half + 5) & (np.abs(base["y"]) < half + 5)
    for x, y in zip(base["x"][inb], base["y"][inb]):
        grid.add(float(x), float(y))

    def to_world(rr, cc):
        x = CORE[0] + (cc + c0) + rng.uniform(0, 1, len(rr)) - ox
        y = CORE[3] - (rr + r0) - rng.uniform(0, 1, len(rr)) - oy
        return x, y

    out_x, out_y, out_h, out_r, out_c, out_kind = [], [], [], [], [], []

    # 1) Canopy fill: every vegetated pixel is a candidate, random order, Poisson-disk acceptance.
    rr, cc = np.nonzero((chm >= 2.0) & ~blocked)
    order = rng.permutation(len(rr))
    rr, cc = rr[order], cc[order]
    xs, ys = to_world(rr, cc)
    hs = chm[rr, cc] * rng.uniform(0.88, 1.02, len(rr))
    ds = spacing(hs) * spacing_mul
    for x, y, h, d, a, b in zip(xs, ys, hs, ds, rr, cc):
        if abs(x) < half and abs(y) < half and grid.free(x, y, d):
            grid.add(x, y)
            out_x.append(x); out_y.append(y); out_h.append(h); out_r.append(a); out_c.append(b); out_kind.append(0)
    n_fill = len(out_x)

    if understory:
        # 2) Young trees: under the canopy (shade-tolerant spruce), more in gaps and along edges.
        p = 0.022 * mw.smoothstep(0.3, 0.8, canopy) + 0.045 * np.clip(1 - np.abs(canopy - 0.45) / 0.3, 0, 1)
        # 3) Scattered small pines, birches and junipers out on the heath and bog edges.
        p += 0.0025 * (canopy < 0.25)
        p *= ~blocked
        rr, cc = np.nonzero(rng.random(chm.shape) < p)
        xs, ys = to_world(rr, cc)
        for x, y, a, b in zip(xs, ys, rr, cc):
            if abs(x) < half and abs(y) < half and grid.free(x, y, 1.1):
                grid.add(x, y)
                h = 0.5 + 3.5 * rng.random() ** 1.6
                out_x.append(x); out_y.append(y); out_h.append(h); out_r.append(a); out_c.append(b); out_kind.append(1)

    x, y, h = np.array(out_x), np.array(out_y), np.array(out_h)
    R, C, kind = np.array(out_r) + r0, np.array(out_c) + c0, np.array(out_kind)
    u = rng.random(len(x))
    species = mw.assign_species(m, R, C, np.where(kind == 1, 10.0, h), u, edge_birch=0.22)
    # Saplings: mostly spruce under spruce, birch/pine in the open, junipers on dry open rock.
    sap = kind == 1
    open_ = m["canopy"][R, C] < 0.25
    v = rng.random(len(x))
    species[sap & ~open_ & (v < 0.72)] = 0
    species[sap & ~open_ & (v >= 0.72) & (v < 0.92)] = 2
    species[sap & open_ & (v < 0.35)] = 1
    species[sap & open_ & (v >= 0.35) & (v < 0.65)] = 2
    species[sap & open_ & (v >= 0.65)] = 4
    species[(species == 3) & sap] = 2  # no alder saplings in the library sizes
    z = m["dtm"][R, C] - oz
    log(f"densify +-{half:.0f} m: {n_fill} canopy fill trees, {int(sap.sum())} young trees/shrubs")
    return {"x": x, "y": y, "z": z, "height": h, "species": species}


def instances(base, extra, near_half):
    out = []
    names = mw.SPECIES
    for src in (base, extra):
        for i in range(len(src["x"])):
            x, y = float(src["x"][i]), float(src["y"][i])
            sp = names[int(src["species"][i])]
            h = float(src["height"][i])
            if sp == "juniper":
                h = min(h, 4.0)
            near = abs(x) < near_half and abs(y) < near_half
            out.append({"species": sp, "x": x, "y": y, "z": float(src["z"][i]), "height": h,
                        "tier": "near" if near else "far"})
    return out


def in_box(x, y, box, margin=0.0):
    x0, y0, x1, y1 = box
    return (x0 - margin <= x) & (x <= x1 + margin) & (y0 - margin <= y) & (y <= y1 + margin)


def extra_people(m, origin, box, plan, taken, start_id, seed):
    """People for the (zone, state) plan inside the box, >= 25 m from anyone in `taken` and from
    the launch spot. Same output format as make_world.place_people. Zones without room fall back
    to open ground."""
    rng = np.random.default_rng(seed)
    ox, oy, oz = origin
    paths = np.maximum(m["osm"]["path"], m["osm"]["track"])
    shore = mw.ndimage.binary_dilation(m["water"], iterations=4) & ~m["water"]
    zones = {
        "forest": (m["canopy"] > 0.6) & ~m["water"],
        "path": paths > 0.5,
        "shore": shore,
        "rock": (m["rough"] > 0.4) & (m["canopy"] < 0.4) & ~m["water"],
        "open": (m["canopy"] < 0.3) & ~m["water"] & (m["osm"]["wetland"] < 0.5),
    }
    x0, y0, x1, y1 = box
    c0, c1 = int(x0 + ox - CORE[0]), int(x1 + ox - CORE[0])
    r0, r1 = int(CORE[3] - (y1 + oy)), int(CORE[3] - (y0 + oy))
    people, actors, taken = [], [], list(taken) + [(0.0, 0.0)]
    for k, (zone, state) in enumerate(plan):
        pick = None
        for z in (zone, "open"):
            rr, cc = np.nonzero(zones[z][r0 + 8:r1 - 8, c0 + 8:c1 - 8])  # 8 m inside the box edge
            for _ in range(400 if len(rr) else 0):
                i = rng.integers(len(rr))
                x = float(CORE[0] + cc[i] + c0 + 8 + 0.5 - ox)
                y = float(CORE[3] - (rr[i] + r0 + 8) - 0.5 - oy)
                if all(math.hypot(x - a, y - b) > 25 for a, b in taken):
                    pick = (x, y, rr[i] + r0 + 8, cc[i] + c0 + 8, z)
                    break
            if pick:
                break
        if pick is None:
            log(f"no room for an extra {zone}/{state} person")
            continue
        x, y, R, C, z = pick
        taken.append((x, y))
        yaw = float(rng.uniform(-math.pi, math.pi))
        p = {"id": f"person_{start_id + k:02d}", "zone": z, "state": state, "x": round(x, 2), "y": round(y, 2),
             "z": round(float(m["dtm"][R, C]) - oz, 2)}
        if state in ("walking", "waving"):
            p["model"] = "actor"
            if state == "walking":
                length = float(rng.uniform(8, 14))
                xe, ye = x + length * math.cos(yaw), y + length * math.sin(yaw)
                ce = int(np.clip(xe + ox - CORE[0], 0, 1999)); re_ = int(np.clip(CORE[3] - (ye + oy), 0, 1999))
                p["path"] = [[round(x, 2), round(y, 2)], [round(xe, 2), round(ye, 2)]]
                p["z1"] = round(float(m["dtm"][re_, ce]) - oz, 2)
            actors.append((p, yaw))
        else:
            variants = mw.local_people(state)
            if state == "sitting" or not variants:
                p["model"] = "rescue_randy_sitting" if state == "sitting" else "standing_person"
            else:
                p["model"] = variants[rng.integers(len(variants))]
            if state == "lying":
                p["face"], p["injured"] = "up", True
            people.append((p, yaw))
        p["yaw"] = round(yaw, 2)
    return people, actors


def build_crop(suffix, masks, origin, inst, props, people, actors, props_chunk=50.0):
    """One crop world: detailed trees + props inside the box, impostors outside, people inside."""
    box, chunk, plan = CROPS[suffix]
    name = f"{NAME}_{suffix}"
    t0 = mw.time.time()
    near = [dict(d, tier="near") for d in inst if in_box(d["x"], d["y"], box)]
    far = [dict(d, tier="far") for d in inst if not in_box(d["x"], d["y"], box)]
    w, h = box[2] - box[0], box[3] - box[1]
    log(f"{name}: {w:.0f} x {h:.0f} m, {len(near)} detailed trees ({len(near) / (w * h / 1e4):.0f}/ha), "
        f"{len(far)} impostors outside")
    for sfx, trees, ch in (("forest", near, chunk), ("far", far, 250.0)):
        out = os.path.join(mw.MODELS, f"{name}_{sfx}")
        shutil.rmtree(out, ignore_errors=True)
        build_forest(trees, mw.MODELS, f"{name}_{sfx}", chunk_size=ch, collision="mesh")
    if props is not None:
        sel = [p for p in props if in_box(p["x"], p["y"], box, margin=5.0)]
        out = os.path.join(mw.MODELS, f"{name}_props")
        shutil.rmtree(out, ignore_errors=True)
        build_props(sel, out, f"{name}_props", chunk_size=min(props_chunk, chunk * 2))
    keep_p = [(p, yaw) for p, yaw in people if in_box(p["x"], p["y"], box)]
    keep_a = [(p, yaw) for p, yaw in actors if in_box(p["x"], p["y"], box)]
    taken = [(p["x"], p["y"]) for p, _ in keep_p + keep_a]
    start = 1 + max(int(p["id"].split("_")[1]) for p, _ in people + actors)
    xp, xa = extra_people(masks, origin, box, plan, taken, start, seed=100 + ord(suffix))
    keep_p += xp
    keep_a += xa
    mw.write_people_yaml(keep_p, keep_a, 5, name=name)
    log(f"{name}: {len(keep_p) + len(keep_a)} people ({len(xp) + len(xa)} added for this crop)")
    mw.write_world(origin, [f"{name}_forest", f"{name}_far", f"{name}_props"],
                   mw.people_sdf(keep_p, keep_a), name=name)
    log(f"wrote worlds/{name}.sdf in {mw.time.time() - t0:.0f} s")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--half", type=float, default=300.0, help="search area is +-half m around the launch spot")
    ap.add_argument("--people", type=int, default=20)
    ap.add_argument("--props-density", type=float, default=2.0)
    ap.add_argument("--far-fill", type=float, default=1.6,
                    help="spacing multiplier for the cheap canopy fill outside the search area (0 = none)")
    ap.add_argument("--chunk", type=float, default=50.0)
    ap.add_argument("--skip-props", action="store_true", help="keep the props built last time")
    ap.add_argument("--crops", default=",".join(CROPS), help="crop worlds to build (default %(default)s, '' = none)")
    ap.add_argument("--crops-only", action="store_true", help="build only the crop worlds, keep bymarka_dense")
    args = ap.parse_args()
    crops = [c for c in args.crops.split(",") if c]
    for c in crops:
        if c not in CROPS:
            sys.exit(f"unknown crop {c}: {', '.join(CROPS)}")

    for need in ("bymarka_terrain", "bymarka_water", "bymarka_backdrop"):
        if not os.path.exists(os.path.join(mw.MODELS, need, "model.sdf")):
            sys.exit(f"{need} missing: run make_world.py first (it builds the shared terrain)")
    masks = mw.build_masks()
    origin = mw.choose_origin(masks)
    base = mw.tree_instances(masks, origin)
    extra = densify(masks, origin, base, args.half, seed=11)
    if args.far_fill > 0:  # thicken the far forest too (8-triangle impostors, cheap)
        far = densify(masks, origin, base, 1000.0, seed=12, spacing_mul=args.far_fill, understory=False)
        keep = (np.abs(far["x"]) >= args.half) | (np.abs(far["y"]) >= args.half)
        extra = {k: np.concatenate([extra[k], far[k][keep]]) for k in extra}
    inst = instances(base, extra, args.half + 10)
    n_near = sum(1 for d in inst if d["tier"] == "near")
    log(f"{len(inst)} trees, {n_near} detailed in the search area "
        f"({n_near / (2 * args.half / 100) ** 2:.0f}/ha incl. open ground)")
    # Two models: detailed trees in small chunks (Gazebo culls per visual, so small chunks cull
    # well), the far impostors in big chunks (fewer draw calls).
    if not args.crops_only:
        for suffix, tier, chunk in (("forest", "near", args.chunk), ("far", "far", 250.0)):
            out = os.path.join(mw.MODELS, f"{NAME}_{suffix}")
            shutil.rmtree(out, ignore_errors=True)
            build_forest([d for d in inst if d["tier"] == tier], mw.MODELS, f"{NAME}_{suffix}", chunk_size=chunk,
                         collision="mesh")

    trees = {k: np.concatenate([base[k], extra[k]]) for k in ("x", "y", "z", "height")}
    props = None
    if not args.skip_props:
        props = mw.prop_instances(masks, origin, trees, density=args.props_density, half=args.half + 20)
        kinds = {}
        for p in props:
            kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
        log(f"{len(props)} props: {kinds}")
        if not args.crops_only:
            out = os.path.join(mw.MODELS, f"{NAME}_props")
            shutil.rmtree(out, ignore_errors=True)
            build_props(props, out, f"{NAME}_props", chunk_size=args.chunk)

    people, actors = mw.place_people(masks, origin, trees, n=args.people, half=args.half)
    if not args.crops_only:
        mw.write_people_yaml(people, actors, 5, name=NAME)
        log(f"{len(people) + len(actors)} people ({len(actors)} animated)")
        mw.write_world(origin, [f"{NAME}_forest", f"{NAME}_far", f"{NAME}_props"], mw.people_sdf(people, actors),
                       name=NAME)
        log(f"wrote worlds/{NAME}.sdf")
    for c in crops:
        build_crop(c, masks, origin, inst, props, people, actors, props_chunk=args.chunk)


if __name__ == "__main__":
    main()
