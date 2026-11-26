"""Minimal reader for Radiance .hdr (RGBE) images, new-style RLE scanlines
(the format Poly Haven's .hdr downloads use). No external deps beyond numpy.
"""
import numpy as np


def read_hdr(path):
    with open(path, "rb") as f:
        data = f.read()

    # --- header ---
    pos = 0
    line_end = data.index(b"\n", pos)
    magic = data[pos:line_end]
    assert magic.startswith(b"#?"), magic
    pos = line_end + 1
    while True:
        line_end = data.index(b"\n", pos)
        line = data[pos:line_end]
        pos = line_end + 1
        if line == b"":
            break  # blank line ends header
    # resolution line, e.g. "-Y 1024 +X 2048"
    line_end = data.index(b"\n", pos)
    resline = data[pos:line_end].decode("ascii")
    pos = line_end + 1
    parts = resline.split()
    assert parts[0] == "-Y" and parts[2] == "+X", f"unsupported orientation: {resline}"
    H = int(parts[1]); W = int(parts[3])

    out = np.zeros((H, W, 4), dtype=np.uint8)  # RGBE bytes
    for row in range(H):
        # scanline header: 2,2,hi,lo  (new-style RLE), else old-style flat/RLE
        b0, b1, b2, b3 = data[pos], data[pos + 1], data[pos + 2], data[pos + 3]
        if b0 == 2 and b1 == 2 and ((b2 << 8) | b3) == W and 8 <= W < 0x8000:
            pos += 4
            channels = [np.zeros(W, dtype=np.uint8) for _ in range(4)]
            for c in range(4):
                x = 0
                while x < W:
                    n = data[pos]; pos += 1
                    if n > 128:
                        # run of (n-128) identical bytes
                        n -= 128
                        val = data[pos]; pos += 1
                        channels[c][x:x + n] = val
                        x += n
                    else:
                        # literal n bytes
                        channels[c][x:x + n] = np.frombuffer(data, dtype=np.uint8, count=n, offset=pos)
                        pos += n
                        x += n
            out[row, :, 0] = channels[0]
            out[row, :, 1] = channels[1]
            out[row, :, 2] = channels[2]
            out[row, :, 3] = channels[3]
        else:
            # old-style: flat RGBE or run-length per-pixel (rare for modern exports)
            x = 0
            while x < W:
                r, g, b, e = data[pos], data[pos + 1], data[pos + 2], data[pos + 3]
                pos += 4
                if r == 1 and g == 1 and b == 1:
                    n = e
                    prev = out[row, x - 1]
                    out[row, x:x + n] = prev
                    x += n
                else:
                    out[row, x] = (r, g, b, e)
                    x += 1

    # RGBE -> float32 linear radiance
    e = out[:, :, 3].astype(np.int32)
    scale = np.where(e > 0, 2.0 ** (e - 128 - 8), 0.0).astype(np.float32)
    rgb = out[:, :, :3].astype(np.float32) * scale[:, :, None]
    return rgb  # (H, W, 3) float32 linear radiance
