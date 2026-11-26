#!/usr/bin/env python3
"""Build the two shared tree texture atlases from the CC0 sources.

  bark.jpg     1024 x 2048  one 256 px column per species (spruce, pine, birch, alder),
               v = 0 (image bottom) is the trunk base, v = 1 (top) the tree top.
  foliage.png  2048 x 2048  RGBA needle / leaf "cluster" sprites composed from real
               photo-scanned twig and leaf cut-outs (Poly Haven, CC0).

The sprite rectangles are written to atlas.json (pixel rects, origin top-left) and are
read by treegen.py. Everything is seeded, so re-running gives the same images.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy import ndimage

from common import ASSET_DIR, CACHE, TEX_DIR

RNG = np.random.default_rng(7)


# --------------------------------------------------------------------------- helpers
def load_rgb(name):
    return np.asarray(Image.open(os.path.join(CACHE, name)).convert("RGB")).astype(np.float32) / 255.0


def load_alpha(name):
    a = np.asarray(Image.open(os.path.join(CACHE, name))).astype(np.float32)
    if a.ndim == 3:
        a = a[..., 0]
    return a / max(a.max(), 1.0)


def components(mask):
    lab, n = ndimage.label(mask)
    objs = ndimage.find_objects(lab)
    return lab, [(i + 1, s) for i, s in enumerate(objs)]


def make_sprite(rgb, alpha, lab, idx, sl, pad=6, grow=0):
    """Crop one cut-out: colour from rgb, alpha from the source alpha restricted to its component."""
    y0 = max(sl[0].start - pad, 0); y1 = min(sl[0].stop + pad, rgb.shape[0])
    x0 = max(sl[1].start - pad, 0); x1 = min(sl[1].stop + pad, rgb.shape[1])
    m = lab[y0:y1, x0:x1] == idx
    if grow:
        m = ndimage.binary_dilation(m, iterations=grow)
    a = alpha[y0:y1, x0:x1] * m
    c = rgb[y0:y1, x0:x1]
    out = np.dstack([c, a])
    return Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8), "RGBA")


def fir_twigs():
    """The six spruce/fir twig cut-outs of fir_tree_01 (needles pointing up in the source)."""
    rgb = load_rgb("fir_tree_01_twig_diff_2k.png")
    al = load_alpha("fir_tree_01_twig_alpha_2k.png")
    lab, comps = components(al > 0.5)
    out = []
    for i, s in comps:
        h = s[0].stop - s[0].start; w = s[1].stop - s[1].start
        if s[1].start > 360 and s[0].stop < 1740 and h * w > 6000:
            out.append(make_sprite(rgb, al, lab, i, s))
    return out


def pine_twigs():
    """Two Scots pine shoot cut-outs. In this alpha map the sprites are white needles inside
    black islands on a white background, so: twig = white pixels enclosed by the largest island."""
    rgb = load_rgb("pine_tree_01_twig_diff_2k.png")
    al = load_alpha("pine_tree_01_twig_alpha_2k.png")
    out = []
    for (x0, y0, x1, y1) in [(0, 0, 520, 940), (1280, 620, 2048, 1100)]:
        sub = al[y0:y1, x0:x1]
        lab, comps = components(sub < 0.5)
        big = max(comps, key=lambda c: (lab[c[1]] == c[0]).sum())[0]
        island = ndimage.binary_fill_holes(lab == big)
        twig = (sub > 0.5) & island
        tl, tc = components(ndimage.binary_dilation(twig, iterations=4))
        best = max(tc, key=lambda c: (tl[c[1]] == c[0]).sum())
        m = (tl == best[0]) & island
        out.append(make_sprite(rgb[y0:y1, x0:x1], sub, m.astype(np.int32), 1, best[1], pad=4))
    # second shoot lies roughly horizontally, base on the left: rotate so the tip points up
    out[1] = out[1].rotate(70, expand=True, resample=Image.BICUBIC)
    return out


def leaves(diff, alpha, open_r, amin, amax):
    rgb = load_rgb(diff)
    al = load_alpha(alpha)
    m = ndimage.binary_opening(al > 0.95, iterations=open_r)
    lab, comps = components(m)
    out = []
    for i, s in comps:
        area = (lab[s] == i).sum()
        h = s[0].stop - s[0].start; w = s[1].stop - s[1].start
        if amin < area < amax and area > 0.45 * h * w:
            lab_g = ndimage.binary_dilation(lab == i, iterations=open_r + 1)
            out.append(make_sprite(rgb, al, lab_g.astype(np.int32), 1, s, pad=open_r + 2))
    return out


def grade(img, mul, sat=1.0, gamma=1.0):
    a = np.asarray(img).astype(np.float32) / 255.0
    c = a[..., :3]
    g = c.mean(axis=2, keepdims=True)
    c = g + (c - g) * sat
    c = np.clip(c, 0, 1) ** gamma * np.asarray(mul, np.float32)
    return Image.fromarray((np.clip(np.dstack([c, a[..., 3:]]), 0, 1) * 255).astype(np.uint8), "RGBA")


def place(canvas, sprite, pos, angle_deg, length, brightness=1.0):
    """Paste sprite so that its 'up' axis points along angle_deg (0 = up, 90 = right, image coords)
    with its base at pos and its long side scaled to `length` px."""
    s = length / max(sprite.size)
    sp = sprite.resize((max(2, int(sprite.width * s)), max(2, int(sprite.height * s))), Image.LANCZOS)
    if brightness != 1.0:
        sp = grade(sp, (brightness,) * 3)
    w, h = sp.size
    rot = sp.rotate(-angle_deg, expand=True, resample=Image.BICUBIC)
    # sprite base = bottom centre; find where it lands after rotation about the centre
    a = math.radians(angle_deg)
    bx, by = 0.0, h / 2.0  # base relative to centre (y down)
    rx = bx * math.cos(a) - by * math.sin(a)
    ry = bx * math.sin(a) + by * math.cos(a)
    cx = pos[0] - rx; cy = pos[1] - ry
    x = int(cx - rot.width / 2); y = int(cy - rot.height / 2)
    left = max(0, -x); top = max(0, -y)
    right = min(rot.width, canvas.width - x); bottom = min(rot.height, canvas.height - y)
    if right <= left or bottom <= top:
        return
    canvas.alpha_composite(rot.crop((left, top, right, bottom)), (x + left, y + top))


def underlayer(canvas, blur, thresh, colour, keep=1.0):
    """Fill the gaps inside a cluster with an opaque darker mass so it does not dissolve
    when the GPU picks small mip levels (Gazebo alpha-tests at 0.5 without alpha-to-coverage)."""
    a = np.asarray(canvas)[..., 3].astype(np.float32) / 255.0
    b = ndimage.gaussian_filter(a, blur)
    m = np.clip((b - thresh) * 6.0, 0, 1) * keep
    under = np.zeros(a.shape + (4,), np.float32)
    noise = ndimage.gaussian_filter(RNG.random(a.shape), 2.0)
    noise = (noise - noise.mean()) / (noise.std() + 1e-6)
    for k in range(3):
        under[..., k] = np.clip(colour[k] * (1.0 + 0.18 * noise), 0, 1)
    under[..., 3] = m
    u = Image.fromarray((under * 255).astype(np.uint8), "RGBA")
    u.alpha_composite(canvas)
    return u


def bleed(img):
    """Push colour into fully transparent pixels (nearest opaque colour) so mip-mapping and
    bilinear filtering don't pull in black/white halos at cut-out edges."""
    a = np.asarray(img).astype(np.float32)
    al = a[..., 3]
    m = al > 8
    if m.all() or not m.any():
        return img
    idx = ndimage.distance_transform_edt(~m, return_distances=False, return_indices=True)
    c = a[..., :3][idx[0], idx[1]]
    # soften the bled colour a bit
    c = ndimage.uniform_filter(c, size=(5, 5, 1))
    a[..., :3] = np.where(m[..., None], a[..., :3], c)
    return Image.fromarray(a.astype(np.uint8), "RGBA")


# --------------------------------------------------------------------------- foliage sprites
def spruce_spray(twigs, w=1024, h=384, seed=0, mul=(0.86, 1.04, 0.80)):
    """One Norway-spruce branch seen from above / the side: a woody axis with twig sprays on
    both sides, densest near the tip, with a dark opaque core."""
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cv)
    y0 = h / 2
    axis = [(x, y0 + 10 * math.sin(x / w * 2.4 + seed)) for x in np.linspace(8, w - 30, 40)]
    for i in range(len(axis) - 1):
        d.line([axis[i], axis[i + 1]], fill=(70, 52, 38, 255), width=int(9 - 6 * i / len(axis)))
    tw = [grade(t, mul, sat=0.9) for t in twigs]
    n = 30
    for k in range(n):
        s = (k + rng.random() * 0.6) / n
        x = 20 + s * (w - 70)
        y = y0 + 10 * math.sin(x / w * 2.4 + seed)
        reach = (0.35 + 0.65 * min(1.0, s * 1.6)) * (h * 0.40)
        for side in (-1, 1):
            ang = 90 - side * (35 + 30 * rng.random())
            t = tw[rng.integers(len(tw))]
            place(cv, t, (x, y), ang, reach * (0.75 + 0.4 * rng.random()), 0.8 + 0.35 * rng.random())
    for k in range(4):  # tip
        t = tw[rng.integers(len(tw))]
        place(cv, t, (w - 120 + 25 * k, y0), 90 + rng.normal(0, 12), h * 0.36, 1.0)
    cv = underlayer(cv, 9, 0.30, (0.15, 0.21, 0.12))
    return cv


def spruce_core(twigs, w=256, h=768, seed=0, mul=(0.66, 0.82, 0.64)):
    """Cone-shaped dense mass used on the inner crossed cards so the crown never looks see-through."""
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    tw = [grade(t, mul, sat=0.85) for t in twigs]
    for k in range(260):
        v = rng.random() ** 0.8
        half = (w / 2 - 20) * v
        x = w / 2 + rng.uniform(-half, half)
        y = 20 + v * (h - 40)
        ang = rng.uniform(-110, 110) + (90 if x > w / 2 else -90) * 0.5
        place(cv, tw[rng.integers(len(tw))], (x, y), ang, 70 + 90 * v, 0.7 + 0.4 * rng.random())
    return underlayer(cv, 7, 0.25, (0.08, 0.12, 0.07))


def spruce_leader(twigs, w=256, h=384, seed=0, mul=(0.68, 0.84, 0.64)):
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cv)
    d.line([(w / 2, h - 4), (w / 2, 30)], fill=(80, 60, 40, 255), width=5)
    tw = [grade(t, mul) for t in twigs]
    for k in range(14):
        y = h - 10 - k * (h - 50) / 14
        sc = 0.3 + 0.7 * (1 - k / 14)
        for side in (-1, 1):
            place(cv, tw[rng.integers(len(tw))], (w / 2, y), side * (30 + 30 * rng.random()), 140 * sc, 0.9)
    place(cv, tw[0], (w / 2, 90), 0, 110, 1.0)
    return underlayer(cv, 6, 0.3, (0.1, 0.15, 0.09))


def pine_tuft(twigs, w=448, h=448, seed=0, mul=(0.95, 1.05, 0.82)):
    """Scots pine needle tuft clump: shoots radiating up and out from a hidden base."""
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cv)
    tw = [grade(t, mul, sat=0.8) for t in twigs]
    base = (w / 2, h * 0.78)
    for k in range(7):
        a = math.radians(rng.uniform(-60, 60))
        L = rng.uniform(0.18, 0.28) * h
        tip = (base[0] + L * math.sin(a), base[1] - L * math.cos(a))
        d.line([base, tip], fill=(95, 70, 50, 255), width=6)
        for j in range(3):
            place(cv, tw[rng.integers(len(tw))], tip, math.degrees(a) + rng.normal(0, 25),
                  rng.uniform(0.22, 0.30) * h, 0.8 + 0.35 * rng.random())
    for k in range(10):
        a = rng.uniform(-90, 90)
        place(cv, tw[rng.integers(len(tw))], (base[0] + rng.normal(0, 25), base[1] - rng.uniform(0, 60)),
              a * 0.7, rng.uniform(0.22, 0.32) * h, 0.75 + 0.35 * rng.random())
    return underlayer(cv, 10, 0.34, (0.20, 0.24, 0.15), keep=0.9)


def leaf_cluster(leafs, w=448, h=448, seed=0, mul=(1.0, 1.0, 1.0), n_twigs=7, n_leaves=160,
                 leaf_len=(26, 44), droop=0.6, under=None, twig_col=(70, 50, 45)):
    """Birch / alder leafy spray: thin twigs radiating from the card centre, bending down by
    `droop` (silver birch weeps), leaves along them."""
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cv)
    lv = [grade(l, mul, sat=1.05) for l in leafs]
    paths = []
    for k in range(n_twigs):
        a0 = (k + rng.random()) / n_twigs * 2 * math.pi
        p = np.array([w / 2 + rng.normal(0, 15), h * 0.40 + rng.normal(0, 15)])
        pts = [p.copy()]
        dirv = np.array([math.cos(a0), -abs(math.sin(a0)) * 0.8])
        L = rng.uniform(0.38, 0.5) * w
        for s in range(10):
            dirv = dirv + np.array([0, droop * 0.15])
            dirv = dirv / np.linalg.norm(dirv)
            p = p + dirv * L / 10
            p = np.clip(p, 8, w - 8)
            pts.append(p.copy())
        paths.append(pts)
        d.line([tuple(q) for q in pts], fill=twig_col + (255,), width=3)
    for k in range(n_leaves):
        pts = paths[rng.integers(len(paths))]
        i = rng.integers(1, len(pts))
        q = pts[i] + rng.normal(0, 9, 2)
        ang = rng.uniform(0, 360)
        place(cv, lv[rng.integers(len(lv))], tuple(q), ang, rng.uniform(*leaf_len), 0.72 + 0.5 * rng.random())
    if under is not None:
        cv = underlayer(cv, 8, 0.38, under, keep=0.9)
    return cv


def dense_clump(twigs, w=256, h=256, seed=0, mul=(0.72, 0.88, 0.74)):
    """Juniper: dense prickly mass."""
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    tw = [grade(t, mul, sat=0.7) for t in twigs]
    for k in range(90):
        r = rng.random() ** 0.6 * w * 0.38
        a = rng.uniform(0, 2 * math.pi)
        place(cv, tw[rng.integers(len(tw))], (w / 2 + r * math.cos(a), h / 2 + r * math.sin(a) * 1.1),
              rng.uniform(-60, 60), rng.uniform(50, 90), 0.75 + 0.4 * rng.random())
    return underlayer(cv, 6, 0.3, (0.12, 0.16, 0.13))


def dead_branch(w=512, h=128, seed=0):
    rng = np.random.default_rng(seed)
    cv = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(cv)
    col = (88, 80, 72, 255)

    def br(p, ang, L, wd, depth):
        q = (p[0] + L * math.cos(ang), p[1] + L * math.sin(ang))
        d.line([p, q], fill=col, width=max(1, int(wd)))
        if depth > 0:
            for k in range(3):
                t = rng.uniform(0.3, 0.9)
                pp = (p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1]))
                br(pp, ang + rng.choice([-1, 1]) * rng.uniform(0.4, 0.9), L * rng.uniform(0.25, 0.45), wd * 0.6, depth - 1)

    br((4, h / 2), 0.0, w - 20, 6, 3)
    return cv


# --------------------------------------------------------------------------- bark
def tile_column(src, w, h, tile):
    """src tileable square -> column w x h by tiling `tile`-px copies."""
    t = Image.fromarray((np.clip(src, 0, 1) * 255).astype(np.uint8)).resize((tile, tile), Image.LANCZOS)
    col = Image.new("RGB", (w, h))
    for y in range(0, h, tile):
        for x in range(0, w, tile):
            col.paste(t, (x, y))
    return np.asarray(col).astype(np.float32) / 255.0


def birch_bark(w, h, rough):
    rng = np.random.default_rng(3)
    y = np.linspace(1, 0, h)[:, None]  # 1 at top (image row 0), 0 at base
    base = np.ones((h, w, 3), np.float32) * np.array([0.88, 0.87, 0.83])
    n = ndimage.gaussian_filter(rng.random((h, w)), (6, 2), mode="wrap")
    n = (n - n.mean()) / n.std()
    base *= (1 + 0.04 * n)[..., None]
    img = Image.fromarray((base * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # horizontal lenticels
    for k in range(int(w * h / 700)):
        cx = rng.uniform(0, w); cy = rng.uniform(0, h)
        L = rng.uniform(6, 34); t = rng.uniform(1.2, 3.2)
        g = int(rng.uniform(40, 95))
        for dx in (-w, 0, w):
            d.ellipse([cx - L / 2 + dx, cy - t / 2, cx + L / 2 + dx, cy + t / 2], fill=(g, g - 3, g - 6))
    # dark diamond / patch scars
    for k in range(int(h / 110)):
        cx = rng.uniform(0, w); cy = rng.uniform(0, h); s = rng.uniform(5, 14)
        for dx in (-w, 0, w):
            d.polygon([(cx + dx, cy - s), (cx + dx + s * 1.3, cy), (cx + dx, cy + s * 0.8), (cx + dx - s * 1.3, cy)],
                      fill=(45, 42, 40))
    a = np.asarray(img.filter(ImageFilter.GaussianBlur(0.6))).astype(np.float32) / 255.0
    # rough black-grey fissured base
    m = np.clip((0.22 - y) / 0.12, 0, 1)
    patch = ndimage.gaussian_filter(rng.random((h, w)), (14, 6), mode="wrap")
    patch = (patch - patch.mean()) / patch.std()
    m = np.clip(m + np.clip((0.35 - y) / 0.2, 0, 1) * (patch > 0.6), 0, 1)[..., None]
    r = rough * np.array([0.55, 0.52, 0.5]) * 0.75
    out = a * (1 - m) + r * m
    # slightly warmer, darker near the top (thin twiggy top is reddish-brown)
    top = np.clip((y - 0.85) / 0.15, 0, 1)[..., None]
    return out * (1 - top) + np.array([0.45, 0.33, 0.28]) * top


def build_bark():
    W, H, C = 1024, 2048, 256
    fir = load_rgb("fir_tree_01_bark_diff_1k.jpg")
    pine = load_rgb("pine_tree_01_bark_diff_1k.jpg")
    b14 = load_rgb("Bark014_Color.jpg")
    b06 = load_rgb("Bark006_Color.jpg")
    b07 = load_rgb("Bark007_Color.jpg")
    y = np.linspace(1, 0, H)[:, None, None]
    out = np.zeros((H, W, 3), np.float32)
    # spruce: grey-brown scaly bark, a little redder towards the top
    sp = 0.6 * tile_column(fir, C, H, 256) + 0.4 * tile_column(b14, C, H, 256) * np.array([0.8, 0.75, 0.75])
    sp = (sp * 1.25 + np.array([0.03, 0.02, 0.02])) * (1 - 0.25 * np.clip((y - 0.6) / 0.4, 0, 1)) + np.array([0.08, 0.03, 0.0]) * np.clip((y - 0.6) / 0.4, 0, 1)
    out[:, 0:C] = sp
    # pine: dark furrowed base -> flaky orange upper trunk
    low = tile_column(pine, C, H, 256) * 0.95
    upp = tile_column(b14, C, H, 128)
    g = upp.mean(axis=2, keepdims=True)
    upp = np.clip(g * np.array([1.30, 0.86, 0.60]) + (upp - g) * 0.8 + np.array([0.10, 0.04, 0.0]), 0, 1)
    upp = upp * (0.75 + 0.5 * ndimage.gaussian_filter(np.asarray(RNG.random(upp.shape[:2])), (10, 3))[..., None])
    t = np.clip((y - 0.30) / 0.25, 0, 1)
    out[:, C:2 * C] = low * (1 - t) + upp * t
    # birch
    out[:, 2 * C:3 * C] = birch_bark(C, H, tile_column(b06, C, H, 256))
    # alder: smooth grey-green / brownish
    al = tile_column(b07, C, H, 256)
    g = al.mean(axis=2, keepdims=True)
    out[:, 3 * C:4 * C] = np.clip(g * np.array([0.82, 0.82, 0.76]) + (al - g) * 0.4, 0, 1) * 0.85
    img = Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8))
    img.save(os.path.join(TEX_DIR, "bark.jpg"), quality=88)
    cols = {"spruce": 0, "pine": 1, "birch": 2, "alder": 3, "juniper": 0}
    return {k: [v * C / W, (v + 1) * C / W] for k, v in cols.items()}


# --------------------------------------------------------------------------- packing
def build_foliage():
    fir = fir_twigs()
    pin = pine_twigs()
    birch_l = leaves("tree_small_02_leaves_diff_2k.png", "tree_small_02_leaves_alpha_2k.png", 6, 4000, 60000)
    alder_l = leaves("island_tree_01_leaves_diff_2k.png", "island_tree_01_leaves_alpha_2k.png", 3, 20000, 400000)
    print(f"cut-outs: fir {len(fir)}, pine {len(pin)}, small leaves {len(birch_l)}, big leaves {len(alder_l)}")
    S = {}
    S["spruce_spray0"] = spruce_spray(fir, seed=1)
    S["spruce_spray1"] = spruce_spray(fir, seed=2, mul=(0.78, 1.0, 0.84))
    S["spruce_spray2"] = spruce_spray(fir, seed=3, mul=(0.90, 1.03, 0.72))
    S["spruce_core"] = spruce_core(fir, seed=4)
    S["spruce_leader"] = spruce_leader(fir, seed=5)
    S["juniper0"] = dense_clump(fir, seed=6)
    S["juniper1"] = dense_clump(fir, seed=7, mul=(0.74, 0.84, 0.82))
    S["pine_tuft0"] = pine_tuft(pin, seed=8)
    S["pine_tuft1"] = pine_tuft(pin, seed=9, mul=(0.88, 1.0, 0.84))
    S["pine_tuft2"] = pine_tuft(pin, seed=10, mul=(1.0, 1.05, 0.78))
    S["birch0"] = leaf_cluster(birch_l, seed=11, mul=(1.05, 1.22, 0.70), droop=1.0)
    S["birch1"] = leaf_cluster(birch_l, seed=12, mul=(1.12, 1.25, 0.65), droop=1.4, n_leaves=180)
    S["birch2"] = leaf_cluster(birch_l, seed=13, mul=(0.98, 1.15, 0.72), droop=0.8)
    S["alder0"] = leaf_cluster(alder_l, seed=14, mul=(0.62, 0.80, 0.52), n_twigs=8, n_leaves=120,
                               leaf_len=(40, 62), droop=0.3, under=(0.10, 0.16, 0.07))
    S["alder1"] = leaf_cluster(alder_l, seed=15, mul=(0.70, 0.86, 0.52), n_twigs=8, n_leaves=120,
                               leaf_len=(40, 62), droop=0.4, under=(0.11, 0.17, 0.07))
    S["dead_branch"] = dead_branch(seed=16)
    # fixed layout in the 2048 x 2048 atlas: name -> (x, y)
    layout = {
        "spruce_spray0": (0, 0), "spruce_spray1": (1024, 0), "spruce_spray2": (0, 384),
        "juniper0": (1024, 384), "juniper1": (1280, 384), "dead_branch": (1024, 640),
        "spruce_leader": (1536, 384), "spruce_core": (1792, 384),
        "pine_tuft0": (0, 768), "pine_tuft1": (448, 768), "pine_tuft2": (896, 768), "birch0": (1344, 768),
        "birch1": (0, 1216), "birch2": (448, 1216), "alder0": (896, 1216), "alder1": (1344, 1216),
    }
    W = H = 2048
    atlas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    rects = {}
    for k in S:  # fade alpha to 0 at the sprite border: no straight card edges, no mip bleeding
        a = np.asarray(S[k]).astype(np.float32)
        h, w = a.shape[:2]
        e = np.minimum(np.minimum(np.arange(h)[:, None], h - 1 - np.arange(h)[:, None]),
                       np.minimum(np.arange(w)[None], w - 1 - np.arange(w)[None]))
        a[..., 3] *= np.clip((e - 3) / 10.0, 0, 1)
        S[k] = Image.fromarray(a.astype(np.uint8), "RGBA")
    for k, (x, y) in layout.items():
        im = S[k]
        assert x + im.width <= W and y + im.height <= H, k
        atlas.alpha_composite(im, (x, y))
        rects[k] = [x, y, im.width, im.height]
    atlas = bleed(atlas)
    atlas.save(os.path.join(TEX_DIR, "foliage.png"), optimize=True)
    return rects


def main():
    os.makedirs(TEX_DIR, exist_ok=True)
    bark = build_bark()
    rects = build_foliage()
    meta = {"foliage_size": [2048, 2048], "foliage": rects, "bark_size": [1024, 2048], "bark_u": bark}
    json.dump(meta, open(os.path.join(ASSET_DIR, "atlas.json"), "w"), indent=1)
    print("wrote", TEX_DIR)


if __name__ == "__main__":
    main()
