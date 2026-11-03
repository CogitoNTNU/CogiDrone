"""Tiny numpy software rasteriser (orthographic, textured, alpha-tested, z-buffered).

Used to bake the far-tier impostor sprites from the near trees and to draw preview sheets,
so the whole asset pipeline runs without Blender / a GPU.
"""
import numpy as np


def sample(tex, uv):
    """Nearest-neighbour lookup, uv in glTF convention (0,0 = top-left)."""
    h, w = tex.shape[:2]
    x = np.clip((uv[..., 0] * w).astype(np.int32), 0, w - 1)
    y = np.clip((uv[..., 1] * h).astype(np.int32), 0, h - 1)
    return tex[y, x]


def rasterize(xy, depth, uv, shade, tris, texs, tex_id, W, H, alpha_ref=0.5):
    """xy (N,2) pixel coords (y down), depth (N,) smaller = closer, uv (N,2), shade (N,) brightness,
    tris (M,3), texs list of float RGBA images, tex_id (M,) which texture each tri uses.
    Returns rgba (H,W,4) float and the z buffer."""
    col = np.zeros((H, W, 4), np.float32)
    zbuf = np.full((H, W), np.inf, np.float32)
    for t in range(len(tris)):
        i0, i1, i2 = tris[t]
        p0, p1, p2 = xy[i0], xy[i1], xy[i2]
        x0 = int(max(np.floor(min(p0[0], p1[0], p2[0])), 0)); x1 = int(min(np.ceil(max(p0[0], p1[0], p2[0])), W - 1))
        y0 = int(max(np.floor(min(p0[1], p1[1], p2[1])), 0)); y1 = int(min(np.ceil(max(p0[1], p1[1], p2[1])), H - 1))
        if x1 < x0 or y1 < y0:
            continue
        den = (p1[1] - p2[1]) * (p0[0] - p2[0]) + (p2[0] - p1[0]) * (p0[1] - p2[1])
        if abs(den) < 1e-9:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((p1[1] - p2[1]) * (gx - p2[0]) + (p2[0] - p1[0]) * (gy - p2[1])) / den
        w1 = ((p2[1] - p0[1]) * (gx - p2[0]) + (p0[0] - p2[0]) * (gy - p2[1])) / den
        w2 = 1.0 - w0 - w1
        m = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not m.any():
            continue
        z = w0 * depth[i0] + w1 * depth[i1] + w2 * depth[i2]
        zb = zbuf[y0:y1 + 1, x0:x1 + 1]
        m &= z < zb
        if not m.any():
            continue
        u = w0[..., None] * uv[i0] + w1[..., None] * uv[i1] + w2[..., None] * uv[i2]
        c = sample(texs[tex_id[t]], u[m])
        keep = c[:, 3] >= alpha_ref
        if not keep.any():
            continue
        s = (w0 * shade[i0] + w1 * shade[i1] + w2 * shade[i2])[m][keep]
        ys, xs = np.nonzero(m)
        ys = ys[keep] + y0; xs = xs[keep] + x0
        col[ys, xs, :3] = c[keep, :3] * s[:, None]
        col[ys, xs, 3] = 1.0
        zbuf[ys, xs] = z[m][keep]
    return col, zbuf


def downsample(rgba, f):
    """Box filter with alpha-weighted colour (no dark fringes)."""
    H, W = rgba.shape[:2]
    r = rgba[:H // f * f, :W // f * f].reshape(H // f, f, W // f, f, 4)
    a = r[..., 3].mean(axis=(1, 3))
    c = (r[..., :3] * r[..., 3:4]).sum(axis=(1, 3)) / np.maximum(r[..., 3:4].sum(axis=(1, 3)), 1e-6)
    return np.dstack([c, a])
