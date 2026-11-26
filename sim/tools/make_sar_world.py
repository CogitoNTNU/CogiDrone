#!/usr/bin/env python3
"""Generate the search-and-rescue test world: sim/worlds/sar.sdf.

    python3 sim/tools/make_sar_world.py                 # default world (seed 1)
    python3 sim/tools/make_sar_world.py --seed 7 --people 30
    python3 sim/tools/make_sar_world.py --trees 600     # lighter forest for slow PCs

A 600 x 600 m area loosely like the outskirts of Trondheim: a fjord to the north, a hilly
spruce forest to the west, fields to the south and a small town (partly collapsed) to the east.
The drone spawns on a flat field at the origin.

People are placed at random (from --seed) in all three zones: standing, walking, sitting and
lying down, some under trees or next to collapsed buildings. Their true positions are written
to sim/worlds/sar_people.yaml so a search can be scored afterwards.

Writes (all generated, re-run this script instead of editing them by hand):
    sim/models/sar_terrain/      ground mesh + procedural textures
    sim/models/sar_forest/       all trees as one model (much faster than one model per tree)
    sim/models/sar_house_*/      Fuel houses with materials that work in Gazebo Harmonic
    sim/worlds/sar.sdf           the world
    sim/worlds/sar_people.yaml   ground truth for the people

Only needs numpy and Pillow. Buildings, trees and people are Gazebo Fuel models (CC-BY),
downloaded by Gazebo on first start and cached in ~/.gz/fuel.
"""

import argparse
import math
import os

import numpy as np
from PIL import Image

SIM = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = os.path.join(SIM, "models")
WORLDS = os.path.join(SIM, "worlds")
FUEL = "https://fuel.gazebosim.org/1.0"

HALF = 300.0  # world spans -HALF..HALF in x (east) and y (north)
GRID = 3.0  # terrain mesh resolution in m
COLLISION_GRID = 6.0
WATER_Z = -1.5

# Zones (x0, x1, y0, y1) in m.
TOWN = (70.0, 270.0, -40.0, 200.0)
TOWN_Z = 1.0
FIELDS = (-50.0, 270.0, -280.0, -80.0)
FOREST_X = -70.0  # forest west of this
MAIN_ROAD_Y = -25.0
TOWN_STREETS_X = (100.0, 170.0, 240.0)
TOWN_STREETS_Y = (-10.0, 60.0, 130.0, 190.0)
ROAD_HALF_WIDTH = 3.5


# ---------------------------------------------------------------------------
# Terrain


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def periodic_noise(n, rng, beta, lowcut=0):
    """Tileable fractal noise in [-1, 1]: white noise filtered to a 1/f^beta spectrum."""
    f = np.fft.fftfreq(n)
    fx, fy = np.meshgrid(f, f)
    r = np.hypot(fx, fy)
    r[0, 0] = 1.0
    spec = np.fft.fft2(rng.standard_normal((n, n))) / r**beta
    if lowcut:
        spec[r < lowcut / n] = 0
    out = np.real(np.fft.ifft2(spec))
    out -= out.mean()
    return out / np.abs(out).max()


def in_box(x, y, box, margin=0.0):
    x0, x1, y0, y1 = box
    return (x > x0 - margin) & (x < x1 + margin) & (y > y0 - margin) & (y < y1 + margin)


def box_weight(x, y, box, blend):
    """1 inside box, falling to 0 over `blend` m outside it."""
    x0, x1, y0, y1 = box
    dx = np.maximum(np.maximum(x0 - x, x - x1), 0)
    dy = np.maximum(np.maximum(y0 - y, y - y1), 0)
    return 1 - smoothstep(0, blend, np.hypot(dx, dy))


def road_mask(x, y):
    """0 = no road, 1 = asphalt, 2 = gravel."""
    m = np.zeros(np.shape(x), int)
    main = np.abs(y - MAIN_ROAD_Y) < ROAD_HALF_WIDTH
    m[main & (x >= FOREST_X)] = 1
    # The main road turns into a gravel track that winds into the forest.
    track_y = MAIN_ROAD_Y + 25 * np.sin((x - FOREST_X) / 45)
    m[(np.abs(y - track_y) < 2.5) & (x < FOREST_X)] = 2
    town = in_box(x, y, TOWN, 4)
    for sx in TOWN_STREETS_X:
        m[town & (np.abs(x - sx) < ROAD_HALF_WIDTH)] = 1
    for sy in TOWN_STREETS_Y:
        m[town & (np.abs(y - sy) < ROAD_HALF_WIDTH)] = 1
    # Farm track between the fields.
    m[in_box(x, y, FIELDS) & (np.abs(x - 110) < 2.0)] = 2
    return m


def road_lines():
    """Road centre lines as (points, half width, material), drawn as strips on the ground."""
    roads = [([(FOREST_X, MAIN_ROAD_Y), (TOWN[1] + 4, MAIN_ROAD_Y)], ROAD_HALF_WIDTH, "asphalt")]
    xs = np.arange(-HALF + 2, FOREST_X + 1, 3.0)
    roads.append((list(zip(xs, MAIN_ROAD_Y + 25 * np.sin((xs - FOREST_X) / 45))), 2.5, "gravel"))
    for sx in TOWN_STREETS_X:
        roads.append(([(sx, TOWN[2] - 4), (sx, TOWN[3] + 4)], ROAD_HALF_WIDTH, "asphalt"))
    for sy in TOWN_STREETS_Y:
        roads.append(([(TOWN[0] - 4, sy), (TOWN[1] + 4, sy)], ROAD_HALF_WIDTH, "asphalt"))
    roads.append(([(110.0, FIELDS[2]), (110.0, FIELDS[3])], 2.0, "gravel"))
    return roads


def road_strips(xs, h):
    """One triangle strip per road, 4-6 cm above the ground, split into a list per material."""
    out = {}
    for k, (pts, hw, mat) in enumerate(road_lines()):
        # Resample every 1.5 m so the strip follows the ground.
        dense = []
        for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 1.5))
            dense += [(x0 + (x1 - x0) * t / n, y0 + (y1 - y0) * t / n) for t in range(n)]
        dense.append(pts[-1])
        p = np.array(dense)
        d = np.gradient(p, axis=0)
        d /= np.linalg.norm(d, axis=1, keepdims=True)
        side = np.stack([-d[:, 1], d[:, 0]], 1) * hw
        lift = 0.04 + 0.01 * (k % 3)  # crossing roads at slightly different heights: no flicker
        ring = []
        for q in (p + side, p - side):
            ring.append(np.array([(a, b, height_at(xs, h, a, b) + lift) for a, b in q]))
        verts = np.concatenate(ring)
        m = len(p)
        i = np.arange(m - 1)
        tris = np.concatenate([np.stack([i, i + m, i + 1], 1), np.stack([i + 1, i + m, i + m + 1], 1)])
        vs, ts = out.setdefault(mat, ([], []))
        ts.append(tris + sum(len(v) for v in vs))
        vs.append(verts)
    return {mat: (np.concatenate(vs), np.concatenate(ts)) for mat, (vs, ts) in out.items()}


def make_heights(rng):
    n = int(2 * HALF / GRID) + 1
    xs = np.linspace(-HALF, HALF, n)
    x, y = np.meshgrid(xs, xs)  # x varies along columns, y along rows
    big = periodic_noise(n - 1, rng, 2.4)
    big = np.pad(big, ((0, 1), (0, 1)), mode="wrap")
    small = np.pad(periodic_noise(n - 1, rng, 1.6), ((0, 1), (0, 1)), mode="wrap")

    forest = smoothstep(FOREST_X + 20, FOREST_X - 80, x)
    h = forest * (10 + 16 * big + 1.5 * small) + (1 - forest) * (1.2 * big + 0.3 * small)
    # Flat town and launch field, gentle fields.
    town = box_weight(x, y, TOWN, 30)
    h = h * (1 - town) + TOWN_Z * town
    launch = 1 - smoothstep(25, 60, np.hypot(x, y))
    h = h * (1 - launch)
    # Roads are flattened a little (no deep cuts).
    roads = road_mask(x, y) > 0
    h[roads] = h[roads] * 0.8 + np.where(town[roads] > 0.5, TOWN_Z, h[roads]) * 0.2
    h = np.maximum(h, -0.3)  # no inland pits below the water line
    # Fjord in the north.
    fjord = smoothstep(215, 255, y + 10 * np.sin(x / 70))
    h = h * (1 - fjord) - 6 * fjord
    return xs, h


def height_at(xs, h, px, py):
    """Height of the mesh surface (same triangulation as write_terrain) at (px, py)."""
    i = np.clip((px - xs[0]) / GRID, 0, len(xs) - 1.001)
    j = np.clip((py - xs[0]) / GRID, 0, len(xs) - 1.001)
    c, r = int(i), int(j)
    u, v = i - c, j - r
    z00, z10 = h[r, c], h[r, c + 1]
    z01, z11 = h[r + 1, c], h[r + 1, c + 1]
    # Triangles: (00, 10, 11) when u >= v, else (00, 11, 01).
    if u >= v:
        return float(z00 + u * (z10 - z00) + v * (z11 - z10))
    return float(z00 + v * (z01 - z00) + u * (z11 - z01))


def triangles(n):
    idx = np.arange(n * n).reshape(n, n)
    a, b = idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel()
    c, d = idx[1:, 1:].ravel(), idx[1:, :-1].ravel()
    return np.concatenate([np.stack([a, b, c], 1), np.stack([a, c, d], 1)])


def normals(verts, tris):
    fn = np.cross(verts[tris[:, 1]] - verts[tris[:, 0]], verts[tris[:, 2]] - verts[tris[:, 0]])
    vn = np.zeros_like(verts)
    for k in range(3):
        np.add.at(vn, tris[:, k], fn)
    return vn / np.linalg.norm(vn, axis=1, keepdims=True)


def write_obj(path, verts, tris, uv_scale=None, vn=None):
    used = np.unique(tris)
    remap = np.full(len(verts), -1)
    remap[used] = np.arange(len(used)) + 1
    v = verts[used]
    lines = ["# generated by sim/tools/make_sar_world.py"]
    lines += [f"v {a:.2f} {b:.2f} {c:.3f}" for a, b, c in v]
    if uv_scale:
        lines += [f"vt {a / uv_scale:.4f} {b / uv_scale:.4f}" for a, b, _ in v]
    if vn is not None:
        lines += [f"vn {a:.3f} {b:.3f} {c:.3f}" for a, b, c in vn[used]]
    t = remap[tris]
    if uv_scale:
        lines += [f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}" for a, b, c in t]
    else:
        lines += [f"f {a}//{a} {b}//{b} {c}//{c}" for a, b, c in t]
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def write_gltf(path, verts, tris, uv_scale, vn):
    """Mesh as glTF with two UV sets: 0 = tiled ground texture, 1 = the whole map (light map)."""
    import json

    used = np.unique(tris)
    remap = np.full(len(verts), -1)
    remap[used] = np.arange(len(used))
    v = verts[used].astype(np.float32)
    uv0 = (v[:, :2] / uv_scale).astype(np.float32)
    uv1 = np.stack([(v[:, 0] + HALF) / (2 * HALF), (HALF - v[:, 1]) / (2 * HALF)], 1).astype(np.float32)
    idx = remap[tris].astype(np.uint32).ravel()
    blobs = [v, vn[used].astype(np.float32), uv0, uv1, idx]
    buf, views, acc = b"", [], []
    for i, b in enumerate(blobs):
        views.append({"buffer": 0, "byteOffset": len(buf), "byteLength": b.nbytes})
        buf += b.tobytes()
        if b.ndim == 1:
            acc.append({"bufferView": i, "componentType": 5125, "count": len(b), "type": "SCALAR"})
        else:
            a = {"bufferView": i, "componentType": 5126, "count": len(b), "type": f"VEC{b.shape[1]}"}
            if i == 0:
                a["min"], a["max"] = v.min(0).tolist(), v.max(0).tolist()
            acc.append(a)
    base = os.path.splitext(os.path.basename(path))[0]
    gltf = {
        "asset": {"version": "2.0", "generator": "sim/tools/make_sar_world.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2,
                                                   "TEXCOORD_1": 3}, "indices": 4, "material": 0}]}],
        "materials": [{"pbrMetallicRoughness": {"metallicFactor": 0}}],
        "buffers": [{"byteLength": len(buf), "uri": base + ".bin"}],
        "bufferViews": views, "accessors": acc,
    }
    with open(path, "w") as f:
        json.dump(gltf, f)
    with open(os.path.splitext(path)[0] + ".bin", "wb") as f:
        f.write(buf)


LIGHT_MAP_SIZE = 2048


def write_light_map(path, xs, h, trees, seed):
    """One texture over the whole map (second UV set), added as light on top of the tiled ground
    textures: patches of lighter/drier vegetation, so the ground doesn't look like one repeated
    tile, and shade under every tree crown."""
    n = LIGHT_MAP_SIZE
    rng = np.random.default_rng(seed + 1000)  # own stream: doesn't move trees or people
    lo = n // 4

    def up(a):
        img = Image.fromarray(((a + 1) * 127.5).astype(np.uint8)).resize((n, n), Image.BICUBIC)
        return np.asarray(img, np.float32) / 127.5 - 1

    def norm(a):
        return np.clip(a / a.std(), -2, 2) / 2

    # Patches 10-50 m across (what the camera sees from drone height) plus a little larger scale.
    patches = norm(up(periodic_noise(lo, rng, 1.0)) + 0.6 * up(periodic_noise(lo, rng, 1.5)))
    dry = smoothstep(0.3, 0.8, norm(up(periodic_noise(lo, rng, 1.3))))
    # Gazebo ADDS the light map as extra light (white = about +30 %, black = nothing), it
    # can't darken. So: bright patches in the open, no extra light under tree crowns.
    m = np.stack([0.5 + 0.5 * patches] * 3, -1)
    # Dry, yellower patches: less blue (and a bit less green) light.
    m[..., 1] *= 1 - 0.2 * dry
    m[..., 2] *= 1 - 0.8 * dry
    px = n / (2 * HALF)
    shade = np.ones((n, n), np.float32)
    for kind, x, y, _, _ in trees:
        r = 1.4 * TREES[kind][2] * px
        c, rr = (x + HALF) * px, (HALF - y) * px
        x0, x1 = int(max(c - 2 * r, 0)), int(min(c + 2 * r + 1, n))
        y0, y1 = int(max(rr - 2 * r, 0)), int(min(rr + 2 * r + 1, n))
        gx, gy = np.meshgrid(np.arange(x0, x1), np.arange(y0, y1))
        d2 = ((gx - c) ** 2 + (gy - rr) ** 2) / r**2
        shade[y0:y1, x0:x1] *= 1 - np.exp(-d2)
    m *= shade[..., None]
    Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8)).save(path, quality=88)


# Ground materials: name -> (ambientCG texture (CC0), texture repeat in m, colour tint)
GROUND = {
    "grass": ("Grass001", 3.0, "1 1 1"),
    "forest_floor": ("Ground068", 3.0, "0.6 0.72 0.5"),  # moss, needles and dirt, made greener
    "field_soil": ("Ground048", 4.0, "1 1 1"),
    "field_crop": ("Ground109", 4.0, "1 1 1"),  # dry stubble after harvest
    "asphalt": ("Asphalt031", 4.0, "1 1 1"),
    "gravel": ("Ground081", 3.0, "1 1 1"),
    "rock": ("Gravel041", 2.5, "1 1 1"),  # shore pebbles
}
TEXTURE_SIZE = 1024
MAPS = {"color": "Color", "normal": "NormalGL", "rough": "Roughness"}


def fetch_textures(tex_dir):
    """Photo textures from ambientCG (CC0). Downloaded once, shrunk and kept in the repo."""
    import io
    import urllib.request
    import zipfile

    for name, (asset, _, _) in GROUND.items():
        if all(os.path.exists(os.path.join(tex_dir, f"{name}_{m}.jpg")) for m in MAPS):
            continue
        url = f"https://ambientcg.com/get?file={asset}_2K-JPG.zip"
        print(f"downloading {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "cogidrone-sim"})
        z = zipfile.ZipFile(io.BytesIO(urllib.request.urlopen(req, timeout=120).read()))
        for m, suffix in MAPS.items():
            src = next(f for f in z.namelist() if f.endswith(f"_{suffix}.jpg"))
            img = Image.open(z.open(src)).convert("L" if m == "rough" else "RGB")
            img = img.resize((TEXTURE_SIZE, TEXTURE_SIZE), Image.LANCZOS)
            img.save(os.path.join(tex_dir, f"{name}_{m}.jpg"), quality=85)


def classify(cx, cy, cz):
    """Ground material for triangle centres."""
    mat = np.full(cx.shape, "grass", dtype=object)
    forest = cx < FOREST_X + 10 * np.sin(cy / 40)
    mat[forest] = "forest_floor"
    fields = in_box(cx, cy, FIELDS)
    # Alternate crop and bare soil in 40 m wide strips.
    strip = ((cx - FIELDS[0]) // 40).astype(int) % 2 == 0
    mat[fields & strip] = "field_crop"
    mat[fields & ~strip] = "field_soil"
    mat[cz < WATER_Z + 0.8] = "rock"
    return mat


def write_terrain(xs, h, trees, seed):
    root = os.path.join(MODELS, "sar_terrain")
    os.makedirs(os.path.join(root, "meshes"), exist_ok=True)
    os.makedirs(os.path.join(root, "materials", "textures"), exist_ok=True)
    n = len(xs)
    x, y = np.meshgrid(xs, xs)
    verts = np.stack([x.ravel(), y.ravel(), h.ravel()], 1)
    tris = triangles(n)
    vn = normals(verts, tris)
    centre = verts[tris].mean(1)
    mats = classify(centre[:, 0], centre[:, 1], centre[:, 2])
    visuals = []
    roads = road_strips(xs, h)
    tex_dir = os.path.join(root, "materials", "textures")
    fetch_textures(tex_dir)
    write_light_map(os.path.join(tex_dir, "light_map.jpg"), xs, h, trees, seed)
    for name, (_, repeat, tint) in GROUND.items():
        if name in roads:
            rv, rt = roads[name]
            up = np.tile([0.0, 0.0, 1.0], (len(rv), 1))
            write_gltf(os.path.join(root, "meshes", name + ".gltf"), rv, rt, repeat, up)
        else:
            sel = tris[mats == name]
            if len(sel) == 0:
                continue
            write_gltf(os.path.join(root, "meshes", name + ".gltf"), verts, sel, repeat, vn)
        # Roads get the light map too, so they are shaded under trees like the ground.
        visuals.append(f"""      <visual name="{name}">
        <geometry><mesh><uri>model://sar_terrain/meshes/{name}.gltf</uri></mesh></geometry>
        <material>
          <diffuse>{tint} 1</diffuse>
          <specular>0.05 0.05 0.05 1</specular>
          <pbr><metal>
            <albedo_map>model://sar_terrain/materials/textures/{name}_color.jpg</albedo_map>
            <normal_map>model://sar_terrain/materials/textures/{name}_normal.jpg</normal_map>
            <roughness_map>model://sar_terrain/materials/textures/{name}_rough.jpg</roughness_map>
            <light_map uv_set="1">model://sar_terrain/materials/textures/light_map.jpg</light_map>
            <metalness>0</metalness>
          </metal></pbr>
        </material>
      </visual>""")

    # Coarser mesh for collision: the drone only touches the ground near the launch field.
    step = int(COLLISION_GRID / GRID)
    cx = xs[::step]
    cverts = np.stack([*np.meshgrid(cx, cx), h[::step, ::step]], -1).reshape(-1, 3)
    ctris = triangles(len(cx))
    # DART ignores a collision mesh without normals.
    write_obj(os.path.join(root, "meshes", "collision.obj"), cverts, ctris, vn=normals(cverts, ctris))

    water = 2 * HALF
    sdf = f"""<?xml version="1.0"?>
<!-- Generated by sim/tools/make_sar_world.py, do not edit. -->
<sdf version="1.9">
  <model name="sar_terrain">
    <static>true</static>
    <link name="link">
      <collision name="collision">
        <geometry><mesh><uri>model://sar_terrain/meshes/collision.obj</uri></mesh></geometry>
      </collision>
{chr(10).join(visuals)}
      <visual name="water">
        <pose>0 {HALF - 30} {WATER_Z} 0 0 0</pose>
        <geometry><box><size>{water} 160 0.01</size></box></geometry>
        <material>
          <diffuse>0.10 0.20 0.27 1</diffuse>
          <specular>0.6 0.6 0.6 1</specular>
          <pbr><metal><roughness>0.15</roughness><metalness>0.1</metalness></metal></pbr>
        </material>
      </visual>
    </link>
  </model>
</sdf>
"""
    with open(os.path.join(root, "model.sdf"), "w") as f:
        f.write(sdf)
    write_config(root, "sar_terrain", "Ground for the SAR test world (generated).")


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


# ---------------------------------------------------------------------------
# Wrapper models around Fuel assets


def fuel(owner, model, version, path):
    return f"{FUEL}/{owner}/models/{model.replace(' ', '%20')}/{version}/files/{path}"


# Spruce sizes: the Fuel pine is ~4.7 m tall, Norwegian spruces are 15-25 m.
# name -> (Fuel model, height scale, width scale)
TREES = {
    "spruce_s": ("pine", 2.6, 1.5),
    "spruce_m": ("pine", 3.6, 1.9),
    "spruce_l": ("pine", 4.6, 2.3),
    "oak": ("oak", 1.6, 1.6),
}

# Houses 1-3 use OGRE 1 material scripts, which render black in Harmonic: use PBR instead.
HOUSES = {
    "sar_house_1": ("House 1", "house_1.dae", "House_1_Diffuse.png"),
    "sar_house_2": ("House 2", "house_2.dae", "House_2_Diffuse.png"),
    "sar_house_3": ("House 3", "house_3.dae", "House_3_Diffuse.png"),
}


def tree_sdf(i, kind, x, y, z, yaw):
    """Visuals + trunk collision of one tree, for the single forest model."""
    model, scale, wide = TREES[kind]
    fuel_name, version, branch = {"pine": ("pine tree", 6, "branch_2_diffuse.png"),
                                  "oak": ("oak tree", 7, "branch_diffuse.png")}[model]
    mesh = fuel("openrobotics", fuel_name, version, f"meshes/{fuel_name.replace(' ', '_')}.dae")
    tex = fuel("openrobotics", fuel_name, version, "materials/textures/")
    height = 4.7 * scale
    trunk_r = (0.12 if model == "pine" else 0.2) * wide
    pose = f"{x:.2f} {y:.2f} {z - 0.1:.2f} 0 0 {yaw:.2f}"

    def visual(sub, albedo, double_sided):
        return (f'      <visual name="t{i}_{sub.lower()}"><pose>{pose}</pose>'
                f"<geometry><mesh><uri>{mesh}</uri><submesh><name>{sub}</name></submesh>"
                f"<scale>{wide} {wide} {scale}</scale></mesh></geometry>"
                f"<material><double_sided>{str(double_sided).lower()}</double_sided><diffuse>1 1 1</diffuse>"
                f"<pbr><metal><albedo_map>{tex}{albedo}</albedo_map></metal></pbr></material></visual>")

    return "\n".join([
        visual("Branch", branch, True),
        visual("Bark", "bark_diffuse.png", False),
        f'      <collision name="t{i}_trunk"><pose>{x:.2f} {y:.2f} {z - 0.1 + height / 2:.2f} 0 0 0</pose>'
        f"<geometry><cylinder><radius>{trunk_r:.2f}</radius><length>{height:.1f}</length></cylinder></geometry></collision>",
    ])


def write_forest(trees):
    """All trees as ONE static model: 1300 separate tree models ran the sim at 0.5x real time
    (physics overhead per model), one model with the same shapes runs at 1.0x."""
    root = os.path.join(MODELS, "sar_forest")
    os.makedirs(root, exist_ok=True)
    body = "\n".join(tree_sdf(i, *t) for i, t in enumerate(trees))
    with open(os.path.join(root, "model.sdf"), "w") as f:
        f.write(f"""<?xml version="1.0"?>
<!-- Generated by sim/tools/make_sar_world.py, do not edit. Fuel pine/oak trees, trunk-only collision. -->
<sdf version="1.9">
  <model name="sar_forest">
    <static>true</static>
    <link name="link">
{body}
    </link>
  </model>
</sdf>
""")
    write_config(root, "sar_forest", f"{len(trees)} trees for the SAR test world (generated).")


def write_wrappers():
    for name, (model, dae, tex) in HOUSES.items():
        root = os.path.join(MODELS, name)
        os.makedirs(root, exist_ok=True)
        mesh = fuel("openrobotics", model.lower(), 3, "meshes/" + dae)
        albedo = fuel("openrobotics", model.lower(), 3, "materials/textures/" + tex)
        with open(os.path.join(root, "model.sdf"), "w") as f:
            f.write(f"""<?xml version="1.0"?>
<!-- Generated by sim/tools/make_sar_world.py. Fuel {model} with a PBR material. -->
<sdf version="1.9">
  <model name="{name}">
    <static>true</static>
    <link name="link">
      <collision name="collision">
        <geometry><mesh><uri>{mesh}</uri><scale>1.5 1.5 1.5</scale></mesh></geometry>
      </collision>
      <visual name="visual">
        <geometry><mesh><uri>{mesh}</uri><scale>1.5 1.5 1.5</scale></mesh></geometry>
        <material>
          <diffuse>1 1 1 1</diffuse>
          <pbr><metal><albedo_map>{albedo}</albedo_map><roughness>0.9</roughness></metal></pbr>
        </material>
      </visual>
    </link>
  </model>
</sdf>
""")
        write_config(root, name, f"Fuel {model} with a PBR material (generated).")


# ---------------------------------------------------------------------------
# Placement

# name -> (uri, footprint radius in m, mesh centre offset (x, y) in m)
BUILDINGS = {
    "apartment": (f"{FUEL}/OpenRobotics/models/Apartment", 15.5, (0, 0)),
    "house_1": ("model://sar_house_1", 10.0, (-5.6, 0.4)),
    "house_2": ("model://sar_house_2", 11.0, (-3.7, -0.6)),
    "house_3": ("model://sar_house_3", 9.0, (0, 0)),
    "collapsed_house": (f"{FUEL}/OpenRobotics/models/Collapsed House", 11.0, (0, 0)),
    "collapsed_industrial": (
        f"{FUEL}/OpenRobotics/models/Collapsed industrial building",
        16.0,
        (0.8, 0),
    ),
    "collapsed_fire_station": (f"{FUEL}/chapulina/models/Fire station (collapsed)", 17.5, (0, 0)),
}
BUILDING_MIX = ["house_1", "house_2", "house_3", "apartment", "house_1", "house_2",
                "collapsed_house", "collapsed_house", "collapsed_industrial", "collapsed_fire_station"]

ACTOR = fuel("mingfei", "actor", 1, "meshes/")

# People that are a static Fuel model: name -> (uri, natural state)
PEOPLE = {
    "rescue_randy": (f"{FUEL}/OpenRobotics/models/Rescue Randy", "standing"),
    "rescue_randy_sitting": (f"{FUEL}/OpenRobotics/models/Rescue Randy Sitting", "sitting"),
    "standing_person": (f"{FUEL}/OpenRobotics/models/Standing person", "standing"),
    "walking_person": (f"{FUEL}/OpenRobotics/models/Walking person", "standing"),
    "male_visitor": (f"{FUEL}/OpenRobotics/models/Male visitor", "standing"),
    "casual_female": (f"{FUEL}/OpenRobotics/models/Casual female", "standing"),
}
UPRIGHT = ["rescue_randy", "standing_person", "walking_person", "male_visitor", "casual_female"]


class Placer:
    """Keeps a list of occupied circles so nothing overlaps."""

    def __init__(self):
        self.taken = []

    def free(self, x, y, r):
        return all(math.hypot(x - a, y - b) > r + c for a, b, c in self.taken)

    def take(self, x, y, r):
        self.taken.append((x, y, r))


def place_buildings(rng, placer, xs, h):
    out = []
    blocks = []
    sx = (TOWN[0],) + TOWN_STREETS_X
    sy = TOWN_STREETS_Y + (TOWN[3],)
    for i in range(len(sx) - 1):
        for j in range(len(sy) - 1):
            blocks.append((sx[i] + ROAD_HALF_WIDTH + 2, sx[i + 1] - ROAD_HALF_WIDTH - 2,
                           sy[j] + ROAD_HALF_WIDTH + 2, sy[j + 1] - ROAD_HALF_WIDTH - 2))
    for b in blocks:
        for _ in range(3):
            kind = BUILDING_MIX[rng.integers(len(BUILDING_MIX))]
            uri, r, (ox, oy) = BUILDINGS[kind]
            for _ in range(40):
                x = rng.uniform(b[0] + r, b[1] - r) if b[1] - b[0] > 2 * r else None
                y = rng.uniform(b[2] + r, b[3] - r) if b[3] - b[2] > 2 * r else None
                if x is None or y is None or not placer.free(x, y, r):
                    continue
                yaw = rng.integers(4) * math.pi / 2
                # Shift so the mesh's centre (not its origin) lands on (x, y).
                px = x - (ox * math.cos(yaw) - oy * math.sin(yaw))
                py = y - (ox * math.sin(yaw) + oy * math.cos(yaw))
                placer.take(x, y, r)
                out.append((kind, uri, px, py, TOWN_Z, yaw))
                break
    return out


def place_trees(rng, placer, xs, h, n_trees):
    out = []
    names = ["spruce_s", "spruce_m", "spruce_l"]
    tries = 0
    # Forest: west of FOREST_X, above the shore line.
    while len(out) < n_trees and tries < n_trees * 20:
        tries += 1
        x = rng.uniform(-HALF + 5, FOREST_X + 15)
        y = rng.uniform(-HALF + 5, HALF - 5)
        z = height_at(xs, h, x, y)
        if z < WATER_Z + 1 or road_mask(np.array(x), np.array(y)) > 0:
            continue
        if x > FOREST_X + 10 * math.sin(y / 40) or not placer.free(x, y, 1.5):
            continue
        placer.take(x, y, 1.5)
        out.append((names[rng.integers(3)], x, y, z, rng.uniform(0, 2 * math.pi)))
    # Clumps of trees in the open land and oaks in town.
    clumps = 0
    for _ in range(400):
        if clumps >= 14:
            break
        cx, cy = rng.uniform(-40, 280), rng.uniform(-280, 200)
        if in_box(cx, cy, TOWN, 10) or math.hypot(cx, cy) < 40 or height_at(xs, h, cx, cy) < WATER_Z + 1:
            continue
        clumps += 1
        for _ in range(rng.integers(4, 12)):
            x, y = cx + rng.normal(0, 7), cy + rng.normal(0, 7)
            if road_mask(np.array(x), np.array(y)) > 0 or not placer.free(x, y, 1.5):
                continue
            placer.take(x, y, 1.5)
            name = "oak" if rng.random() < 0.4 else names[rng.integers(3)]
            out.append((name, x, y, height_at(xs, h, x, y), rng.uniform(0, 2 * math.pi)))
    for _ in range(25):
        x, y = rng.uniform(TOWN[0], TOWN[1]), rng.uniform(TOWN[2], TOWN[3])
        if road_mask(np.array(x), np.array(y)) == 0 and placer.free(x, y, 3):
            placer.take(x, y, 3)
            out.append(("oak", x, y, TOWN_Z, rng.uniform(0, 2 * math.pi)))
    return out


def random_spot(rng, placer, xs, h, zone, trees, buildings):
    """A free spot in a zone. Forest people are put close to a tree, town people near buildings."""
    for _ in range(500):
        if zone == "forest":
            t = trees[rng.integers(len(trees))]
            if not t[0].startswith("spruce"):
                continue
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(1.5, 4)
            x, y = t[1] + d * math.cos(a), t[2] + d * math.sin(a)
        elif zone == "town":
            if rng.random() < 0.5:
                b = buildings[rng.integers(len(buildings))]
                r = BUILDINGS[b[0]][1]
                a, d = rng.uniform(0, 2 * math.pi), r + rng.uniform(1, 4)
                x, y = b[2] + d * math.cos(a), b[3] + d * math.sin(a)
            else:
                x, y = rng.uniform(TOWN[0], TOWN[1]), rng.uniform(TOWN[2], TOWN[3])
        else:  # open land: fields, meadows, along the road
            x, y = rng.uniform(-60, 280), rng.uniform(-280, 200)
            if in_box(x, y, TOWN, 5) or math.hypot(x, y) < 20:
                continue
        if abs(x) > HALF - 10 or abs(y) > HALF - 10 or not placer.free(x, y, 1.2):
            continue
        z = height_at(xs, h, x, y)
        if z < WATER_Z + 0.5:
            continue
        placer.take(x, y, 1.2)
        return x, y, z
    raise RuntimeError(f"no free spot in {zone}")


def place_people(rng, placer, xs, h, n, trees, buildings):
    """Returns (static people, actors). Each is a dict that also goes into the ground truth."""
    zones = ["open"] * (n * 3 // 10) + ["forest"] * (n * 4 // 10)
    zones += ["town"] * (n - len(zones))
    # What people do, per zone. Lying people are the main thing a SAR drone looks for.
    states = {
        "open": ["lying", "lying", "sitting", "standing", "walking"],
        "forest": ["lying", "lying", "lying", "sitting", "standing", "walking"],
        "town": ["lying", "lying", "sitting", "standing", "walking", "waving"],
    }
    people, actors = [], []
    for i, zone in enumerate(zones):
        state = states[zone][rng.integers(len(states[zone]))]
        x, y, z = random_spot(rng, placer, xs, h, zone, trees, buildings)
        yaw = rng.uniform(-math.pi, math.pi)
        p = {"id": f"person_{i:02d}", "zone": zone, "state": state,
             "x": round(x, 2), "y": round(y, 2), "z": round(z, 2)}
        if state in ("walking", "waving") or (state == "sitting" and rng.random() < 0.3):
            p["model"] = "actor"
            if state == "walking":
                # Walk back and forth along a short straight line.
                length = rng.uniform(8, 20)
                p["path"] = [[round(x, 2), round(y, 2)],
                             [round(x + length * math.cos(yaw), 2), round(y + length * math.sin(yaw), 2)]]
            actors.append((p, yaw))
        else:
            if state == "sitting":
                model = "rescue_randy_sitting"
            else:
                model = UPRIGHT[rng.integers(len(UPRIGHT))]
            p["model"] = model
            if state == "lying":
                p["face"] = "up" if rng.random() < 0.6 else "down"
                p["injured"] = bool(rng.random() < 0.6)
            people.append((p, yaw))
        p["yaw"] = round(yaw, 2)
    return people, actors


def person_sdf(p, yaw):
    uri = PEOPLE[p["model"]][0]
    roll, z = 0.0, p["z"]
    if p["state"] == "lying":
        # Tip a standing model over onto its back (or front), feet at (x, y).
        roll = -math.pi / 2 if p["face"] == "up" else math.pi / 2
        z += 0.15
    return f"""    <include>
      <name>{p['id']}</name>
      <uri>{uri}</uri>
      <static>true</static>
      <pose>{p['x']} {p['y']} {z:.2f} {roll:.3f} 0 {yaw:.3f}</pose>
    </include>"""


def actor_sdf(p, yaw):
    anim = {"walking": "walk", "waving": "talk_a", "sitting": "sitting"}[p["state"]]
    z = p["z"] + 1.0  # the actor's origin is at its hips
    if p["state"] == "walking":
        (x0, y0), (x1, y1) = p["path"]
        d = math.hypot(x1 - x0, y1 - y0)
        t = d / 1.1  # walking speed 1.1 m/s
        back = yaw + math.pi
        waypoints = [
            (0.0, x0, y0, yaw), (t, x1, y1, yaw), (t + 1.5, x1, y1, back),
            (2 * t + 1.5, x0, y0, back), (2 * t + 3.0, x0, y0, yaw),
        ]
        wp = "\n".join(
            f"          <waypoint><time>{tt:.2f}</time><pose>{wx} {wy} {z:.2f} 0 0 {wyaw:.3f}</pose></waypoint>"
            for tt, wx, wy, wyaw in waypoints
        )
        script = f"""      <script>
        <loop>true</loop>
        <auto_start>true</auto_start>
        <trajectory id="0" type="walk">
{wp}
        </trajectory>
      </script>"""
        pose = ""
    else:
        script = """      <script><loop>true</loop><auto_start>true</auto_start></script>"""
        pose = f"\n      <pose>{p['x']} {p['y']} {z:.2f} 0 0 {yaw:.3f}</pose>"
    traj_anim = {"walking": "walk"}.get(p["state"], anim)
    return f"""    <actor name="{p['id']}">{pose}
      <skin><filename>{ACTOR}{anim}.dae</filename><scale>1.0</scale></skin>
      <animation name="{traj_anim}"><filename>{ACTOR}{anim}.dae</filename><scale>1.0</scale><interpolate_x>true</interpolate_x></animation>
{script}
    </actor>"""


# ---------------------------------------------------------------------------


def write_world(buildings, trees, people, actors, seed):
    parts = []
    for i, (kind, uri, x, y, z, yaw) in enumerate(buildings):
        parts.append(f"""    <include>
      <name>{kind}_{i:02d}</name>
      <uri>{uri}</uri>
      <static>true</static>
      <pose>{x:.2f} {y:.2f} {z:.2f} 0 0 {yaw:.3f}</pose>
    </include>""")
    parts.append("    <include><uri>model://sar_forest</uri></include>")
    parts += [person_sdf(p, yaw) for p, yaw in people]
    parts += [actor_sdf(p, yaw) for p, yaw in actors]

    world = f"""<?xml version="1.0" encoding="UTF-8"?>
<!--
  CogiDrone search-and-rescue test world. GENERATED by sim/tools/make_sar_world.py (seed {seed}),
  edit the script and re-run it instead of editing this file.
  600 x 600 m: fjord (north), spruce forest on hills (west), fields (south), town with
  collapsed buildings (east). The drone spawns on the flat field at the origin.
  Where every person is: sar_people.yaml next to this file.
  The world name must match the file name (PX4 uses it for Gazebo service paths).
-->
<sdf version="1.9">
  <world name="sar">
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
      <ambient>0.45 0.45 0.48 1</ambient>
      <background>0.62 0.72 0.85 1</background>
      <shadows>true</shadows>
      <sky></sky>
    </scene>
    <light name="sun" type="directional">
      <pose>0 0 500 0 0 0</pose>
      <cast_shadows>true</cast_shadows>
      <intensity>1</intensity>
      <!-- Low northern sun from the south-west. -->
      <direction>0.5 0.6 -0.62</direction>
      <diffuse>0.95 0.92 0.85 1</diffuse>
      <specular>0.3 0.3 0.3 1</specular>
    </light>
    <include>
      <uri>model://sar_terrain</uri>
      <pose>0 0 0 0 0 0</pose>
    </include>
{chr(10).join(parts)}
    <spherical_coordinates>
      <surface_model>EARTH_WGS84</surface_model>
      <world_frame_orientation>ENU</world_frame_orientation>
      <latitude_deg>63.4186</latitude_deg>
      <longitude_deg>10.4027</longitude_deg>
      <elevation>50</elevation>
    </spherical_coordinates>
  </world>
</sdf>
"""
    with open(os.path.join(WORLDS, "sar.sdf"), "w") as f:
        f.write(world)


def write_ground_truth(people, actors, seed):
    rows = sorted([p for p, _ in people] + [p for p, _ in actors], key=lambda p: p["id"])
    lines = [
        "# Where every person in sar.sdf is. GENERATED by sim/tools/make_sar_world.py.",
        f"# seed: {seed}. Positions in the Gazebo world frame (ENU, m), drone spawns at 0 0.",
        "# state: standing | walking | sitting | lying | waving. walking people move along `path`.",
        "people:",
    ]
    for p in rows:
        fields = ", ".join(f"{k}: {v}" for k, v in p.items() if k != "id")
        lines.append(f"  - {{id: {p['id']}, {fields}}}")
    with open(os.path.join(WORLDS, "sar_people.yaml"), "w") as f:
        f.write("\n".join(lines).replace("True", "true").replace("False", "false") + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=1, help="Changes the terrain and where people are")
    ap.add_argument("--people", type=int, default=24)
    ap.add_argument("--trees", type=int, default=1200, help="Trees in the forest (fewer = faster)")
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    xs, h = make_heights(rng)
    write_wrappers()
    placer = Placer()
    placer.take(0, 0, 15)  # keep the launch spot clear
    buildings = place_buildings(rng, placer, xs, h)
    trees = place_trees(rng, placer, xs, h, args.trees)
    people, actors = place_people(rng, placer, xs, h, args.people, trees, buildings)
    write_terrain(xs, h, trees, args.seed)
    write_forest(trees)
    write_world(buildings, trees, people, actors, args.seed)
    write_ground_truth(people, actors, args.seed)
    print(f"sar.sdf: {len(buildings)} buildings, {len(trees)} trees, "
          f"{len(people) + len(actors)} people ({len(actors)} animated), seed {args.seed}")


if __name__ == "__main__":
    main()
