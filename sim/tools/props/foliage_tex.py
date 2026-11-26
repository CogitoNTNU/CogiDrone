"""Procedural alpha-card foliage textures (own work, CC0).

Each generator returns (rgba uint8 HxWx4, height float HxW) at the requested
size. Drawn at 2x then downsampled for anti-aliased edges.

  bilberry_sprigs : Vaccinium myrtillus - angular green twigs, small ovate leaves
  heather_sprigs  : Calluna vulgaris    - woody stems, tiny scale leaves, faded bloom
  juniper_branch  : Juniperus communis  - prickly blue-green needles
"""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


def _rgb(c, jitter, rng):
    return tuple(int(np.clip(v + rng.normal(0, jitter), 0, 255)) for v in c)


class Canvas:
    def __init__(self, w, h, ss=2):
        self.ss = ss
        self.W, self.H = w * ss, h * ss
        self.col = Image.new("RGB", (self.W, self.H), (0, 0, 0))
        self.alpha = Image.new("L", (self.W, self.H), 0)
        self.hgt = Image.new("L", (self.W, self.H), 0)
        self.dc = ImageDraw.Draw(self.col)
        self.da = ImageDraw.Draw(self.alpha)
        self.dh = ImageDraw.Draw(self.hgt)

    def line(self, pts, width, color, h=120):
        p = [(x * self.ss, y * self.ss) for x, y in pts]
        w = max(1, int(round(width * self.ss)))
        self.dc.line(p, fill=color, width=w, joint="curve")
        self.da.line(p, fill=255, width=w, joint="curve")
        self.dh.line(p, fill=h, width=w, joint="curve")
        r = w / 2.0
        for x, y in (p[0], p[-1]):
            self.dc.ellipse([x - r, y - r, x + r, y + r], fill=color)
            self.da.ellipse([x - r, y - r, x + r, y + r], fill=255)

    def poly(self, pts, color, h=200):
        p = [(x * self.ss, y * self.ss) for x, y in pts]
        self.dc.polygon(p, fill=color)
        self.da.polygon(p, fill=255)
        self.dh.polygon(p, fill=h)

    def finish(self, w, h):
        col = self.col.resize((w, h), Image.LANCZOS)
        al = self.alpha.resize((w, h), Image.LANCZOS)
        hg = self.hgt.filter(ImageFilter.GaussianBlur(self.ss * 1.5)).resize((w, h), Image.LANCZOS)
        rgba = np.dstack([np.asarray(col), np.asarray(al)])
        return rgba, np.asarray(hg, np.float32) / 255.0


def _leaf(cv, base, ang, length, width, color, rng, serr=0.0, tip=1.0):
    """Ovate leaf polygon starting at base pointing at angle ang (radians, image coords)."""
    n = 14
    ca, sa = math.cos(ang), math.sin(ang)
    left, right = [], []
    for i in range(n + 1):
        t = i / n
        # ovate profile: widest ~40% along the leaf
        wprof = math.sin(math.pi * t ** 0.8) ** (0.9 * tip) * width * 0.5
        if serr > 0 and 0.15 < t < 0.95:
            wprof *= 1.0 + serr * (1 if i % 2 else -1)
        x = t * length
        left.append((x, -wprof))
        right.append((x, wprof))
    pts = left + right[::-1]
    pts = [(base[0] + x * ca - y * sa, base[1] + x * sa + y * ca) for x, y in pts]
    cv.poly(pts, color, h=int(150 + rng.uniform(0, 80)))
    # lighter midrib
    mid = tuple(min(255, c + 22) for c in color)
    cv.line([base, (base[0] + 0.85 * length * ca, base[1] + 0.85 * length * sa)], max(1.0, width * 0.06), mid, h=230)


def _branch_path(x0, y0, ang, length, seg, wobble, rng):
    pts = [(x0, y0)]
    a = ang
    for _ in range(seg):
        a += rng.normal(0, wobble)
        x0 += math.cos(a) * length / seg
        y0 += math.sin(a) * length / seg
        pts.append((x0, y0))
    return pts


def _bilberry_sprig(cv, x, y, ang, length, rng, xmin, xmax, leaf_scale=1.0):
    leaf_base = np.array([72, 114, 36])
    stem_col = _rgb((78, 118, 44), 8, rng)
    main = _branch_path(x, y, ang, length, 6, 0.12, rng)
    cv.line(main, 5.0, stem_col)
    branches = []
    for i in range(1, len(main) - 1):
        for side in (-1, 1):
            if rng.random() < 0.75:
                a = ang + side * rng.uniform(0.45, 0.85)
                L = rng.uniform(0.2, 0.4) * length * (1.0 - 0.4 * i / len(main))
                bp = _branch_path(main[i][0], main[i][1], a, L, 3, 0.12, rng)
                cv.line(bp, 3.2, stem_col)
                branches.append(bp)
    branches.append(main[1:])
    for bp in branches:
        for j in range(1, len(bp)):
            for side in (-1, 1):
                if rng.random() < 0.9:
                    dx, dy = bp[j][0] - bp[j - 1][0], bp[j][1] - bp[j - 1][1]
                    a0 = math.atan2(dy, dx) + side * rng.uniform(0.6, 1.2)
                    L = rng.uniform(0.05, 0.075) * 1024 * leaf_scale
                    tipx = bp[j][0] + L * math.cos(a0)
                    if not (xmin + 3 < tipx < xmax - 3) or bp[j][1] + L * math.sin(a0) < 3:
                        continue
                    shade = rng.uniform(0.6, 1.12)
                    col = np.clip(leaf_base * shade + rng.normal(0, 8, 3), 0, 255)
                    if rng.random() < 0.05:  # occasional reddish leaf
                        col = col * np.array([1.35, 0.8, 0.6])
                    _leaf(cv, bp[j], a0, L, L * 0.62, tuple(int(c) for c in np.clip(col, 0, 255)), rng, serr=0.04)
        if rng.random() < 0.08:
            x2, y2 = bp[len(bp) // 2]
            r = 0.011 * cv.H
            cv.dc.ellipse([x2 * cv.ss - r, y2 * cv.ss - r, x2 * cv.ss + r, y2 * cv.ss + r], fill=(35, 40, 70))
            cv.da.ellipse([x2 * cv.ss - r, y2 * cv.ss - r, x2 * cv.ss + r, y2 * cv.ss + r], fill=255)


def bilberry_bush(w=1024, h=1024, seed=1, n=9):
    """Dense 'bush card': n bilberry sprigs fanning out from the bottom centre (half-dome silhouette)."""
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    for k in range(n):
        x = w / 2 + rng.normal(0, 0.14 * w)
        ang = -math.pi / 2 + (x - w / 2) / (w / 2) * 0.9 + rng.normal(0, 0.12)
        length = h * rng.uniform(0.6, 0.95) * (1 - 0.35 * abs(x - w / 2) / (w / 2))
        _bilberry_sprig(cv, x, h - 2, ang, length, rng, 0, w)
    return cv.finish(w, h)


def bilberry_hedge(w=1024, h=1024, seed=1, n=10, strips=2):
    """`strips` stacked 2:1 strips (each w x h/strips): a low, wide bilberry stand per strip.
    Used for wide alpha cards -> twice the ground coverage per triangle of a square card."""
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    sh = h / strips
    for s_ in range(strips):
        y0 = (s_ + 1) * sh - 2
        for k in range(n):
            x = rng.uniform(0.08, 0.92) * w
            ang = -math.pi / 2 + rng.normal(0, 0.35)
            length = sh * rng.uniform(0.6, 0.97)
            _bilberry_sprig_clip(cv, x, y0, ang, length, rng, s_ * sh, (s_ + 1) * sh, w)
    return cv.finish(w, h)


def _bilberry_sprig_clip(cv, x, y, ang, length, rng, ymin, ymax, w):
    """sprig whose leaves stay inside rows [ymin, ymax) and columns [0, w)."""
    leaf_base = np.array([72, 114, 36])
    stem_col = _rgb((74, 110, 42), 8, rng)
    main = _branch_path(x, y, ang, length, 5, 0.12, rng)
    main = [(min(max(px, 4), w - 4), max(py, ymin + 4)) for px, py in main]
    cv.line(main, 4.5, stem_col)
    branches = []
    for i in range(1, len(main) - 1):
        for side in (-1, 1):
            if rng.random() < 0.8:
                a = ang + side * rng.uniform(0.5, 1.0)
                L = rng.uniform(0.25, 0.5) * length
                bp = _branch_path(main[i][0], main[i][1], a, L, 3, 0.12, rng)
                bp = [(min(max(px, 4), w - 4), max(py, ymin + 4)) for px, py in bp]
                cv.line(bp, 3.0, stem_col)
                branches.append(bp)
    branches.append(main[1:])
    for bp in branches:
        for j in range(1, len(bp)):
            for side in (-1, 1):
                if rng.random() < 0.92:
                    dx, dy = bp[j][0] - bp[j - 1][0], bp[j][1] - bp[j - 1][1]
                    a0 = math.atan2(dy, dx) + side * rng.uniform(0.6, 1.2)
                    L = rng.uniform(0.05, 0.075) * 1024
                    tx, ty = bp[j][0] + L * math.cos(a0), bp[j][1] + L * math.sin(a0)
                    if not (3 < tx < w - 3 and ymin + 3 < ty < ymax - 3):
                        continue
                    shade = rng.uniform(0.6, 1.12)
                    col = np.clip(leaf_base * shade + rng.normal(0, 8, 3), 0, 255)
                    if rng.random() < 0.05:
                        col = col * np.array([1.35, 0.8, 0.6])
                    _leaf(cv, bp[j], a0, L, L * 0.62, tuple(int(c) for c in np.clip(col, 0, 255)), rng, serr=0.04)


def heather_hedge(w=1024, h=1024, seed=2, n=7, strips=2):
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    sh = h / strips
    wood = (88, 70, 52)
    for s_ in range(strips):
        y0 = (s_ + 1) * sh - 2
        for _ in range(n):
            x = rng.uniform(0.1, 0.9) * w
            main = _branch_path(x, y0, -math.pi / 2 + rng.normal(0, 0.3), sh * rng.uniform(0.2, 0.35), 3, 0.15, rng)
            cv.line(main, 5, wood)
            for i in range(1, len(main)):
                for _ in range(2):
                    ang = -math.pi / 2 + rng.normal(0, 0.55)
                    L = rng.uniform(0.45, 0.7) * sh
                    sp = _branch_path(main[i][0], main[i][1], ang, L, 5, 0.1, rng)
                    sp = [(min(max(px, 12), w - 12), max(py, s_ * sh + 14)) for px, py in sp]
                    cv.line(sp, 3, wood)
                    _heather_shoot(cv, sp, rng, 1024)
    return cv.finish(w, h)


def bilberry_sprigs(w=1024, h=1024, seed=1, n_sprigs=2):
    """n_sprigs separate upright sprigs side by side (each w/n wide)."""
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    sw = w / n_sprigs
    for s in range(n_sprigs):
        _bilberry_sprig(cv, sw * (s + 0.5), h - 2, -math.pi / 2, h * 0.92, rng, s * sw, (s + 1) * sw)
    return cv.finish(w, h)


def _heather_shoot(cv, sp, rng, h):
    for j in range(1, len(sp)):
        (x0, y0), (x1, y1) = sp[j - 1], sp[j]
        base_ang = math.atan2(y1 - y0, x1 - x0)
        for k in range(12):
            t = k / 12
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            for side in (-1, 1):
                a = base_ang + side * rng.uniform(0.3, 0.9)
                L = rng.uniform(0.016, 0.028) * h
                g = rng.uniform(0.75, 1.15)
                col = np.array([72, 86, 46]) * g + rng.normal(0, 6, 3)
                if rng.random() < 0.3:
                    col = np.array([104, 86, 62]) * g  # browned
                cv.line([(x, y), (x + L * math.cos(a), y + L * math.sin(a))], 5.0,
                        tuple(int(c) for c in np.clip(col, 0, 255)), h=200)
    if rng.random() < 0.35:
        for j in range(len(sp) - 2, len(sp)):
            x, y = sp[j]
            for _ in range(4):
                r = rng.uniform(0.006, 0.010) * h * cv.ss
                px, py = (x + rng.normal(0, 6)) * cv.ss, (y + rng.normal(0, 6)) * cv.ss
                c = _rgb((135, 100, 115), 12, rng) if rng.random() < 0.5 else _rgb((125, 95, 72), 12, rng)
                cv.dc.ellipse([px - r, py - r, px + r, py + r], fill=c)
                cv.da.ellipse([px - r, py - r, px + r, py + r], fill=255)
                cv.dh.ellipse([px - r, py - r, px + r, py + r], fill=220)


def heather_bush(w=1024, h=1024, seed=2, n=5):
    """Dense heather card: woody stems from the bottom centre, many leafy shoots."""
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    wood = (88, 70, 52)
    for _ in range(n):
        x = w / 2 + rng.normal(0, 0.12 * w)
        ang0 = -math.pi / 2 + (x - w / 2) / (w / 2) * 0.8
        main = _branch_path(x, h - 2, ang0, h * rng.uniform(0.3, 0.5), 4, 0.15, rng)
        cv.line(main, 6, wood)
        for i in range(1, len(main)):
            for _ in range(2):
                ang = ang0 + rng.normal(0, 0.45)
                L = rng.uniform(0.25, 0.45) * h
                ex = main[i][0] + math.cos(ang) * L
                if ex < 0.06 * w or ex > 0.94 * w:
                    ang = 0.5 * (ang + -math.pi / 2)
                sp = _branch_path(main[i][0], main[i][1], ang, L, 5, 0.1, rng)
                cv.line(sp, 3, wood)
                _heather_shoot(cv, sp, rng, h)
    return cv.finish(w, h)


def juniper_branch(w=1024, h=1024, seed=3):
    rng = np.random.default_rng(seed)
    cv = Canvas(w, h)
    wood = (85, 72, 60)
    main = _branch_path(w * 0.5, h - 2, -math.pi / 2, h * 0.9, 8, 0.08, rng)
    cv.line(main, 9, wood)
    twigs = []
    for i in range(1, len(main)):
        for side in (-1, 1, -1, 1):
            ang = -math.pi / 2 + side * rng.uniform(0.4, 1.1)
            L = rng.uniform(0.2, 0.42) * w * (1 - 0.4 * i / len(main))
            tp = _branch_path(main[i][0], main[i][1], ang, L, 5, 0.12, rng)
            cv.line(tp, 4, wood)
            twigs.append(tp)
    twigs.append(main)
    for tp in twigs:
        for j in range(1, len(tp)):
            (x0, y0), (x1, y1) = tp[j - 1], tp[j]
            base_ang = math.atan2(y1 - y0, x1 - x0)
            for k in range(9):
                t = k / 9
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                for side in (-1, 1):
                    a = base_ang + side * rng.uniform(0.5, 1.0)
                    L = rng.uniform(0.035, 0.055) * h
                    g = rng.uniform(0.8, 1.2)
                    col = np.array([66, 84, 62]) * g + rng.normal(0, 5, 3)
                    cv.line([(x, y), (x + L * math.cos(a), y + L * math.sin(a))], 4.0,
                            tuple(int(c) for c in np.clip(col, 0, 255)), h=210)
                    # glaucous stripe
                    cv.line([(x, y), (x + 0.7 * L * math.cos(a), y + 0.7 * L * math.sin(a))], 1.0,
                            tuple(int(c) for c in np.clip(col * np.array([1.3, 1.3, 1.25]), 0, 255)), h=230)
    return cv.finish(w, h)


def height_to_normal(hgt, strength=3.0):
    """Tangent-space normal map (OpenGL convention, +Y up in image = -row)."""
    gy, gx = np.gradient(hgt)
    nx, ny, nz = -gx * strength, gy * strength, np.ones_like(hgt)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    out = np.dstack([nx / n, ny / n, nz / n]) * 0.5 + 0.5
    return (out * 255).astype(np.uint8)


def dilate_rgb(rgba, iters=12):
    """Bleed opaque colours into transparent texels (prevents dark mip fringes)."""
    rgb = rgba[..., :3].astype(np.float32)
    a = (rgba[..., 3] > 8).astype(np.float32)
    acc = rgb * a[..., None]
    w = a.copy()
    from scipy.ndimage import uniform_filter
    for i in range(iters):
        k = 3 + 2 * i
        s_acc = np.stack([uniform_filter(acc[..., c], k) for c in range(3)], -1)
        s_w = uniform_filter(w, k)
        fill = (w == 0) & (s_w > 1e-4)
        acc[fill] = s_acc[fill] / s_w[fill, None]
        w[fill] = 1.0
    # anything still empty -> mean colour
    mean = rgb[a > 0].mean(0) if a.sum() else np.zeros(3)
    acc[w == 0] = mean
    out = rgba.copy()
    out[..., :3] = np.clip(acc, 0, 255).astype(np.uint8)
    return out
