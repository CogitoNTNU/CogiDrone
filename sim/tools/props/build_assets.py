#!/usr/bin/env python3
"""Build the real-time prop library in sim/assets/props from cached sources.

    ~/cogidrone_deps/simtools-venv/bin/python build_assets.py [--cache DIR] [--out DIR]

Pipeline per prop variant:
  * load Poly Haven glTF (Y-up) -> convert to Z-up, metres, origin = ground
    contact point (x/y = footprint centre, z = 0 at the ground line of the scan)
  * decimate with meshoptimizer (UV-seam aware) to a real-time triangle budget
  * remap UVs into one of three texture atlases (rock / plant / wood) so a
    merged chunk needs only ~3 draw calls
  * simplified convex hull for collision (solid kinds only)
Procedural assets (CC0, own work): bilberry / heather / juniper alpha-card
clumps and a glacially-polished granite slab ("svaberg") outcrop.

Outputs:
  out/meshes/<variant>.glb        visual mesh (atlas UVs, Z-up)
  out/textures/<atlas>_albedo.*   atlas albedo (png with alpha for plants)
  out/textures/<atlas>_normal.jpg atlas normal map (OpenGL convention)
  out/props_library.json          variant metadata + collision hulls
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageEnhance

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import foliage_tex as FT  # noqa: E402
from glb import write_glb  # noqa: E402

Image.MAX_IMAGE_PIXELS = None

# --------------------------------------------------------------------------
# Atlases: name -> (albedo size, normal size, tiles{key: (x, y, w) in [0,1] image coords, top-left})
ATLASES = {
    "rock": (4096, 2048, {
        "rock_moss_set_02": (0.0, 0.0, 0.5),
        "rock_moss_set_01": (0.5, 0.0, 0.5),
        "rock_face_02": (0.0, 0.5, 0.5),
        "rock_face_01": (0.5, 0.5, 0.25),
        "rock_07": (0.5, 0.75, 0.25),
        "tex_lichen_rock": (0.75, 0.75, 0.25),
    }),
    "plant": (2048, 1024, {
        "fern_02": (0.0, 0.0, 0.5),
        "bilberry": (0.5, 0.0, 0.5),
        "heather": (0.0, 0.5, 0.5),
        "juniper": (0.5, 0.5, 0.5),
    }),
    "wood": (2048, 2048, {
        "tree_stump_01": (0.0, 0.0, 0.5),
        "tree_stump_02": (0.5, 0.0, 0.5),
        "dead_tree_trunk": (0.0, 0.5, 0.5),
        "dry_branches_medium_01": (0.5, 0.5, 0.5),
    }),
}
# Per-material render settings used by the builder's SDF
MATERIALS = {
    "rock": {"roughness": 0.9, "alpha": False, "cast_shadows": True},
    "plant": {"roughness": 0.75, "alpha": True, "cast_shadows": True},
    "wood": {"roughness": 0.92, "alpha": False, "cast_shadows": True},
}
# colour grading of source textures to match Bymarka (grey granite/greenstone)
GRADE = {
    "boulder_01": {"sat": 0.45, "bright": 0.95},
    "rock_face_01": {"sat": 0.35, "bright": 0.95},
    "rock_07": {"sat": 0.8},
    "fern_02": {"sat": 0.9, "bright": 0.92},
    "tex_lichen_rock": {"sat": 0.65, "bright": 1.3},
}

# Scanned variants: (variant name, kind, source, node substring or None, target tris, opts)
SCANNED = [
    # boulders 1.5-3 m (rock_moss_set_01: mossy, rock_moss_set_02: grey lichen)
    ("boulder_moss_a", "boulder", "rock_moss_set_01", "rock01", 1000, {}),
    ("boulder_moss_b", "boulder", "rock_moss_set_01", "rock02", 1000, {}),
    ("boulder_moss_c", "boulder", "rock_moss_set_01", "rock03", 800, {}),
    ("boulder_moss_d", "boulder", "rock_moss_set_01", "rock04", 1000, {}),
    ("boulder_moss_e", "boulder", "rock_moss_set_01", "rock05", 1100, {}),
    ("boulder_moss_f", "boulder", "rock_moss_set_01", "rock06", 1000, {}),
    ("boulder_lichen_a", "boulder", "rock_moss_set_02", "rock07", 800, {}),
    ("boulder_lichen_b", "boulder", "rock_moss_set_02", "rock10", 800, {}),
    ("boulder_lichen_c", "boulder", "rock_moss_set_02", "rock11", 800, {}),
    ("boulder_lichen_d", "boulder", "rock_moss_set_02", "rock13", 800, {}),
    # rocks 0.3-1.5 m
    ("rock_lichen_a", "rock", "rock_moss_set_02", "rock08", 400, {}),
    ("rock_lichen_b", "rock", "rock_moss_set_02", "rock09", 400, {}),
    ("rock_lichen_c", "rock", "rock_moss_set_02", "rock12", 400, {}),
    ("rock_small", "rock", "rock_07", None, 300, {"ground": "sink10"}),
    # outcrop / rock face sections (scale x1.5-3 in the builder for 5-15 m)
    ("outcrop_face_a", "outcrop", "rock_face_01", None, 3000, {}),
    ("outcrop_face_b", "outcrop", "rock_face_02", None, 3000, {}),
    # plants
    ("fern_a", "fern", "fern_02", "_a", 160, {"two_faced": True}),
    ("fern_b", "fern", "fern_02", "_b", 300, {"two_faced": True}),
    ("fern_c", "fern", "fern_02", "_c", 300, {"two_faced": True}),
    ("fern_d", "fern", "fern_02", "_d", 160, {"two_faced": True}),
    # wood
    ("stump_a", "stump", "tree_stump_01", None, 800, {}),
    ("stump_b", "stump", "tree_stump_02", None, 800, {}),
    ("log_a", "log", "dead_tree_trunk", None, 1100, {}),
    ("branch_a", "branch", "dry_branches_medium_01", "_a", 300, {"sloppy": True}),
    ("branch_b", "branch", "dry_branches_medium_01", "_b", 300, {"sloppy": True}),
    ("branch_c", "branch", "dry_branches_medium_01", "_c", 300, {"sloppy": True}),
]
SOLID_KINDS = {"rock", "boulder", "outcrop", "log", "stump"}


# --------------------------------------------------------------------------
def y_up_to_z_up(v):
    return np.stack([v[:, 0], -v[:, 2], v[:, 1]], 1)


def load_scanned(cache, src, node_sub):
    import trimesh
    s = trimesh.load(os.path.join(cache, src, f"{src}_2k.gltf"))
    parts = []
    for name, g in s.graph.to_flattened().items():
        gn = g.get("geometry")
        if gn is None or (node_sub and not name.endswith(node_sub)):
            continue
        m = s.geometry[gn].copy()
        m.apply_transform(g["transform"])
        uv = np.asarray(m.visual.uv, np.float64).copy()
        uv[:, 1] = 1.0 - uv[:, 1]  # trimesh flips V on load -> back to glTF/image convention
        parts.append((y_up_to_z_up(np.asarray(m.vertices)), y_up_to_z_up(np.asarray(m.vertex_normals)),
                      uv, np.asarray(m.faces, np.uint32)))
    assert parts, (src, node_sub)
    # concatenate
    off, V, N, UV, F = 0, [], [], [], []
    for v, n, uv, f in parts:
        V.append(v), N.append(n), UV.append(uv), F.append(f + off)
        off += len(v)
    return np.concatenate(V), np.concatenate(N), np.concatenate(UV), np.concatenate(F)


def decimate(V, F, target_tris, attrs=None, sloppy=False):
    import meshoptimizer as mo
    idx = F.reshape(-1).astype(np.uint32)
    if len(F) <= target_tris:
        return F
    pos = np.ascontiguousarray(V, np.float32)
    dst = np.zeros_like(idx)
    import ctypes
    err = ctypes.c_float(1.0)  # the binding has no argtypes -> pass an explicit c_float
    if sloppy:  # ignores topology/seams: only for thin bark-textured parts where UV smear is invisible
        n = mo.simplify_sloppy(dst, idx, pos, target_index_count=target_tris * 3, target_error=1.0)
    elif attrs is not None:
        at = np.ascontiguousarray(attrs, np.float32)
        w = np.ones(at.shape[1], np.float32) * 0.5
        n = mo.simplify_with_attributes(dst, idx, pos, at, w, target_index_count=target_tris * 3,
                                        target_error=err)
    else:
        n = mo.simplify(dst, idx, pos, target_index_count=target_tris * 3, target_error=err)
    return dst[:n].reshape(-1, 3)


def compact(V, N, UV, F):
    used = np.unique(F)
    remap = np.full(len(V), -1, np.int64)
    remap[used] = np.arange(len(used))
    return V[used], N[used], UV[used], remap[F].astype(np.uint32)


def optimize_cache(F, nv):
    try:
        import meshoptimizer as mo
        idx = F.reshape(-1).astype(np.uint32)
        out = np.zeros_like(idx)
        mo.optimize_vertex_cache(out, idx, len(idx), nv)
        return out.reshape(-1, 3)
    except Exception:
        return F


def to_atlas(UV, atlas, key):
    x, y, w = ATLASES[atlas][2][key]
    # half-texel inset avoids sampling the neighbouring tile at the very edge
    inset = 3.0 / ATLASES[atlas][0]
    u = np.clip(UV[:, 0], 0, 1)
    v = np.clip(UV[:, 1], 0, 1)
    return np.stack([x + inset + u * (w - 2 * inset), y + inset + v * (w - 2 * inset)], 1)


def atlas_of(src):
    for a, (_, _, tiles) in ATLASES.items():
        if src in tiles:
            return a
    raise KeyError(src)


def two_faced(V, N, UV, F):
    """Duplicate faces with flipped winding+normals so the plant material can keep back-face culling."""
    n = len(V)
    return (np.concatenate([V, V]), np.concatenate([N, -N]), np.concatenate([UV, UV]),
            np.concatenate([F, F[:, ::-1] + n]))


def hull_simplified(V, max_pts=40):
    """Convex hull of the mesh reduced to <= max_pts support points (farthest point sampling)."""
    from scipy.spatial import ConvexHull
    h = ConvexHull(V)
    P = V[h.vertices]
    if len(P) > max_pts:
        sel = [int(np.argmax(np.linalg.norm(P - P.mean(0), axis=1)))]
        d = np.linalg.norm(P - P[sel[0]], axis=1)
        while len(sel) < max_pts:
            i = int(np.argmax(d))
            sel.append(i)
            d = np.minimum(d, np.linalg.norm(P - P[i], axis=1))
        P = P[sel]
    h = ConvexHull(P)
    F = h.simplices.copy()
    # orient outward
    c = P.mean(0)
    for k, f in enumerate(F):
        a, b, cc = P[f]
        if np.dot(np.cross(b - a, cc - a), a - c) < 0:
            F[k] = f[::-1]
    used = np.unique(F)
    remap = np.full(len(P), -1)
    remap[used] = np.arange(len(used))
    return P[used], remap[F]


# --------------------------------------------------------------------------
# procedural meshes
def card_clump(rng, n_cards, radius, height, tile_rects, aspect, tilt_range=(0.15, 1.1),
               ell=(1.0, 1.0), normal_bend=0.65, dome=True, bend=False, canopy_frac=0.3):
    """Alpha-card clump. tile_rects: list of (u0, v0, u1, v1) atlas rects (image coords, v down).
    Cards are fanned out from the clump centre and tilted outward, two-faced, with
    normals bent towards a dome around the clump for soft bush shading."""
    V, N, UV, F = [], [], [], []
    single = []  # per-face flag: canopy cards are single-faced (seen from above only)
    centre_low = np.array([0, 0, -0.6 * height])
    for i in range(n_cards):
        r = radius * math.sqrt(rng.uniform(0, 1))
        th = rng.uniform(0, 2 * math.pi)
        bx, by = r * math.cos(th) * ell[0], r * math.sin(th) * ell[1]
        rr = r / radius
        hc = height * (1.0 - 0.45 * rr ** 2 if dome else 1.0) * rng.uniform(0.75, 1.1)
        wc = hc * aspect * rng.uniform(0.85, 1.15)
        yaw = rng.uniform(0, 2 * math.pi)
        # tilt outward, more at the rim; some cards near-horizontal for top-down coverage
        canopy = rng.random() < canopy_frac
        tilt = rng.uniform(1.15, 1.45) if canopy else rng.uniform(*tilt_range) * (0.5 + 0.5 * rr)
        out_dir = th if r > 1e-3 else rng.uniform(0, 2 * math.pi)
        # card local frame: x across, z up; then tilt about axis perpendicular to out_dir
        cy, sy = math.cos(yaw), math.sin(yaw)
        across = np.array([cy, sy, 0.0])
        up = np.array([0.0, 0.0, 1.0])
        od = np.array([math.cos(out_dir), math.sin(out_dir), 0.0])
        up = up * math.cos(tilt) + od * math.sin(tilt)
        across = across - up * np.dot(across, up)
        across /= np.linalg.norm(across)
        fn = np.cross(across, up)
        base = np.array([bx, by, (rng.uniform(0.25, 0.6) * hc) if canopy else -0.02 * height])
        if canopy:  # canopy card: centre it over the base point, it lies across the top
            base = base - np.array([math.cos(out_dir), math.sin(out_dir), 0.0]) * 0.5 * hc * math.sin(tilt)
        rows = 3 if bend else 2
        u0, v0, u1, v1 = tile_rects[rng.integers(len(tile_rects))]
        if rng.random() < 0.5:
            u0, u1 = u1, u0  # mirror
        k0 = len(V)
        for j in range(rows):
            t = j / (rows - 1)
            droop = (0.12 * hc * t * t) if bend else 0.0
            for s in (0, 1):
                p = base + across * (s - 0.5) * wc + up * t * hc - np.array([0, 0, droop]) + od * droop * 0.5
                V.append(p)
                UV.append((u0 + (u1 - u0) * s, v1 + (v0 - v1) * t))
        up_facing = np.cross(across, up)[2] > 0
        for j in range(rows - 1):
            a, b, c, d = k0 + 2 * j, k0 + 2 * j + 1, k0 + 2 * j + 2, k0 + 2 * j + 3
            # winding: front face = +fn; canopy cards keep only their upward-facing side
            tri = [(a, b, d), (a, d, c)] if (not canopy or up_facing) else [(a, d, b), (a, c, d)]
            F.extend(tri)
            single.extend([canopy, canopy])
        for q in range(k0, len(V)):
            radial = V[q] - centre_low
            radial = radial / (np.linalg.norm(radial) + 1e-9)
            N.append(fn)
            N[-1] = (1 - normal_bend) * fn + normal_bend * radial
    V, N, UV, F = (np.array(V, np.float64), np.array(N, np.float64), np.array(UV, np.float64),
                   np.array(F, np.uint32))
    # back faces get their own bent normals (radial part kept, card part flipped)
    n = len(V)
    radial = V - centre_low
    radial /= np.linalg.norm(radial, axis=1, keepdims=True)
    card_n = (N - normal_bend * radial) / (1 - normal_bend)
    Nb = -(1 - normal_bend) * card_n + normal_bend * radial
    single = np.array(single, bool)
    # canopy cards: make sure the kept face's normal points up (bent normal with +z card part)
    vs = np.unique(F[single])
    flip = card_n[vs, 2] < 0
    N[vs[flip]] = Nb[vs[flip]]
    V2 = np.concatenate([V, V])
    N2 = np.concatenate([N, Nb])
    N2 /= np.linalg.norm(N2, axis=1, keepdims=True)
    UV2 = np.concatenate([UV, UV])
    F2 = np.concatenate([F, F[~single][:, ::-1] + n])
    return compact(V2, N2, UV2, F2)


def granite_slab(rng, lx=10.0, ly=6.0, hz=1.6, nx=37, ny=25, tile_m=3.0):
    """Glacially polished granite whaleback ('svaberg'): smooth dome with joint facets, skirt below ground."""
    xs = np.linspace(-1.12, 1.12, nx)
    ys = np.linspace(-1.12, 1.12, ny)
    X, Y = np.meshgrid(xs, ys)
    R2 = X ** 2 + Y ** 2
    dome = 1.0 - R2 ** 1.3          # smooth whaleback that keeps descending below ground past r=1
    # asymmetric: glacier-polished stoss side, steeper plucked lee side
    dome *= (1.0 + 0.25 * X)
    # joint planes -> facets / ledges
    for _ in range(4):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(0.35, 0.8)
        dome = np.minimum(dome, (d - (X * math.cos(a) + Y * math.sin(a))) * rng.uniform(1.2, 2.5) + 0.7)
    # low-frequency noise
    noise = np.zeros_like(X)
    for f, a in ((1.5, 0.06), (3.3, 0.03), (7.0, 0.012)):
        ph = rng.uniform(0, 6.28, 2)
        noise += a * np.sin(f * 3 * X + ph[0]) * np.cos(f * 3 * Y + ph[1])
    Z = (dome + noise) * hz
    Z = np.maximum(Z, -0.6 * hz)
    V = np.stack([X * lx / 2, Y * ly / 2, Z], -1).reshape(-1, 3)
    F = []
    for j in range(ny - 1):
        for i in range(nx - 1):
            a = j * nx + i
            F.append((a, a + 1, a + nx + 1))
            F.append((a, a + nx + 1, a + nx))
    F = np.array(F, np.uint32)
    # drop faces fully under ground
    F = F[(V[F][:, :, 2] > -0.59 * hz).any(1)]
    # normals
    import trimesh
    m = trimesh.Trimesh(V, F, process=False)
    N = np.asarray(m.vertex_normals)
    # mirror-tiled UVs: folds sit exactly on grid lines so no triangle straddles a fold;
    # gives ~2 m texture repeats inside a single (non-wrapping) atlas tile
    k = max(2, int(round(tile_m / (lx * 1.12 / (nx - 1)))))
    I, J = np.meshgrid(np.arange(nx), np.arange(ny))

    def tri(a):
        a = a % (2 * k)
        return np.where(a <= k, a, 2 * k - a) / k
    UV = np.stack([tri(I).reshape(-1), tri(J + k // 2).reshape(-1)], 1)
    UV = 0.04 + 0.92 * UV  # keep away from the tile border (mip bleeding from neighbouring tiles)
    return compact(V, N, UV, F)


# --------------------------------------------------------------------------
def grade(img, key):
    g = GRADE.get(key)
    if not g:
        return img
    if "sat" in g:
        img = ImageEnhance.Color(img).enhance(g["sat"])
    if "bright" in g:
        img = ImageEnhance.Brightness(img).enhance(g["bright"])
    return img


def build_atlases(cache, out, foliage):
    tdir = os.path.join(out, "textures")
    os.makedirs(tdir, exist_ok=True)
    flat_n = (128, 128, 255)
    for atlas, (asz, nsz, tiles) in ATLASES.items():
        is_alpha = MATERIALS[atlas]["alpha"]
        alb = Image.new("RGBA" if is_alpha else "RGB", (asz, asz), (90, 90, 80, 0) if is_alpha else (110, 110, 100))
        nrm = Image.new("RGB", (nsz, nsz), flat_n)
        for key, (x, y, w) in tiles.items():
            ta, tn = int(w * asz), int(w * nsz)
            if key in foliage:
                rgba, hgt = foliage[key]
                a_img = Image.fromarray(FT.dilate_rgb(rgba))
                n_img = Image.fromarray(FT.height_to_normal(hgt, 4.0))
            elif key.startswith("tex_"):
                t = key[4:]
                a_img = grade(Image.open(os.path.join(cache, key, f"{t}_diff.jpg")).convert("RGB"), key)
                n_img = Image.open(os.path.join(cache, key, f"{t}_nor_gl.jpg")).convert("RGB")
            else:
                td = os.path.join(cache, key, "textures")
                diff = [f for f in os.listdir(td) if "_diff_" in f][0]
                a_img = Image.open(os.path.join(td, diff)).convert("RGB")
                a_img = grade(a_img, key)
                if is_alpha:
                    al = [f for f in os.listdir(td) if "_alpha_" in f][0]
                    am = Image.open(os.path.join(td, al)).convert("L").resize(a_img.size)
                    rgba = np.dstack([np.asarray(a_img), np.asarray(am)])
                    a_img = Image.fromarray(FT.dilate_rgb(rgba))
                nf = [f for f in os.listdir(td) if "_nor_gl_" in f][0]
                n_img = Image.open(os.path.join(td, nf)).convert("RGB")
            a_img = a_img.convert("RGBA" if is_alpha else "RGB").resize((ta, ta), Image.LANCZOS)
            n_img = n_img.resize((tn, tn), Image.LANCZOS)
            alb.paste(a_img, (int(x * asz), int(y * asz)))
            nrm.paste(n_img, (int(x * nsz), int(y * nsz)))
        if is_alpha:
            # binarise alpha slightly towards opaque so thin features survive mip-mapping
            a = np.asarray(alb).copy()
            al = a[..., 3].astype(np.float32)
            a[..., 3] = np.clip((al - 60) * 1.6, 0, 255).astype(np.uint8)
            Image.fromarray(a).save(os.path.join(tdir, f"{atlas}_albedo.png"), optimize=True)
        else:
            alb.save(os.path.join(tdir, f"{atlas}_albedo.jpg"), quality=88, optimize=True)
        nrm.save(os.path.join(tdir, f"{atlas}_normal.jpg"), quality=90, optimize=True)
        print("atlas", atlas, flush=True)


def tile_rect(atlas, key, sub=(0, 0, 1, 1)):
    x, y, w = ATLASES[atlas][2][key]
    inset = 1.0 / ATLASES[atlas][0]
    return (x + w * sub[0] + inset, y + w * sub[1] + inset, x + w * sub[2] - inset, y + w * sub[3] - inset)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=os.path.expanduser("~/cogidrone_deps/simassets-cache/props"))
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                  "..", "..", "assets", "props"))
    ap.add_argument("--skip-textures", action="store_true")
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    mdir = os.path.join(out, "meshes")
    os.makedirs(mdir, exist_ok=True)

    if not a.skip_textures:
        foliage = {
            "bilberry": FT.bilberry_hedge(1024, 1024, seed=11),
            "heather": FT.heather_hedge(1024, 1024, seed=12),
            "juniper": FT.juniper_branch(1024, 1024, seed=13),
        }
        build_atlases(a.cache, out, foliage)

    lib = {"version": 1, "materials": {}, "variants": {}}
    for atlas, m in MATERIALS.items():
        ext = "png" if m["alpha"] else "jpg"
        lib["materials"][atlas] = dict(m, albedo=f"textures/{atlas}_albedo.{ext}", normal=f"textures/{atlas}_normal.jpg")

    def emit(name, kind, atlas, V, N, UV, F, src):
        N = N / (np.linalg.norm(N, axis=1, keepdims=True) + 1e-12)
        F = optimize_cache(F, len(V))
        write_glb(os.path.join(mdir, name + ".glb"), V, N, UV, F, name=name)
        ent = {"kind": kind, "material": atlas, "mesh": f"meshes/{name}.glb", "tris": int(len(F)),
               "bounds": [V.min(0).round(3).tolist(), V.max(0).round(3).tolist()], "source": src}
        if kind in SOLID_KINDS:
            hv, hf = hull_simplified(V[V[:, 2] > -0.3] if (V[:, 2] > -0.3).sum() > 8 else V)
            ent["hull"] = {"v": hv.round(4).tolist(), "f": hf.tolist()}
        lib["variants"][name] = ent
        print(f"  {name:18s} {kind:8s} {len(F):6d} tris  size {(V.max(0) - V.min(0)).round(2)}", flush=True)

    for name, kind, src, sub, target, opts in SCANNED:
        V, N, UV, F = load_scanned(a.cache, src, sub)
        V, N, UV, F = compact(V, N, UV, F)
        # origin: footprint centre (of the part above ground), z=0 at the scan's ground line
        above = V[:, 2] > 0.02 * (V[:, 2].max() - V[:, 2].min()) + V[:, 2].min() if V[:, 2].min() >= -0.05 else V[:, 2] > 0
        c = (V[above].min(0) + V[above].max(0)) / 2 if above.sum() > 10 else (V.min(0) + V.max(0)) / 2
        V = V - np.array([c[0], c[1], 0.0])
        if opts.get("ground") == "sink10":
            V[:, 2] -= V[:, 2].min() + 0.12 * (V[:, 2].max() - V[:, 2].min())
        elif V[:, 2].min() > -0.02:
            V[:, 2] -= V[:, 2].min()
        F2 = decimate(V, F, target, attrs=np.concatenate([UV, N * 0.2], 1), sloppy=opts.get("sloppy", False))
        V, N, UV, F2 = compact(V, N, UV, F2)
        atlas = atlas_of(src)
        UVa = to_atlas(UV, atlas, src)
        if opts.get("two_faced"):
            V, N, UVa, F2 = two_faced(V, N, UVa, F2)
        emit(name, kind, atlas, V, N, UVa, F2, f"polyhaven:{src}")

    rng = np.random.default_rng(7)
    # granite slab outcrops (lichen_rock tile, planar top projection)
    for i, (lx, ly, hz) in enumerate(((10.0, 6.0, 1.6), (7.0, 5.0, 1.1))):
        V, N, UV, F = granite_slab(rng, lx, ly, hz)
        emit(f"outcrop_slab_{'ab'[i]}", "outcrop", "rock", V, N, to_atlas(UV, "rock", "tex_lichen_rock"), F,
             "procedural+polyhaven:lichen_rock")

    # bilberry / heather: wide 2:1 'hedge strip' cards (two strips per tile), crossing + canopy cards
    bil = [tile_rect("plant", "bilberry", (0, 0, 1, 0.5)), tile_rect("plant", "bilberry", (0, 0.5, 1, 1))]
    for i, (n, r, h, ell) in enumerate(((5, 0.22, 0.30, (1, 1)), (12, 0.5, 0.32, (1, 0.8)),
                                        (22, 0.9, 0.34, (1, 0.65)), (40, 1.5, 0.34, (1, 0.8)))):
        V, N, UV, F = card_clump(rng, n, r, h, bil, aspect=2.0, ell=ell, canopy_frac=0.4)
        emit(f"bilberry_{'abcd'[i]}", "bilberry", "plant", V, N, UV, F, "procedural")
    hea = [tile_rect("plant", "heather", (0, 0, 1, 0.5)), tile_rect("plant", "heather", (0, 0.5, 1, 1))]
    for i, (n, r, h) in enumerate(((5, 0.22, 0.30), (12, 0.5, 0.34), (30, 1.2, 0.34))):
        V, N, UV, F = card_clump(rng, n, r, h, hea, aspect=2.0, tilt_range=(0.1, 0.9), canopy_frac=0.4)
        emit(f"heather_{'abc'[i]}", "heather", "plant", V, N, UV, F, "procedural")
    jun = [tile_rect("plant", "juniper")]
    V, N, UV, F = card_clump(rng, 30, 0.6, 0.55, jun, aspect=0.9, tilt_range=(0.5, 1.2), bend=True)
    emit("juniper_a", "juniper", "plant", V, N, UV, F, "procedural")
    V, N, UV, F = card_clump(rng, 34, 0.3, 1.1, jun, aspect=0.6, tilt_range=(0.05, 0.5), bend=True, dome=False)
    emit("juniper_b", "juniper", "plant", V, N, UV, F, "procedural")

    with open(os.path.join(out, "props_library.json"), "w") as f:
        json.dump(lib, f, indent=1)
    print("wrote", out)


if __name__ == "__main__":
    main()
