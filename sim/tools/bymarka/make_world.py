#!/usr/bin/env python3
"""Generate the Bymarka world: real terrain, lakes, forest and paths around Kobberdammen /
Gråkallen in Trondheim, from open map data (see data.py and sim/ATTRIBUTION.md).

    python3 sim/tools/bymarka/make_world.py            # everything (first run downloads ~200 MB)
    python3 sim/tools/bymarka/make_world.py --tex 2048 # lighter ground textures
    python3 sim/tools/bymarka/make_world.py --retex --tex-far 1024  # re-encode the baked tiles only
    python3 sim/tools/bymarka/make_world.py --skip-terrain --forest --props   # trees + ground clutter
    python3 sim/tools/bymarka/make_dense.py            # the smaller, denser search-area world

Writes (generated, git-ignored, re-run instead of editing):
    sim/models/bymarka_terrain/   8 x 8 ground tiles (2 m mesh) with baked 4K textures (BC1 .dds
                                  colour, 4K near the launch spot, --tex-far outside; masters in materials/bake/)
    sim/models/bymarka_water/     lakes at their real water level
    sim/models/bymarka_backdrop/  30 x 30 km of real terrain around it (hills, fjord, city)
    sim/worlds/bymarka.sdf
    sim/worlds/bymarka_trees.npz  tree instances (position, height, species) from the laser data

World frame: ENU, metres, origin at the drone's launch spot (a flat clearing near the centre),
z = 0 at the ground there. The real height above sea level of the origin is in the world's
<spherical_coordinates>.
"""

import argparse
import json
import math
import os
import shutil
import sys
import time

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data  # noqa: E402
from dxt import save_bc1  # noqa: E402

SIM = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODELS = os.path.join(SIM, "models")
WORLDS = os.path.join(SIM, "worlds")

CENTRE = (563900.0, 7033500.0)  # EPSG:25832, Kobberdammen
HALF = 1000.0  # core area: 2 x 2 km at 1 m
CORE = (CENTRE[0] - HALF, CENTRE[1] - HALF, CENTRE[0] + HALF, CENTRE[1] + HALF)
FAR_HALF = 15000.0  # backdrop: 30 x 30 km at 25 m
FAR = (CENTRE[0] - FAR_HALF, CENTRE[1] - FAR_HALF, CENTRE[0] + FAR_HALF, CENTRE[1] + FAR_HALF)
MESH_RES = 2.0  # ground mesh grid
TILES = 8  # tiles per side (250 m each)
COLLISION_RES = 4.0


def log(msg):
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------------------------------------
# Masks at 1 m (row 0 = north, col 0 = west)


def rasterize_osm(ways, shape, res, bbox, scale=1):
    """OSM layers as 1 m (x scale) masks: paths, tracks, roads, ski trails, streams, wetland,
    bare rock, buildings (polygons)."""
    h, w = shape[0] * scale, shape[1] * scale
    px = scale / res

    def to_px(pts):
        return [((e - bbox[0]) * px, (bbox[3] - n) * px) for e, n in pts]

    layers = {k: Image.new("L", (w, h), 0) for k in
              ("path", "track", "road", "asphalt", "ski", "stream", "wetland", "bare_rock", "building")}
    draw = {k: ImageDraw.Draw(v) for k, v in layers.items()}
    widths = {  # m
        "path": 1.2, "footway": 1.5, "bridleway": 2.0, "cycleway": 2.5, "steps": 1.5,
        "track": 3.2, "service": 4.0, "unclassified": 5.0, "residential": 5.5, "tertiary": 6.5,
        "secondary": 7.0,
    }
    for way in ways:
        t, pts = way["tags"], way["pts"]
        if len(pts) < 2:
            continue
        p = to_px(pts)
        hw = t.get("highway")
        if hw in widths:
            layer = ("path" if hw in ("path", "footway", "bridleway", "steps", "cycleway")
                     else "track" if hw == "track" else "road")
            if t.get("surface") == "asphalt" or hw in ("tertiary", "secondary"):
                layer = "asphalt"
            draw[layer].line(p, fill=255, width=max(1, int(widths[hw] * px)), joint="curve")
        elif "piste:type" in t and t["piste:type"] == "nordic" and "highway" not in t:
            draw["ski"].line(p, fill=255, width=max(1, int(4.0 * px)), joint="curve")
        if t.get("waterway") in ("stream", "ditch", "river", "brook"):
            draw["stream"].line(p, fill=255, width=max(1, int((2.0 if t["waterway"] != "river" else 6) * px)))
        closed = pts[0] == pts[-1] and len(pts) > 3
        if closed and t.get("natural") == "wetland":
            draw["wetland"].polygon(p, fill=255)
        if closed and t.get("natural") == "bare_rock":
            draw["bare_rock"].polygon(p, fill=255)
        if closed and "building" in t:
            draw["building"].polygon(p, fill=255)
    return {k: np.asarray(v, np.float32) / 255 for k, v in layers.items()}


def build_masks():
    log("loading data")
    dtm = data.height("dtm", CORE, 1)
    dom = data.height("dom", CORE, 1)
    topo = data.topo(CORE, 2000)
    species = data.sr16("SRRTRESLAG", CORE, 500)
    ways = data.osm(CORE)
    osm = rasterize_osm(ways, dtm.shape, 1.0, CORE)

    log("masks")
    water = np.abs(topo - np.array([220, 242, 242])).sum(2) < 25
    water = ndimage.binary_opening(ndimage.binary_closing(water, iterations=3), iterations=2)
    # Remove tiny specks (map symbols), keep lakes and ponds.
    lab, n = ndimage.label(water)
    sizes = ndimage.sum(water, lab, range(1, n + 1))
    water = np.isin(lab, np.nonzero(sizes > 150)[0] + 1)

    chm = np.clip(dom - dtm, 0, 45)
    chm[water] = 0
    chm[osm["building"] > 0.5] = 0  # buildings are not trees
    smooth = ndimage.gaussian_filter(dtm, 1.0)
    gy, gx = np.gradient(smooth)
    slope = np.degrees(np.arctan(np.hypot(gx, gy)))
    rough = np.abs(dtm - ndimage.gaussian_filter(dtm, 4.0))  # outcrops, boulders, ledges
    canopy = ndimage.gaussian_filter((chm > 2.5).astype(np.float32), 2.0)
    return {
        "dtm": dtm, "dom": dom, "chm": chm, "slope": slope, "rough": rough, "canopy": canopy,
        "water": water, "species_rgb": species, "osm": osm, "ways": ways, "topo": topo,
    }


def preview_masks(m, path):
    rgb = np.zeros(m["dtm"].shape + (3,), np.float32)
    rgb[:] = (0.55, 0.6, 0.4)
    rock = smoothstep(28, 45, m["slope"]) + smoothstep(0.3, 0.8, m["rough"]) * (m["chm"] < 2)
    rgb = rgb * (1 - np.clip(rock, 0, 1)[..., None]) + np.clip(rock, 0, 1)[..., None] * (0.6, 0.6, 0.6)
    rgb = rgb * (1 - m["canopy"][..., None]) + m["canopy"][..., None] * (0.1, 0.3, 0.1)
    for k, c in (("wetland", (0.6, 0.5, 0.3)), ("ski", (0.75, 0.8, 0.5)), ("track", (0.8, 0.7, 0.5)),
                 ("road", (0.85, 0.8, 0.7)), ("asphalt", (0.3, 0.3, 0.3)), ("path", (0.9, 0.5, 0.3)),
                 ("stream", (0.2, 0.4, 0.9)), ("building", (0.8, 0.1, 0.1))):
        a = m["osm"][k][..., None]
        rgb = rgb * (1 - a) + a * np.array(c)
    rgb[m["water"]] = (0.15, 0.25, 0.4)
    Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((1000, 1000)).save(path)



# ---------------------------------------------------------------------------------------------
# Heights, origin, lakes


def grid_heights(dtm, water):
    """Ground height on the mesh grid (MESH_RES) + lake beds lowered under the water."""
    n = int(2 * HALF / MESH_RES) + 1
    # Grid point k sits at x = CORE[0] + k * MESH_RES; DTM pixel centres are at CORE[0] + 0.5 + j.
    idx = np.arange(n) * MESH_RES - 0.5
    rr, cc = np.meshgrid(np.clip(idx, 0, dtm.shape[0] - 1), np.clip(idx, 0, dtm.shape[1] - 1), indexing="ij")
    h = ndimage.map_coordinates(dtm, [rr, cc], order=1)
    inside = ndimage.distance_transform_edt(water)
    depth = np.clip(inside * 0.25, 0, 4.0)
    h -= ndimage.map_coordinates(depth, [rr, cc], order=1)
    return h


def lakes(dtm, water):
    """[(mask, level)] for each lake: the DTM is flat at the water surface."""
    lab, n = ndimage.label(water)
    out = []
    for i in range(1, n + 1):
        m = lab == i
        out.append((m, float(np.median(dtm[m]))))
    return out


def choose_origin(m):
    """A flat, open, dry spot near the centre for the launch: no trees within 12 m."""
    open_ = (m["chm"] < 1.0) & ~m["water"] & (m["osm"]["building"] < 0.5) & (m["osm"]["wetland"] < 0.5)
    clear = ndimage.binary_erosion(open_, iterations=12)
    flat = ndimage.maximum_filter(m["slope"], 15) < 6
    ok = clear & flat
    yy, xx = np.nonzero(ok)
    c = m["dtm"].shape[0] / 2
    k = np.argmin((yy - c) ** 2 + (xx - c) ** 2)
    r, col = int(yy[k]), int(xx[k])
    e, n = CORE[0] + col + 0.5, CORE[3] - r - 0.5
    return e, n, float(m["dtm"][r, col])


# ---------------------------------------------------------------------------------------------
# Meshes


def write_gltf(path, verts, tris, normals, uv):
    used_bytes = []
    blobs = [verts.astype(np.float32), normals.astype(np.float32), uv.astype(np.float32),
             tris.astype(np.uint32).ravel()]
    buf, views, acc = b"", [], []
    for i, b in enumerate(blobs):
        views.append({"buffer": 0, "byteOffset": len(buf), "byteLength": b.nbytes})
        buf += b.tobytes()
        if b.ndim == 1:
            acc.append({"bufferView": i, "componentType": 5125, "count": len(b), "type": "SCALAR"})
        else:
            a = {"bufferView": i, "componentType": 5126, "count": len(b), "type": f"VEC{b.shape[1]}"}
            if i == 0:
                a["min"], a["max"] = blobs[0].min(0).tolist(), blobs[0].max(0).tolist()
            acc.append(a)
    del used_bytes
    base = os.path.splitext(os.path.basename(path))[0]
    gltf = {
        "asset": {"version": "2.0", "generator": "sim/tools/bymarka/make_world.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                    "indices": 3, "material": 0}]}],
        "materials": [{"pbrMetallicRoughness": {"metallicFactor": 0, "roughnessFactor": 0.9}}],
        "buffers": [{"byteLength": len(buf), "uri": base + ".bin"}],
        "bufferViews": views, "accessors": acc,
    }
    with open(path, "w") as f:
        json.dump(gltf, f)
    with open(os.path.splitext(path)[0] + ".bin", "wb") as f:
        f.write(buf)


def write_obj(path, verts, tris, normals):
    with open(path, "w") as f:
        f.write("".join(f"v {a:.2f} {b:.2f} {c:.2f}\n" for a, b, c in verts))
        f.write("".join(f"vn {a:.3f} {b:.3f} {c:.3f}\n" for a, b, c in normals))
        f.write("".join(f"f {a}//{a} {b}//{b} {c}//{c}\n" for a, b, c in tris + 1))


def grid_mesh(z, x0, y_top, res):
    """Regular grid (row 0 = north) -> verts (world x, y, z), triangles, normals."""
    rows, cols = z.shape
    xs = x0 + np.arange(cols) * res
    ys = y_top - np.arange(rows) * res
    X, Y = np.meshgrid(xs, ys)
    verts = np.stack([X.ravel(), Y.ravel(), z.ravel()], 1)
    idx = np.arange(rows * cols).reshape(rows, cols)
    a, b = idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel()
    c, d = idx[1:, 1:].ravel(), idx[1:, :-1].ravel()
    tris = np.concatenate([np.stack([a, d, b], 1), np.stack([b, d, c], 1)])  # counter-clockwise from above
    gy, gx = np.gradient(z, res)
    nrm = np.stack([-gx, gy, np.ones_like(z)], -1).reshape(-1, 3)  # row index grows southwards
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    return verts, tris, nrm



# ---------------------------------------------------------------------------------------------
# Baked ground textures. Gazebo can't blend textures by a mask on a mesh, so every tile gets its
# own texture with the ground layers already blended, shaded under trees and colour-varied.

# name -> (source, repeat in m, colour multiplier)
LAYERS = {
    "moss": (("acg", "Moss004"), 2.5, (1.0, 1.0, 1.0)),           # spruce forest moss carpet
    "bilberry": (("acg", "Moss002"), 3.0, (0.85, 0.95, 0.8)),     # bilberry/moss mix
    "litter": (("ph", "forest_ground_04"), 3.0, (0.8, 0.8, 0.75)),  # needles, twigs, dirt
    "heath": (("acg", "Ground037"), 3.5, (0.66, 0.66, 0.46)),      # dry heath / lichen-grass, heather
    "grass": (("acg", "Grass001"), 3.0, (0.86, 0.84, 0.56)),      # tussocky moor grass, straw-tinged
    "rock": (("ph", "lichen_rock"), 4.0, "grey"),                 # grey rock, pale lichen
    "rock2": (("acg", "Rock064"), 5.0, (0.85, 0.88, 0.85)),       # mossy rock
    "bog": (("acg", "Grass001"), 3.0, (0.86, 0.84, 0.52)),        # sedge / deergrass, pale yellow-green
    "bog2": (("acg", "Moss004"), 2.0, (0.9, 0.95, 0.62)),         # sphagnum hummocks, olive
    "path": (("acg", "Ground078"), 2.0, (0.72, 0.68, 0.62)),
    "gravel": (("acg", "Ground081"), 3.0, (0.9, 0.9, 0.88)),
    "asphalt": (("acg", "Asphalt031"), 4.0, (0.75, 0.75, 0.75)),
    "shore": (("acg", "Gravel041"), 2.0, (0.85, 0.85, 0.82)),
}


def load_layers(texel):
    """Each layer resized so ONE layer pixel = ONE baked texel: baking is then plain tiling."""
    out = {}
    for name, ((src, asset), repeat, mul) in LAYERS.items():
        t = data.ambientcg(asset) if src == "acg" else data.polyhaven_tex(asset)
        p = max(16, int(round(repeat / texel)))
        img = {}
        for k in ("color", "normal"):
            a = Image.fromarray((t[k] * 255).astype(np.uint8)).resize((p, p), Image.LANCZOS)
            img[k] = np.asarray(a, np.float32) / 255
        if mul == "grey":  # brownish lichen rock -> light grey with grey-green lichen
            g = img["color"].mean(2, keepdims=True)
            img["color"] = np.clip(0.35 * img["color"] + 0.65 * g, 0, 1) * np.array((1.65, 1.7, 1.62))
        else:
            img["color"] = img["color"] * np.array(mul)
        img["color"] = np.clip(img["color"], 0, 1)
        out[name] = img
    return out


def noise_field(n, seed, beta):
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(n)
    r = np.hypot(*np.meshgrid(f, f))
    r[0, 0] = 1
    a = np.real(np.fft.ifft2(np.fft.fft2(rng.standard_normal((n, n))) / r**beta))
    return ((a - a.mean()) / a.std()).astype(np.float32)


_BAKE = {}  # shared with worker processes (fork)


def bake_setup(m, tex):
    texel = 2 * HALF / TILES / tex
    _BAKE.update(m=m, tex=tex, texel=texel, layers=load_layers(texel))
    # World-continuous noise at 0.5 m (no seams between tiles): fine and coarse.
    _BAKE["n_fine"] = noise_field(4096, 11, 1.1)
    _BAKE["n_mid"] = noise_field(4096, 12, 1.5)
    _BAKE["n_coarse"] = noise_field(4096, 13, 2.0)


def _sample(a, rows, cols, order=1):
    return ndimage.map_coordinates(a, [rows, cols], order=order, mode="nearest").astype(np.float32)


def bake_tile(ij):
    i, j = ij  # i = row (north to south), j = column
    m, tex, texel, L = _BAKE["m"], _BAKE["tex"], _BAKE["texel"], _BAKE["layers"]
    size = 2 * HALF / TILES
    x0, y1 = CORE[0] + j * size, CORE[3] - i * size
    g0r, g0c = int(round(i * size / texel)), int(round(j * size / texel))  # global texel offset
    color = np.zeros((tex, tex, 3), np.float32)
    normal = np.zeros((tex, tex, 3), np.float32)
    # Paths etc. drawn straight at texel resolution for crisp edges.
    osm = rasterize_osm(m["ways"], (tex, tex), texel, (x0, y1 - size, x0 + size, y1))
    for half in range(4):  # 4 strips to bound memory
        r0, r1 = half * tex // 4, (half + 1) * tex // 4
        rr, cc = np.meshgrid(np.arange(r0, r1), np.arange(tex), indexing="ij")
        # 1 m mask pixel coordinates of each texel centre
        mr = (i * size + (rr + 0.5) * texel) - 0.5
        mc = (j * size + (cc + 0.5) * texel) - 0.5
        S = lambda a: _sample(a, mr, mc)  # noqa: E731
        nr, nc = mr * 2 % 4096, mc * 2 % 4096
        nf = _sample(_BAKE["n_fine"], nr, nc)
        nm = _sample(_BAKE["n_mid"], nr, nc)
        ncs = _sample(_BAKE["n_coarse"], nr, nc)
        canopy, chm, slope = S(m["canopy"]), S(m["chm"]), S(m["slope"])
        rough = S(m["rough"])
        water = S(m["water"].astype(np.float32))
        wet = S(ndimage.gaussian_filter(m["osm"]["wetland"], 3))
        bare = S(m["osm"]["bare_rock"])

        def edge(w, soft=0.18, k=0.35):  # noisy, organic transition
            return smoothstep(0.5 - soft, 0.5 + soft, w + k * nf * 0.5)

        w = {}
        forest = edge(canopy * 1.15)
        open_ = 1 - forest
        w["moss"] = forest * smoothstep(-0.6, 0.6, nm)
        w["bilberry"] = forest * (1 - smoothstep(-0.6, 0.6, nm)) * 0.8
        w["litter"] = forest * smoothstep(0.85, 1.0, canopy) * smoothstep(0.0, 1.2, -ncs) * 0.9
        w["heath"] = open_ * smoothstep(-0.8, 0.8, ncs + 0.6 + 0.5 * nf)
        w["grass"] = open_ * (1 - smoothstep(-0.8, 0.8, ncs + 0.6 + 0.5 * nf))
        rock = np.clip(smoothstep(30, 48, slope) + 1.3 * smoothstep(0.25, 0.7, rough) * (1 - forest * 0.7)
                       + bare * 0.7, 0, 1)
        rock = edge(rock)
        bog = edge(wet * (1 - rock))
        for k in w:
            w[k] *= (1 - rock) * (1 - bog)
        w["rock"] = rock * smoothstep(-0.5, 0.5, nm + 0.2)
        w["rock2"] = rock * (1 - smoothstep(-0.5, 0.5, nm + 0.2))
        w["bog"] = bog * smoothstep(-0.7, 0.7, nm - 0.2)
        w["bog2"] = bog * (1 - smoothstep(-0.7, 0.7, nm - 0.2))
        shore = edge(S(ndimage.gaussian_filter(m["water"].astype(np.float32), 2.5)) * 2.2) * (1 - water)
        stream = S(ndimage.gaussian_filter(m["osm"]["stream"], 1.0))
        o = {k: v[r0:r1] for k, v in osm.items()}
        over = [("shore", np.maximum(shore, stream * 0.9)), ("grass", o["ski"] * 0.8), ("path", o["path"]),
                ("gravel", np.maximum(o["track"], np.maximum(o["road"], o["building"]))),
                ("asphalt", o["asphalt"])]
        for k, a in over:
            a = np.clip(ndimage.gaussian_filter(a, 1.2) if k != "shore" else a, 0, 1)
            a = smoothstep(0.25, 0.75, a + 0.25 * nf * a)
            for kk in w:
                w[kk] *= 1 - a
            w[k] = w.get(k, 0) + a
        total = sum(w.values()) + 1e-6
        rows = (g0r + rr) % 1, None  # placeholder (unused)
        del rows
        for k, wk in w.items():
            lay = L[k]
            p = lay["color"].shape[0]
            tr, tc = (g0r + rr) % p, (g0c + cc) % p
            wk = (wk / total)[..., None]
            color[r0:r1] += wk * lay["color"][tr, tc]
            normal[r0:r1] += wk * (lay["normal"][tr, tc] * 2 - 1)
        # Large-scale colour variation and shade under the canopy (ambient occlusion).
        # Variation at every scale: tussocks and shrubs (0.5-3 m), patches (5-30 m), slopes.
        vary = 1 + 0.10 * ncs + 0.07 * nm + 0.10 * nf * (1 - sum(w.get(k, 0) for k in ("path", "gravel", "asphalt")) / total)
        ao = 1 - 0.35 * smoothstep(0.3, 1.0, canopy) - 0.15 * smoothstep(8, 25, chm) * canopy
        wet_dark = 1 - 0.3 * stream - 0.25 * shore * (1 - water)
        color[r0:r1] *= (vary * ao * wet_dark)[..., None]
        # Greener moss where it's moist (low slope, under canopy), yellower on dry ridges.
        color[r0:r1, :, 0] *= 1 + 0.06 * ncs + 0.04 * nf
        color[r0:r1, :, 1] *= 1 + 0.03 * nm
        color[r0:r1, :, 2] *= 1 - 0.08 * ncs
        # Fine relief from the 1 m terrain model (the mesh only has 2 m).
        relief = S(m["relief"])
        gy, gx = np.gradient(relief, texel)
        normal[r0:r1, :, 0] += -gx * 0.7
        normal[r0:r1, :, 1] += gy * 0.7  # +green = north (image up)
    normal[..., 2] = np.maximum(normal[..., 2], 0.2)
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    out = os.path.join(MODELS, "bymarka_terrain", "materials", "bake")
    Image.fromarray((np.clip(color, 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(out, f"tile_{i}_{j}_color.jpg"), quality=90)
    Image.fromarray(((normal * 0.5 + 0.5) * 255).astype(np.uint8)).save(
        os.path.join(out, f"tile_{i}_{j}_normal.jpg"), quality=92)
    return finish_tile(ij)


# Texture sizes per tile. Gazebo's server (drone camera) and GUI each keep every texture in GPU
# memory, which on the Jetson is system RAM. Full size only where the drone searches; colour as
# BC1 (1/8 of the RGBA8 Ogre makes from a JPG); normal maps at half the colour size (Ogre keeps
# them as RG8, and the relief they add is subtle).
_FINISH = {"origin": (0.0, 0.0, 0.0), "tex": 4096, "tex_far": 2048}


def tile_tex(i, j):
    """(colour, normal) texture size for tile (i, j): full inside the NEAR_RADIUS square
    around the launch spot, tex_far outside."""
    size = 2 * HALF / TILES
    ox, oy, _ = _FINISH["origin"]
    x0, y1 = CORE[0] + j * size - ox, CORE[3] - i * size - oy
    near = x0 < NEAR_RADIUS and x0 + size > -NEAR_RADIUS and y1 > -NEAR_RADIUS and y1 - size < NEAR_RADIUS
    c = _FINISH["tex"] if near else min(_FINISH["tex"], _FINISH["tex_far"])
    return c, c // 2


def finish_tile(ij):
    """Baked masters (materials/bake) -> the textures Gazebo loads (materials/textures)."""
    i, j = ij
    root = os.path.join(MODELS, "bymarka_terrain", "materials")
    c, nsz = tile_tex(i, j)
    color = Image.open(os.path.join(root, "bake", f"tile_{i}_{j}_color.jpg")).convert("RGB")
    save_bc1(os.path.join(root, "textures", f"tile_{i}_{j}_color.dds"),
             np.asarray(color.resize((c, c), Image.LANCZOS) if color.size[0] != c else color))
    normal = Image.open(os.path.join(root, "bake", f"tile_{i}_{j}_normal.jpg")).convert("RGB")
    if normal.size[0] != nsz:
        n = np.asarray(normal.resize((nsz, nsz), Image.LANCZOS), np.float32) / 127.5 - 1
        n /= np.linalg.norm(n, axis=2, keepdims=True) + 1e-6
        normal = Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8))
    normal.save(os.path.join(root, "textures", f"tile_{i}_{j}_normal.jpg"), quality=92)
    return ij


def _pool_map(fn, jobs, workers, what):
    if workers > 1:
        import multiprocessing as mp
        with mp.get_context("fork").Pool(workers) as pool:
            for k, _ in enumerate(pool.imap_unordered(fn, jobs)):
                if k % 8 == 7:
                    log(f"  {k + 1}/{len(jobs)} {what}")
    else:
        for ij in jobs:
            fn(ij)


def write_terrain(m, h, origin, tex, workers, tex_far=2048, bake=True):
    """bake=False: keep the baked masters (materials/bake) and only redo meshes + texture sizes."""
    root = os.path.join(MODELS, "bymarka_terrain")
    _FINISH.update(origin=origin, tex=tex, tex_far=tex_far)
    for d in ("meshes", "materials/textures", "materials/bake"):
        os.makedirs(os.path.join(root, d), exist_ok=True)
    ox, oy, oz = origin
    n = h.shape[0] - 1
    step = n // TILES
    size = 2 * HALF / TILES
    visuals, collisions = [], []
    cstep = int(COLLISION_RES / MESH_RES)
    for i in range(TILES):
        for j in range(TILES):
            z = h[i * step:(i + 1) * step + 1, j * step:(j + 1) * step + 1] - oz
            x0 = CORE[0] + j * size - ox
            yt = CORE[3] - i * size - oy
            v, t, nr = grid_mesh(z, x0, yt, MESH_RES)
            uv = np.stack([(v[:, 0] - x0) / size, (yt - v[:, 1]) / size], 1)
            write_gltf(os.path.join(root, "meshes", f"tile_{i}_{j}.gltf"), v, t, nr, uv)
            zc = z[::cstep, ::cstep]
            cv, ct, cn = grid_mesh(zc, x0, yt, COLLISION_RES)
            write_obj(os.path.join(root, "meshes", f"tile_{i}_{j}_collision.obj"), cv, ct, cn)
            visuals.append(f"""      <visual name="tile_{i}_{j}">
        <geometry><mesh><uri>model://bymarka_terrain/meshes/tile_{i}_{j}.gltf</uri></mesh></geometry>
        <material>
          <diffuse>1 1 1 1</diffuse><specular>0.04 0.04 0.04 1</specular>
          <pbr><metal>
            <albedo_map>model://bymarka_terrain/materials/textures/tile_{i}_{j}_color.dds</albedo_map>
            <normal_map>model://bymarka_terrain/materials/textures/tile_{i}_{j}_normal.jpg</normal_map>
            <roughness>0.92</roughness><metalness>0</metalness>
          </metal></pbr>
        </material>
      </visual>""")
            collisions.append(f"""      <collision name="tile_{i}_{j}">
        <geometry><mesh><uri>model://bymarka_terrain/meshes/tile_{i}_{j}_collision.obj</uri></mesh></geometry>
      </collision>""")
    sdf = f"""<?xml version="1.0"?>
<!-- Generated by sim/tools/bymarka/make_world.py, do not edit. -->
<sdf version="1.9">
  <model name="bymarka_terrain">
    <static>true</static>
    <link name="link">
{chr(10).join(collisions)}
{chr(10).join(visuals)}
    </link>
  </model>
</sdf>
"""
    with open(os.path.join(root, "model.sdf"), "w") as f:
        f.write(sdf)
    write_config(root, "bymarka_terrain", "Real terrain of Kobberdammen/Gråkallen, Trondheim (Kartverket DTM 1 m)")

    jobs = [(i, j) for i in range(TILES) for j in range(TILES)]
    tdir, bdir = os.path.join(root, "materials", "textures"), os.path.join(root, "materials", "bake")
    for f in os.listdir(tdir):  # older runs kept the baked 4K .jpg in textures/: they are the masters
        if f.endswith(".jpg") and not os.path.exists(os.path.join(bdir, f)):
            shutil.move(os.path.join(tdir, f), os.path.join(bdir, f))
        elif f.endswith("_color.jpg"):
            os.remove(os.path.join(tdir, f))
    if not bake:
        log(f"re-encoding {len(jobs)} baked tiles (near {tex} px, far {min(tex, tex_far)} px)")
        _pool_map(finish_tile, jobs, workers, "tiles")
        return
    # Fine relief = 1 m DTM minus the 2 m mesh surface, for the baked normal maps.
    up = ndimage.zoom(h, MESH_RES, order=1)[:m["dtm"].shape[0], :m["dtm"].shape[1]]
    m["relief"] = np.where(m["water"], 0, m["dtm"] - up).astype(np.float32)
    bake_setup(m, tex)
    log(f"baking {len(jobs)} tiles at {tex} px ({_BAKE['texel'] * 100:.1f} cm/px) with {workers} workers")
    _pool_map(bake_tile, jobs, workers, "tiles")


def write_water(m, origin):
    """Each lake as a flat surface at its real level (2 m cells, reaching 3 m under the shore)."""
    root = os.path.join(MODELS, "bymarka_water")
    os.makedirs(os.path.join(root, "meshes"), exist_ok=True)
    sky = os.path.join(SIM, "assets", "sky", "skybox.dds")
    if os.path.exists(sky):
        os.makedirs(os.path.join(root, "materials", "textures"), exist_ok=True)
        shutil.copy(sky, os.path.join(root, "materials", "textures", "sky.dds"))
    ox, oy, oz = origin
    verts, tris = [], []
    nv = 0
    for mask, level in lakes(m["dtm"], m["water"]):
        cells = ndimage.binary_dilation(mask, iterations=3)[::2, ::2]  # 2 m cells
        r, c = np.nonzero(cells)
        x0 = CORE[0] + c * 2.0 - ox
        y0 = CORE[3] - r * 2.0 - oy
        z = level - oz + 0.02
        quad = np.stack([np.stack([x0, y0], 1), np.stack([x0 + 2, y0], 1),
                         np.stack([x0 + 2, y0 - 2], 1), np.stack([x0, y0 - 2], 1)], 1).reshape(-1, 2)
        verts.append(np.column_stack([quad, np.full(len(quad), z)]))
        k = nv + np.arange(len(r))[:, None] * 4
        tris.append(np.concatenate([k + [0, 3, 1], k + [1, 3, 2]]))
        nv += len(quad)
    v, t = np.concatenate(verts), np.concatenate(tris)
    nrm = np.tile([0.0, 0.0, 1.0], (len(v), 1))
    write_gltf(os.path.join(root, "meshes", "lakes.gltf"), v, t, nrm, v[:, :2] / 10)
    with open(os.path.join(root, "model.sdf"), "w") as f:
        f.write("""<?xml version="1.0"?>
<!-- Generated by sim/tools/bymarka/make_world.py, do not edit. -->
<sdf version="1.9">
  <model name="bymarka_water">
    <static>true</static>
    <link name="link">
      <visual name="lakes">
        <cast_shadows>false</cast_shadows>
        <geometry><mesh><uri>model://bymarka_water/meshes/lakes.gltf</uri></mesh></geometry>
        <material>
          <!-- Dark, peaty Norwegian lake water that mirrors the sky (environment map = the sky cubemap). -->
          <diffuse>0.04 0.055 0.06 1</diffuse>
          <specular>0.7 0.7 0.7 1</specular>
          <pbr><metal><roughness>0.12</roughness><metalness>0.7</metalness>
            <environment_map>model://bymarka_water/materials/textures/sky.dds</environment_map></metal></pbr>
        </material>
      </visual>
    </link>
  </model>
</sdf>
""")
    write_config(root, "bymarka_water", "Lakes of Kobberdammen/Gråkallen at their real level")


def write_backdrop(origin, tex=4096):
    """30 x 30 km of real terrain around the core (hills, fjord, Trondheim) with a baked
    colour map and distance haze: Gazebo has no fog, so the haze is painted on."""
    root = os.path.join(MODELS, "bymarka_backdrop")
    os.makedirs(os.path.join(root, "meshes"), exist_ok=True)
    ox, oy, oz = origin
    res = 25.0
    z = data.height("dtm", FAR, res)
    z = np.where(np.isnan(z), -0.5, z)  # sea
    med = ndimage.median_filter(z, 3)
    z = np.where(np.abs(z - med) > 60, med, z)  # single-pixel spikes from resampling
    # Nothing within 15 km of Trondheim is above ~600 m: anything higher is resampling junk.
    z = np.where(z > 900, ndimage.median_filter(np.where(z > 900, 0, z), 9), z)
    n = z.shape[0]
    # Grid points at pixel centres.
    x0 = FAR[0] + res / 2 - ox
    yt = FAR[3] - res / 2 - oy
    v, t, nr = grid_mesh(z - oz, x0, yt, res)
    # Cut a hole for the core (keep a 100 m overlap, pushed 4 m down so the core covers it).
    inside = (np.abs(v[:, 0] + ox - CENTRE[0]) < HALF - 30) & (np.abs(v[:, 1] + oy - CENTRE[1]) < HALF - 30)
    v[inside, 2] -= 4.0
    deep = (np.abs(v[:, 0] + ox - CENTRE[0]) < HALF - 120) & (np.abs(v[:, 1] + oy - CENTRE[1]) < HALF - 120)
    t = t[~deep[t].all(1)]
    uv = np.stack([(v[:, 0] - x0 + res / 2) / (n * res), (yt + res / 2 - v[:, 1]) / (n * res)], 1)
    write_gltf(os.path.join(root, "meshes", "backdrop.gltf"), v, t, nr, uv)

    # Colour map from the topographic map classes.
    topo = data.topo(FAR, 4000).astype(np.float32)
    r, g, b = topo[..., 0], topo[..., 1], topo[..., 2]
    col = np.zeros(topo.shape, np.float32)
    col[:] = (0.30, 0.36, 0.20)  # open land / heath
    forest = (g > r) & (b < 165)
    col[forest] = (0.12, 0.20, 0.10)
    field = (r >= 225) & (g >= 215) & (b < 200)
    col[field] = (0.38, 0.47, 0.22)
    urban = (r > 215) & (g > 215) & (b > 200) & (np.abs(r - b) < 40)
    col[urban] = (0.52, 0.50, 0.48)
    road = (r < 130) & (np.abs(r - g) < 10)
    col[road] = (0.35, 0.35, 0.35)
    water = (b > 235) & (b - r > 30)
    col[water] = (0.08, 0.13, 0.17)
    rng = np.random.default_rng(5)
    col *= (1 + 0.12 * ndimage.gaussian_filter(rng.standard_normal(col.shape[:2]), 3)[..., None] * 3)
    # Bare rock on high, steep ground.
    zz = np.asarray(Image.fromarray(z).resize((4000, 4000), Image.BILINEAR))
    gy, gx = np.gradient(zz, 7.5)
    steep = smoothstep(25, 40, np.degrees(np.arctan(np.hypot(gx, gy)))) * ~water
    col = col * (1 - steep[..., None] * 0.6) + steep[..., None] * 0.6 * np.array((0.45, 0.45, 0.42))
    # Distance haze.
    ys, xs = np.mgrid[0:4000, 0:4000]
    d = np.hypot((xs + 0.5) * 7.5 - FAR_HALF, (ys + 0.5) * 7.5 - FAR_HALF)
    haze = (1 - np.exp(-np.clip(d - 800, 0, None) / 9000.0))[..., None] * 0.75
    col = col * (1 - haze) + haze * np.array((0.62, 0.70, 0.78))
    img = Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).resize((tex, tex), Image.LANCZOS)
    os.makedirs(os.path.join(root, "materials", "textures"), exist_ok=True)
    tdir = os.path.join(root, "materials", "textures")
    save_bc1(os.path.join(tdir, "backdrop.dds"), np.asarray(img))
    if os.path.exists(os.path.join(tdir, "backdrop.jpg")):  # older runs
        os.remove(os.path.join(tdir, "backdrop.jpg"))
    with open(os.path.join(root, "model.sdf"), "w") as f:
        f.write("""<?xml version="1.0"?>
<!-- Generated by sim/tools/bymarka/make_world.py, do not edit. -->
<sdf version="1.9">
  <model name="bymarka_backdrop">
    <static>true</static>
    <link name="link">
      <visual name="terrain">
        <cast_shadows>false</cast_shadows>
        <geometry><mesh><uri>model://bymarka_backdrop/meshes/backdrop.gltf</uri></mesh></geometry>
        <material>
          <diffuse>1 1 1 1</diffuse><specular>0.02 0.02 0.02 1</specular>
          <pbr><metal>
            <albedo_map>model://bymarka_backdrop/materials/textures/backdrop.dds</albedo_map>
            <roughness>0.95</roughness><metalness>0</metalness>
          </metal></pbr>
        </material>
      </visual>
    </link>
  </model>
</sdf>
""")
    write_config(root, "bymarka_backdrop", "30 x 30 km of real terrain around Bymarka (Kartverket DTM)")


# Sun matching the photographic sky (sim/assets/sky/sun_light.sdf.xml, from the HDRI).
SUN = {"direction": "0.5546 0.3755 -0.7425", "diffuse": "0.973 1.000 0.927 1"}


def write_world(origin, extra_includes=(), extra_sdf="", name="bymarka"):
    ox, oy, oz = origin
    lat, lon = data.utm_to_latlon(ox, oy)
    inc = "\n".join(f"    <include><uri>model://{name}</uri></include>"
                    for name in ("bymarka_terrain", "bymarka_water", "bymarka_backdrop", *extra_includes))
    world = f"""<?xml version="1.0" encoding="UTF-8"?>
<!--
  CogiDrone Bymarka world: the real terrain, lakes, forest and paths around Kobberdammen /
  Gråkallen, Trondheim. GENERATED by sim/tools/bymarka/make_world.py, re-run it instead of
  editing. Data: Kartverket (CC BY 4.0), NIBIO SR16 (NLOD), OpenStreetMap (ODbL), see
  sim/ATTRIBUTION.md. Origin = launch clearing at UTM32 {ox:.0f} {oy:.0f}, {oz:.1f} m above sea level.
  The world name must match the file name (PX4 uses it for Gazebo service paths).
-->
<sdf version="1.9">
  <world name="{name}">
    <physics type="ode">
      <max_step_size>0.004</max_step_size>
      <real_time_factor>1.0</real_time_factor>
      <real_time_update_rate>250</real_time_update_rate>
    </physics>
    <gravity>0 0 -9.8</gravity>
    <magnetic_field>6e-06 2.3e-05 -4.2e-05</magnetic_field>
    <atmosphere type="adiabatic"/>
    <scene>
      <grid>false</grid>
      <ambient>0.32 0.34 0.38 1</ambient>
      <background>0.62 0.72 0.85 1</background>
      <shadows>true</shadows>
      <sky></sky>
    </scene>
    <light name="sun" type="directional">
      <pose>0 0 500 0 0 0</pose>
      <cast_shadows>true</cast_shadows>
      <intensity>1.0</intensity>
      <direction>{SUN["direction"]}</direction>
      <diffuse>{SUN["diffuse"]}</diffuse>
      <specular>0.35 0.35 0.33 1</specular>
    </light>
{inc}
{extra_sdf}
    <spherical_coordinates>
      <surface_model>EARTH_WGS84</surface_model>
      <world_frame_orientation>ENU</world_frame_orientation>
      <latitude_deg>{lat:.7f}</latitude_deg>
      <longitude_deg>{lon:.7f}</longitude_deg>
      <elevation>{oz:.2f}</elevation>
    </spherical_coordinates>
  </world>
</sdf>
"""
    with open(os.path.join(WORLDS, f"{name}.sdf"), "w") as f:
        f.write(world)


# ---------------------------------------------------------------------------------------------
# Trees from the laser data

SPECIES = ("spruce", "pine", "birch", "alder", "juniper")
NEAR_RADIUS = 350.0  # m around the launch spot: detailed trees + ground clutter (where the drone flies)


def assign_species(m, r, c, hgt, u, edge_birch=0.18):
    """Species index (SPECIES) for trees at 1 m pixels (r, c) with heights hgt; u = uniform randoms.
    NIBIO SR16 (16 m cells) refined by terrain."""
    # Species colours on the SR16 WMS: spruce (82,176,56), pine (205,170,101), broadleaf (255,220,130)
    sp = m["species_rgb"][np.clip(r // 4, 0, 499), np.clip(c // 4, 0, 499)]
    def near(col):
        return np.abs(sp - np.array(col)).sum(1) < 40
    species = np.zeros(len(r), np.int8)  # spruce by default (also outside SR16 forest cells)
    species[near((205, 170, 101))] = 1
    species[near((255, 220, 130))] = 2
    stream_d = ndimage.distance_transform_edt(m["osm"]["stream"] < 0.5)[r, c]
    water_d = ndimage.distance_transform_edt(~m["water"])[r, c]
    rough, slope = m["rough"][r, c], m["slope"][r, c]
    # Broadleaf next to streams and lakes is mostly grey alder, otherwise birch.
    species[(species == 2) & ((stream_d < 12) | (water_d < 8)) & (u < 0.6)] = 3
    # Mixed stands: some birch everywhere, more at edges/clearings and near water.
    edge = m["canopy"][r, c] < 0.6
    species[(species == 0) & edge & (u < edge_birch)] = 2
    species[(species == 0) & (water_d < 15) & (u < 0.25)] = 2
    # Pine on rocky / steep dry ground even in spruce cells.
    species[(species == 0) & ((rough > 0.45) | (slope > 25)) & (u < 0.45)] = 1
    # Low shrubs in the open on rock: juniper.
    species[(hgt < 4) & (m["canopy"][r, c] < 0.4) & (u < 0.5) & (species != 3)] = 4
    return species


def tree_instances(m, origin, seed=3):
    """One tree per local maximum of the canopy height model (DOM - DTM), with its real height.
    Species from NIBIO SR16 (16 m cells) refined by terrain."""
    rng = np.random.default_rng(seed)
    chm = ndimage.gaussian_filter(m["chm"], 0.8)
    # Window grows with tree height (bigger crowns): 3 m for small trees, up to 7 m.
    peaks = np.zeros(chm.shape, bool)
    for lo, hi, win in ((2.5, 8, 3), (8, 16, 5), (16, 99, 7)):
        band = (chm >= lo) & (chm < hi)
        peaks |= band & (chm == ndimage.maximum_filter(chm, size=win))
    r, c = np.nonzero(peaks)
    hgt = chm[r, c]
    species = assign_species(m, r, c, hgt, rng.random(len(r)))
    ox, oy, oz = origin
    x = CORE[0] + c + 0.5 + rng.uniform(-0.4, 0.4, len(r)) - ox
    y = CORE[3] - r - 0.5 + rng.uniform(-0.4, 0.4, len(r)) - oy
    z = m["dtm"][r, c] - oz
    near_tier = np.hypot(x, y) < NEAR_RADIUS
    return {"x": x.astype(np.float32), "y": y.astype(np.float32), "z": z.astype(np.float32),
            "height": hgt.astype(np.float32), "species": species, "yaw": rng.uniform(0, 2 * np.pi, len(r)).astype(np.float32),
            "near": near_tier, "species_names": np.array(SPECIES)}


# ---------------------------------------------------------------------------------------------
# Ground clutter and people (near the launch spot, where the drone searches)


def prop_instances(m, origin, trees, seed=4, density=1.0, half=None):
    """Rocks, boulders, outcrop slabs, ferns, bilberry, logs and stumps placed from the data.
    In a circle of NEAR_RADIUS around the origin, or a square of +-half m if half is given."""
    rng = np.random.default_rng(seed)
    ox, oy, oz = origin
    out = []
    # 1 m pixel coordinates of the near zone.
    cr, cc = CORE[3] - oy, ox - CORE[0]
    rad = half or NEAR_RADIUS
    r0, r1 = int(max(cr - rad, 0)), int(min(cr + rad, 2000))
    c0, c1 = int(max(cc - rad, 0)), int(min(cc + rad, 2000))
    canopy = m["canopy"][r0:r1, c0:c1]
    rough = m["rough"][r0:r1, c0:c1]
    slope = m["slope"][r0:r1, c0:c1]
    water = m["water"][r0:r1, c0:c1]
    paths = np.maximum(m["osm"]["path"], np.maximum(m["osm"]["track"], m["osm"]["road"]))[r0:r1, c0:c1]
    stream_d = ndimage.distance_transform_edt(m["osm"]["stream"][r0:r1, c0:c1] < 0.5)
    path_d = ndimage.distance_transform_edt(paths < 0.5)
    rr, ccs = np.mgrid[r0:r1, c0:c1]
    area = (np.maximum(np.abs(rr - cr), np.abs(ccs - cc)) < rad) if half else (np.hypot(rr - cr, ccs - cc) < rad)
    inside = area & ~water & (path_d > 1.5)

    def scatter(kind, prob, n_variants, scale_lo, scale_hi, sink=0.0, align=False):
        pick = inside & (rng.random(prob.shape) < prob * density)
        r, c = np.nonzero(pick)
        for a, b in zip(r, c):
            R, C = a + r0, b + c0
            x = CORE[0] + C + rng.uniform(0, 1) - ox
            y = CORE[3] - R - rng.uniform(0, 1) - oy
            z = float(m["dtm"][R, C]) - oz
            inst = {"kind": kind, "variant": int(rng.integers(n_variants)), "x": float(x), "y": float(y),
                    "z": z, "scale": float(rng.uniform(scale_lo, scale_hi)), "yaw": float(rng.uniform(0, 2 * np.pi)),
                    "sink": sink}
            if align:  # tilt with the slope
                gy, gx = np.gradient(m["dtm"][max(R - 2, 0):R + 3, max(C - 2, 0):C + 3])
                inst["pitch"] = float(np.arctan(np.mean(gx)) * 0.8)
                inst["roll"] = float(-np.arctan(-np.mean(gy)) * 0.8)
            out.append(inst)

    rocky = smoothstep(0.3, 0.9, rough)
    scatter("outcrop", 0.0015 * rocky * smoothstep(15, 35, slope), 3, 0.7, 1.4, sink=0.6, align=True)
    scatter("boulder", 0.004 * rocky + 0.0004 * canopy, 3, 0.6, 1.6, sink=0.3, align=True)
    scatter("rock", 0.02 * rocky + 0.003, 6, 0.4, 1.3, sink=0.1, align=True)
    scatter("bilberry", 0.16 * smoothstep(0.3, 0.8, canopy) * (1 - rocky), 2, 0.7, 1.4, align=True)
    scatter("fern", 0.08 * smoothstep(12, 2, stream_d) + 0.03 * smoothstep(6, 2, path_d) * canopy
            + 0.008 * canopy, 2, 0.6, 1.3)
    # Open moor and heath: heather and crowberry mats, low junipers (not on bog or bare rock).
    heath = smoothstep(0.45, 0.1, canopy) * (1 - smoothstep(0.4, 0.8, rocky)) * (m["osm"]["wetland"][r0:r1, c0:c1] < 0.5)
    scatter("heather", 0.05 * heath + 0.01 * smoothstep(0.6, 0.3, canopy), 3, 0.6, 1.5, align=True)
    scatter("juniper", 0.004 * heath + 0.002 * rocky * smoothstep(0.5, 0.2, canopy), 2, 0.6, 1.4)
    scatter("log", 0.0025 * canopy, 2, 0.7, 1.3, sink=0.1, align=True)
    scatter("stump", 0.002 * canopy + 0.0008, 2, 0.8, 1.2)
    return out


def local_people(state):
    """Recoloured people from sim/tools/people/make_people.py (sim/models/person_*), if built.
    The walking pose looks wrong lying down, so lying people use the standing/female models."""
    try:
        index = json.load(open(os.path.join(MODELS, "people.json")))
    except OSError:
        return []
    ok = ("standing", "female") if state == "lying" else ("walking", "standing", "female")
    return sorted(n for n, v in index.items() if v["base"] in ok and os.path.isdir(os.path.join(MODELS, n)))


def place_people(m, origin, trees, n=24, seed=5, half=None):
    """Missing people near the launch spot: hikers on paths, people sitting by the lake, lying in
    the forest or on rock. Returns (static, actors) in the format of sim/tools/make_sar_world.py."""
    rng = np.random.default_rng(seed)
    ox, oy, oz = origin
    paths = np.maximum(m["osm"]["path"], m["osm"]["track"])
    shore = ndimage.binary_dilation(m["water"], iterations=4) & ~m["water"]
    zones = {
        "forest": (m["canopy"] > 0.6) & ~m["water"],
        "path": paths > 0.5,
        "shore": shore,
        "rock": (m["rough"] > 0.4) & (m["canopy"] < 0.4),
        "open": (m["canopy"] < 0.3) & ~m["water"] & (m["osm"]["wetland"] < 0.5),
    }
    states = {
        "forest": ["lying", "lying", "lying", "sitting", "standing"],
        "path": ["walking", "walking", "lying", "standing", "waving"],
        "shore": ["sitting", "lying", "standing"],
        "rock": ["lying", "sitting", "waving"],
        "open": ["lying", "standing", "waving", "walking"],
    }
    order = ["forest"] * (n * 4 // 10) + ["path"] * (n * 2 // 10) + ["shore"] * (n // 10) + ["rock"] * (n // 10)
    order += ["open"] * (n - len(order))
    yy, xx = np.mgrid[0:2000, 0:2000]
    dist = np.hypot(xx - (ox - CORE[0]), yy - (CORE[3] - oy))
    outer = NEAR_RADIUS - 30
    if half:  # square area: Chebyshev distance
        dist = np.maximum(np.abs(xx - (ox - CORE[0])), np.abs(yy - (CORE[3] - oy)))
        outer = half - 25
    people, actors, taken = [], [], []
    for i, zone in enumerate(order):
        cand = np.nonzero(zones[zone] & (dist > 25) & (dist < outer))
        for _ in range(200):
            k = rng.integers(len(cand[0]))
            R, C = int(cand[0][k]), int(cand[1][k])
            x, y = CORE[0] + C + 0.5 - ox, CORE[3] - R - 0.5 - oy
            if all(math.hypot(x - a, y - b) > 30 for a, b in taken):
                break
        taken.append((x, y))
        z = float(m["dtm"][R, C]) - oz
        state = states[zone][rng.integers(len(states[zone]))]
        yaw = float(rng.uniform(-math.pi, math.pi))
        p = {"id": f"person_{i:02d}", "zone": zone, "state": state, "x": round(x, 2), "y": round(y, 2),
             "z": round(z, 2)}
        if state in ("walking", "waving") or (state == "sitting" and rng.random() < 0.3):
            p["model"] = "actor"
            if state == "walking":
                # Walk along the path direction if on a path, else straight.
                length = float(rng.uniform(8, 18))
                x1, y1 = x + length * math.cos(yaw), y + length * math.sin(yaw)
                c1 = int(np.clip(x1 + ox - CORE[0], 0, 1999)); r1 = int(np.clip(CORE[3] - (y1 + oy), 0, 1999))
                p["path"] = [[round(x, 2), round(y, 2)], [round(x1, 2), round(y1, 2)]]
                p["z1"] = round(float(m["dtm"][r1, c1]) - oz, 2)
            actors.append((p, yaw))
        else:
            variants = local_people(state)
            if variants and not (state == "sitting" or rng.random() < 0.15):
                p["model"] = variants[rng.integers(len(variants))]
            else:
                p["model"] = "rescue_randy_sitting" if state == "sitting" else \
                    ["rescue_randy", "standing_person", "walking_person", "male_visitor", "casual_female"][rng.integers(5)]
            if state == "lying":
                p["face"] = "up" if rng.random() < 0.6 else "down"
                p["injured"] = bool(rng.random() < 0.6)
            people.append((p, yaw))
        p["yaw"] = round(yaw, 2)
    return people, actors


def people_sdf(people, actors):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from make_sar_world import ACTOR, PEOPLE, person_sdf  # same people models as the SAR world

    for name in {p["model"] for p, _ in people if p["model"].startswith("person_")}:
        PEOPLE.setdefault(name, (f"model://{name}", "standing"))
    parts = [person_sdf(p, yaw) for p, yaw in people]
    for p, yaw in actors:
        anim = {"walking": "walk", "waving": "talk_a", "sitting": "sitting"}[p["state"]]
        if p["state"] == "walking":
            (x0, y0), (x1, y1) = p["path"]
            z0, z1 = p["z"] + 1.0, p["z1"] + 1.0  # actor origin is at the hips
            t = math.hypot(x1 - x0, y1 - y0) / 1.0
            back = yaw + math.pi
            wps = [(0, x0, y0, z0, yaw), (t, x1, y1, z1, yaw), (t + 1.5, x1, y1, z1, back),
                   (2 * t + 1.5, x0, y0, z0, back), (2 * t + 3, x0, y0, z0, yaw)]
            wp = "\n".join(f"          <waypoint><time>{a:.2f}</time><pose>{b} {c} {d:.2f} 0 0 {e:.3f}</pose></waypoint>"
                           for a, b, c, d, e in wps)
            script = f"""      <script><loop>true</loop><auto_start>true</auto_start>
        <trajectory id="0" type="walk">
{wp}
        </trajectory>
      </script>"""
            pose = ""
        else:
            script = "      <script><loop>true</loop><auto_start>true</auto_start></script>"
            pose = f"\n      <pose>{p['x']} {p['y']} {p['z'] + 1.0:.2f} 0 0 {yaw:.3f}</pose>"
        parts.append(f"""    <actor name="{p['id']}">{pose}
      <skin><filename>{ACTOR}{anim}.dae</filename><scale>1.0</scale></skin>
      <animation name="{anim if p['state'] != 'walking' else 'walk'}"><filename>{ACTOR}{anim}.dae</filename><scale>1.0</scale><interpolate_x>true</interpolate_x></animation>
{script}
    </actor>""")
    return "\n".join(parts)


def write_people_yaml(people, actors, seed, name="bymarka"):
    rows = sorted([p for p, _ in people] + [p for p, _ in actors], key=lambda p: p["id"])
    lines = [f"# Where every person in {name}.sdf is. GENERATED by sim/tools/bymarka/make_world.py.",
             f"# seed: {seed}. Gazebo world frame (ENU, m), origin = launch spot.", "people:"]
    for p in rows:
        lines.append("  - {id: %s, %s}" % (p["id"], ", ".join(f"{k}: {v}" for k, v in p.items() if k != "id")))
    with open(os.path.join(WORLDS, f"{name}_people.yaml"), "w") as f:
        f.write("\n".join(lines).replace("True", "true").replace("False", "false") + "\n")


def write_config(root, name, desc):
    with open(os.path.join(root, "model.config"), "w") as f:
        f.write(f"""<?xml version="1.0"?>
<model>
  <name>{name}</name>
  <version>1.0</version>
  <sdf version="1.9">model.sdf</sdf>
  <description>{desc}</description>
</model>
""")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="only write a mask preview PNG here")
    ap.add_argument("--tex", type=int, default=4096, help="baked texture size per 250 m tile")
    ap.add_argument("--tex-far", type=int, default=2048,
                    help="texture size of tiles outside the --near-radius square (RAM: GPU memory is system RAM)")
    ap.add_argument("--retex", action="store_true",
                    help="only re-encode the baked tiles (texture sizes), no rebake (needs one full run first)")
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--only-tile", help="i,j: bake just this tile (for testing)")
    ap.add_argument("--skip-terrain", action="store_true", help="keep the baked tiles from last run")
    ap.add_argument("--people", type=int, default=24)
    ap.add_argument("--near-radius", type=float, default=NEAR_RADIUS,
                    help="detailed trees and props within this radius, cheap impostors outside (GPU cost)")
    ap.add_argument("--forest", action="store_true", help="(re)build the trees (bymarka_forest, ~1 GB)")
    ap.add_argument("--props", action="store_true", help="(re)build rocks, ferns, bilberry, logs (bymarka_props)")
    ap.add_argument("--props-density", type=float, default=1.0)
    args = ap.parse_args()
    NEAR_RADIUS = args.near_radius
    masks = build_masks()
    if args.preview:
        preview_masks(masks, args.preview)
        log(f"wrote {args.preview}")
        sys.exit(0)
    origin = choose_origin(masks)
    log(f"origin {origin[0]:.0f} {origin[1]:.0f} at {origin[2]:.1f} m")
    h = grid_heights(masks["dtm"], masks["water"])
    if args.only_tile:
        i, j = map(int, args.only_tile.split(","))
        up = ndimage.zoom(h, MESH_RES, order=1)[:masks["dtm"].shape[0], :masks["dtm"].shape[1]]
        masks["relief"] = np.where(masks["water"], 0, masks["dtm"] - up).astype(np.float32)
        for d in ("textures", "bake"):
            os.makedirs(os.path.join(MODELS, "bymarka_terrain", "materials", d), exist_ok=True)
        _FINISH.update(origin=origin, tex=args.tex, tex_far=args.tex_far)
        bake_setup(masks, args.tex)
        t = time.time()
        bake_tile((i, j))
        log(f"tile {i},{j} baked in {time.time() - t:.0f} s")
        sys.exit(0)
    if args.retex:
        write_terrain(masks, h, origin, args.tex, args.workers, args.tex_far, bake=False)
        write_backdrop(origin)
        log("terrain textures re-encoded")
        sys.exit(0)
    if not args.skip_terrain:
        write_terrain(masks, h, origin, args.tex, args.workers, args.tex_far)
        log("terrain done")
    write_water(masks, origin)
    log("water done")
    write_backdrop(origin)
    log("backdrop done")
    trees = tree_instances(masks, origin)
    np.savez_compressed(os.path.join(WORLDS, "bymarka_trees.npz"), **trees)
    counts = {SPECIES[k]: int((trees["species"] == k).sum()) for k in range(len(SPECIES))}
    log(f"{len(trees['x'])} trees ({int(trees['near'].sum())} near the launch spot): {counts}")
    if args.forest:
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "trees"))
        from forest_builder import build_forest

        inst = [{"species": SPECIES[int(trees["species"][i])], "x": float(trees["x"][i]), "y": float(trees["y"][i]),
                 "z": float(trees["z"][i]), "height": float(trees["height"][i]),
                 "tier": "near" if trees["near"][i] else "far"} for i in range(len(trees["x"]))]
        shutil.rmtree(os.path.join(MODELS, "bymarka_forest"), ignore_errors=True)
        build_forest(inst, MODELS, "bymarka_forest", collision="mesh")
        log("forest done")
        sys.modules.pop("glb", None)  # props/ has its own glb.py
    if args.props:
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "props"))
        from props_builder import build_props

        inst = prop_instances(masks, origin, trees, density=args.props_density)
        kinds = {}
        for p in inst:
            kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
        log(f"{len(inst)} props: {kinds}")
        out = os.path.join(MODELS, "bymarka_props")
        shutil.rmtree(out, ignore_errors=True)
        build_props(inst, out, "bymarka_props")
        log("props done")
    people, actors = place_people(masks, origin, trees, n=args.people)
    write_people_yaml(people, actors, 5)
    log(f"{len(people) + len(actors)} people ({len(actors)} animated)")
    extra = [name for name in ("bymarka_forest", "bymarka_props") if os.path.exists(os.path.join(MODELS, name, "model.sdf"))]
    write_world(origin, extra, people_sdf(people, actors))
    log(f"world written (with {', '.join(extra) or 'no trees/props yet'})")
