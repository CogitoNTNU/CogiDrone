"""BC1 (DXT1) .dds writer in numpy: 4 bits per texel, 1/8 of the RGBA8 texture Ogre makes from a JPG.

On the Jetson the GPU shares system RAM, and Gazebo's server (camera sensor) and GUI each load
every texture, so the 128 baked 4K terrain textures cost ~11 GB per process as JPG and ~1.5 GB as
BC1. Gazebo/Ogre2 loads .dds albedo maps as they are (mipmaps included, nothing decoded).

    from dxt import save_bc1
    save_bc1("tile.dds", rgb_uint8_array)   # (h, w, 3), h and w multiples of 4
"""

import struct

import numpy as np
from PIL import Image


def _to565(c):
    c = np.clip(np.rint(c * np.array([31, 63, 31]) / 255), 0, [31, 63, 31]).astype(np.uint16)
    return (c[..., 0] << 11) | (c[..., 1] << 5) | c[..., 2]


def _from565(v):
    v = v.astype(np.int32)
    return np.stack([(v >> 11) & 31, (v >> 5) & 63, v & 31], -1) * (255.0 / np.array([31, 63, 31]))


def encode_bc1(rgb):
    """(h, w, 3) uint8 -> BC1 blocks as bytes (4-colour mode, no alpha)."""
    h, w, _ = rgb.shape
    out = []
    step = max(4, (1 << 20) // w * 4 // 4)  # rows per pass, bounds memory
    for r0 in range(0, h, step):
        a = rgb[r0:r0 + step].astype(np.float32)
        bh = a.shape[0] // 4
        blk = a.reshape(bh, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3)
        mean = blk.mean(1, keepdims=True)
        d = blk - mean
        cov = np.einsum("nki,nkj->nij", d, d)
        axis = np.ones((len(blk), 3), np.float32)
        for _ in range(6):  # power iteration: principal colour axis
            axis = np.einsum("nij,nj->ni", cov, axis)
            axis /= np.linalg.norm(axis, axis=1, keepdims=True) + 1e-12
        t = np.einsum("nki,ni->nk", d, axis)
        lo, hi = t.min(1), t.max(1)
        inset = (hi - lo) / 32  # pull the ends in a little: less error for the middle colours
        c0 = mean[:, 0] + axis * (hi - inset)[:, None]
        c1 = mean[:, 0] + axis * (lo + inset)[:, None]
        e0, e1 = _to565(c0), _to565(c1)
        # 4-colour mode needs e0 > e1; equal ends = flat block, index 0 everywhere.
        swap = e0 < e1
        e0[swap], e1[swap] = e1[swap], e0[swap]
        p0, p1 = _from565(e0), _from565(e1)
        pal = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1)  # (n, 4, 3)
        err = ((blk[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)  # (n, 16, 4)
        idx = err.argmin(-1).astype(np.uint32)
        idx[e0 == e1] = 0
        bits = (idx << (2 * np.arange(16, dtype=np.uint32))).sum(1).astype(np.uint32)
        blocks = np.zeros(len(blk), dtype=[("e0", "<u2"), ("e1", "<u2"), ("i", "<u4")])
        blocks["e0"], blocks["e1"], blocks["i"] = e0, e1, bits
        out.append(blocks.tobytes())
    return b"".join(out)


def save_bc1(path, rgb):
    """BC1 .dds with the full mip chain (box-filtered, down to 4 x 4)."""
    img = Image.fromarray(np.asarray(rgb, np.uint8))
    w, h = img.size
    levels = []
    while True:
        levels.append(encode_bc1(np.asarray(img)))
        if w <= 4 or h <= 4:
            break
        w, h = w // 2, h // 2
        img = img.resize((w, h), Image.BOX)
    W, H = Image.fromarray(np.asarray(rgb, np.uint8)).size
    # DDSD_CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT|LINEARSIZE; DDPF_FOURCC; CAPS COMPLEX|TEXTURE|MIPMAP
    hdr = struct.pack("<4s7I44x2I4s5I4I4x", b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000,
                      H, W, len(levels[0]), 0, len(levels),
                      32, 0x4, b"DXT1", 0, 0, 0, 0, 0,
                      0x8 | 0x1000 | 0x400000, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr)
        for lv in levels:
            f.write(lv)
