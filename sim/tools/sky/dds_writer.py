"""Minimal DDS cubemap writer (uncompressed RGBA8), matching the layout
Gazebo/Ogre2 expects for its built-in skybox.dds (verified against the
stock file: DDSD_CAPS|HEIGHT|WIDTH|PIXELFORMAT, caps=COMPLEX|TEXTURE,
caps2=CUBEMAP|ALLFACES=0xFE00, mipmapcount=1).

Face order written: slot0..slot5, i.e. the six D3D/Ogre cubemap "+X,-X,+Y,
-Y,+Z,-Z" texture slots, in that order. NOTE: which *world* direction each
slot shows was determined empirically for Gazebo Harmonic's ogre2 skybox
(see ../ATTRIBUTION.md / README.md in sim/assets/sky and the calibration
notes in this tool's README) -- it is NOT the naive assumption that slot
"+Z" is world up. make_sky.py already accounts for this; this writer just
serializes 6 pre-built face images in slot order.
"""
import struct
import numpy as np

DDSD_CAPS = 0x1
DDSD_HEIGHT = 0x2
DDSD_WIDTH = 0x4
DDSD_PIXELFORMAT = 0x1000
DDPF_RGB = 0x40
DDPF_ALPHAPIXELS = 0x1
DDSCAPS_COMPLEX = 0x8
DDSCAPS_TEXTURE = 0x1000
DDSCAPS2_CUBEMAP = 0x200
DDSCAPS2_CUBEMAP_ALLFACES = 0xFC00


def write_cubemap_dds(path, faces_rgba_uint8):
    """faces_rgba_uint8: list of 6 numpy arrays (N,N,4) uint8, in D3D/Ogre
    cubemap slot order [slot0=+X, slot1=-X, slot2=+Y, slot3=-Y, slot4=+Z,
    slot5=-Z] (see module docstring for what world direction each slot
    actually shows in Gazebo's ogre2 skybox)."""
    assert len(faces_rgba_uint8) == 6
    n = faces_rgba_uint8[0].shape[0]
    for f in faces_rgba_uint8:
        assert f.shape == (n, n, 4), f.shape
        assert f.dtype == np.uint8

    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
    pitch = n * 4  # bytes per scanline, uncompressed 32bpp
    header = b"DDS "
    header += struct.pack("<I", 124)  # dwSize
    header += struct.pack("<I", flags)
    header += struct.pack("<I", n)  # height
    header += struct.pack("<I", n)  # width
    header += struct.pack("<I", pitch)  # pitchOrLinearSize
    header += struct.pack("<I", 0)  # depth
    header += struct.pack("<I", 1)  # mipMapCount
    header += b"\x00" * 44  # reserved1[11]
    # DDS_PIXELFORMAT (32 bytes)
    pf = struct.pack("<I", 32)  # dwSize
    pf += struct.pack("<I", DDPF_RGB | DDPF_ALPHAPIXELS)  # dwFlags
    pf += struct.pack("<I", 0)  # dwFourCC
    pf += struct.pack("<I", 32)  # dwRGBBitCount
    pf += struct.pack("<I", 0x00FF0000)  # dwRBitMask
    pf += struct.pack("<I", 0x0000FF00)  # dwGBitMask
    pf += struct.pack("<I", 0x000000FF)  # dwBBitMask
    pf += struct.pack("<I", 0xFF000000)  # dwABitMask
    assert len(pf) == 32
    header += pf
    caps = DDSCAPS_COMPLEX | DDSCAPS_TEXTURE
    caps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES
    header += struct.pack("<I", caps)
    header += struct.pack("<I", caps2)
    header += struct.pack("<I", 0)  # caps3
    header += struct.pack("<I", 0)  # caps4
    header += struct.pack("<I", 0)  # reserved2
    assert len(header) == 128, len(header)

    with open(path, "wb") as fh:
        fh.write(header)
        for f in faces_rgba_uint8:
            # BGRA byte order on disk to match dwRBitMask/G/B/A above
            bgra = f[:, :, [2, 1, 0, 3]].copy()
            fh.write(bgra.tobytes())
