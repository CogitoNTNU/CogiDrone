#!/usr/bin/env python3
"""Build a photographic Nordic-summer sky cubemap for Gazebo Harmonic (ogre2).

Downloads a CC0 Poly Haven HDRI (clear/partly-cloudy daytime sky), tone-maps
it to LDR sRGB, bakes it into a 6-face DDS cubemap in the exact layout
Gazebo's built-in ogre2 skybox.dds uses, and prints the sun direction/colour
to drop into a world's <light type="directional"> so shadows match the sky.

Usage:
    python3 make_sky.py [--faces N] [--out sim/assets/sky] [--cache DIR]

Requires: numpy, pillow (both already present in system python3 / the
project's simtools-venv). No other dependencies -- the Radiance .hdr reader
and the DDS cubemap writer are both hand-rolled (see radiance_hdr.py,
dds_writer.py) so this script has no exotic imports.

--- How the cubemap face mapping was determined -------------------------
Gazebo Harmonic's ogre2 skybox.dds is a standard D3D/Ogre cubemap (6 faces
in the fixed slot order +X,-X,+Y,-Y,+Z,-Z, caps2 = CUBEMAP|ALLFACES). But
empirically (see sim/tools/sky/README.md for the calibration method and
renders) the *world* direction shown by each slot, for Gazebo's ENU world
(x=east, y=north, z=up), is NOT the naive "slot +X = world +X" assumption:

  slot0 (+X slot) -> world EAST  (+X)   : dir = normalize( 1, -ny,  nx)
  slot1 (-X slot) -> world WEST  (-X)   : dir = normalize(-1, -ny, -nx)
  slot2 (+Y slot) -> world NORTH (+Y)   : dir = normalize( nx,  1, -ny)
  slot3 (-Y slot) -> world SOUTH (-Y)   : dir = normalize( nx, -1,  ny)
  slot4 (+Z slot) -> world DOWN  (-Z)   : dir = normalize( nx, -ny, -1)
  slot5 (-Z slot) -> world UP    (+Z)   : dir = normalize(-nx, -ny,  1)

where nx,ny in [-1,1] are the face-local texture column/row coordinates
(nx = 2*(col+0.5)/N-1, increasing left->right; ny = 2*(row+0.5)/N-1,
increasing top->bottom), i.e. the horizontal (+/-Z) slots are inverted
relative to the naive guess, and the X/Y slots carry an extra in-plane
rotation. This was confirmed by rendering a 6-colour labelled calibration
cubemap through `gz sim -s --headless-rendering` with static cameras
looking E/N/W/S/up and reading back which coloured/quadrant-marked face
appeared in which direction (3 independent markers per face, multiple
FOVs). See README.md for the full procedure/evidence.
---------------------------------------------------------------------------
"""
import argparse
import math
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from radiance_hdr import read_hdr
from dds_writer import write_cubemap_dds

HDRI_ID = "kloofendal_48d_partly_cloudy_puresky"
HDRI_RES = "2k"
HDRI_URL = f"https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/{HDRI_RES}/{HDRI_ID}_{HDRI_RES}.hdr"
HDRI_MD5 = "2eba3a4d7eeb23cbfbeca364c97e7980"

DEFAULT_CACHE = os.path.expanduser("~/cogidrone_deps/simassets-cache/sky")


def download_hdri(cache_dir):
    os.makedirs(cache_dir, exist_ok=True)
    dest = os.path.join(cache_dir, f"{HDRI_ID}_{HDRI_RES}.hdr")
    if os.path.exists(dest):
        print(f"[make_sky] using cached {dest}")
        return dest
    print(f"[make_sky] downloading {HDRI_URL}")
    urllib.request.urlretrieve(HDRI_URL, dest)
    print(f"[make_sky] downloaded {os.path.getsize(dest)} bytes -> {dest}")
    return dest


# --- tonemap: exposure + ACES filmic (Narkowicz fit) + sRGB OETF ---------
def srgb_encode(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def aces_filmic(x):
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    return np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)


def tonemap(rgb_linear, exposure=1.1):
    return srgb_encode(aces_filmic(rgb_linear * exposure))


# --- equirect sampling: row=0 is zenith (+Z), azimuth phi=atan2(y,x) -----
def equirect_bilinear_sample(img, dirs):
    """img: (H,W,3) float32. dirs: (...,3) unit vectors in world ENU
    (x=east,y=north,z=up). Returns (...,3) sampled colour (bilinear,
    wrapping in azimuth, clamped at poles)."""
    H, W, _ = img.shape
    x, y, z = dirs[..., 0], dirs[..., 1], dirs[..., 2]
    z = np.clip(z, -1.0, 1.0)
    theta = np.arccos(z)                      # 0 at +Z (zenith) .. pi at -Z (nadir)
    phi = np.arctan2(y, x)                     # -pi..pi
    phi = np.mod(phi, 2 * np.pi)                # 0..2pi

    fr = theta / np.pi * (H - 1)
    fc = phi / (2 * np.pi) * W

    r0 = np.floor(fr).astype(np.int64)
    c0 = np.floor(fc).astype(np.int64)
    r1 = np.clip(r0 + 1, 0, H - 1)
    c1 = np.mod(c0 + 1, W)
    r0 = np.clip(r0, 0, H - 1)
    c0 = np.mod(c0, W)

    tr = (fr - r0)[..., None]
    tc = (fc - c0)[..., None]

    c00 = img[r0, c0]
    c01 = img[r0, c1]
    c10 = img[r1, c0]
    c11 = img[r1, c1]
    top = c00 * (1 - tc) + c01 * tc
    bot = c10 * (1 - tc) + c11 * tc
    return top * (1 - tr) + bot * tr


# --- per-slot direction formulas (see module docstring) ------------------
def _nxny(n):
    idx = np.arange(n, dtype=np.float32)
    coord = 2 * (idx + 0.5) / n - 1.0
    ny, nx = np.meshgrid(coord, coord, indexing="ij")  # ny=row-based, nx=col-based
    return nx, ny


def face_directions(n):
    nx, ny = _nxny(n)
    ones = np.ones_like(nx)
    faces = [
        np.stack([ones, -ny, nx], axis=-1),    # slot0 +X slot -> world EAST
        np.stack([-ones, -ny, -nx], axis=-1),  # slot1 -X slot -> world WEST
        np.stack([nx, ones, -ny], axis=-1),    # slot2 +Y slot -> world NORTH
        np.stack([nx, -ones, ny], axis=-1),    # slot3 -Y slot -> world SOUTH
        np.stack([nx, -ny, -ones], axis=-1),   # slot4 +Z slot -> world DOWN
        np.stack([-nx, -ny, ones], axis=-1),   # slot5 -Z slot -> world UP
    ]
    return [d / np.linalg.norm(d, axis=-1, keepdims=True) for d in faces]


def build_cubemap(equirect_linear, n, exposure):
    faces_dirs = face_directions(n)
    out = []
    for i, dirs in enumerate(faces_dirs):
        linear = equirect_bilinear_sample(equirect_linear, dirs)
        ldr = tonemap(linear, exposure=exposure)
        rgba = np.zeros((n, n, 4), dtype=np.uint8)
        rgba[:, :, :3] = (ldr * 255.0 + 0.5).astype(np.uint8)
        rgba[:, :, 3] = 255
        out.append(rgba)
        print(f"[make_sky] baked face slot{i} ({n}x{n})")
    return out


# --- sun direction / colour -----------------------------------------------
def find_sun(equirect_linear):
    lum = 0.2126 * equirect_linear[:, :, 0] + 0.7152 * equirect_linear[:, :, 1] + 0.0722 * equirect_linear[:, :, 2]
    row, col = np.unravel_index(np.argmax(lum), lum.shape)
    H, W = lum.shape
    # average a small patch around the peak to denoise the sun colour/direction
    r0, r1 = max(0, row - 3), min(H, row + 4)
    c0, c1 = max(0, col - 3), min(W, col + 4)
    patch = equirect_linear[r0:r1, c0:c1].reshape(-1, 3)
    patch_lum = 0.2126 * patch[:, 0] + 0.7152 * patch[:, 1] + 0.0722 * patch[:, 2]
    keep = patch_lum > patch_lum.max() * 0.5
    sun_color_linear = patch[keep].mean(axis=0)

    theta = row / (H - 1) * math.pi
    phi = col / W * 2 * math.pi
    x = math.sin(theta) * math.cos(phi)
    y = math.sin(theta) * math.sin(phi)
    z = math.cos(theta)
    sun_dir_enu = np.array([x, y, z], dtype=np.float64)
    sun_dir_enu /= np.linalg.norm(sun_dir_enu)

    elevation_deg = math.degrees(math.asin(np.clip(sun_dir_enu[2], -1, 1)))
    azimuth_deg = math.degrees(math.atan2(sun_dir_enu[0], sun_dir_enu[1])) % 360  # 0=N,90=E

    return sun_dir_enu, sun_color_linear, elevation_deg, azimuth_deg


def sun_light_snippet(sun_dir_enu, sun_color_linear):
    light_dir = -sun_dir_enu
    light_dir /= np.linalg.norm(light_dir)
    c = sun_color_linear / sun_color_linear.max()
    # warm it slightly and keep in a sane SDF diffuse range
    diffuse = np.clip(c, 0.5, 1.0)
    return light_dir, diffuse


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--faces", type=int, default=1024, help="cube face resolution (px)")
    ap.add_argument("--exposure", type=float, default=1.1)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "sky"))
    ap.add_argument("--cache", default=DEFAULT_CACHE)
    ap.add_argument("--media-root", default=None,
                     help="if set, also write a full ogre2 media overlay tree here "
                          "(stock media + our skybox.dds) for GZ_RENDERING_RESOURCE_PATH testing")
    args = ap.parse_args()

    hdr_path = download_hdri(args.cache)
    print("[make_sky] decoding HDR...")
    rgb = read_hdr(hdr_path)
    print(f"[make_sky] HDR shape {rgb.shape}")

    sun_dir_enu, sun_color_linear, elev, az = find_sun(rgb)
    light_dir, diffuse = sun_light_snippet(sun_dir_enu, sun_color_linear)
    print(f"[make_sky] sun: elevation={elev:.1f} deg azimuth={az:.1f} deg (0=N,90=E)")
    print(f"[make_sky] sun_dir_enu={sun_dir_enu} light_direction={light_dir} diffuse={diffuse}")

    faces = build_cubemap(rgb, args.faces, args.exposure)

    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)
    dds_path = os.path.join(out_dir, "skybox.dds")
    write_cubemap_dds(dds_path, faces)
    size_mb = os.path.getsize(dds_path) / 1e6
    print(f"[make_sky] wrote {dds_path} ({size_mb:.1f} MB)")

    # also dump a small equirect preview (not shipped, just for eyeballing)
    preview = (tonemap(rgb, exposure=args.exposure) * 255).astype(np.uint8)
    prev_path = os.path.join(out_dir, "_preview_equirect.png")
    Image.fromarray(preview).save(prev_path)
    print(f"[make_sky] wrote preview {prev_path}")

    snippet_path = os.path.join(out_dir, "sun_light.sdf.xml")
    with open(snippet_path, "w") as f:
        f.write(f"""<!-- Auto-generated by sim/tools/sky/make_sky.py from {HDRI_ID} -->
<light type="directional" name="sun">
  <cast_shadows>true</cast_shadows>
  <pose>0 0 50 0 0 0</pose>
  <diffuse>{diffuse[0]:.3f} {diffuse[1]:.3f} {diffuse[2]:.3f} 1</diffuse>
  <specular>0.3 0.3 0.3 1</specular>
  <attenuation>
    <range>5000</range>
    <constant>1.0</constant>
    <linear>0</linear>
    <quadratic>0</quadratic>
  </attenuation>
  <direction>{light_dir[0]:.4f} {light_dir[1]:.4f} {light_dir[2]:.4f}</direction>
</light>
<!-- sun elevation={elev:.1f}deg azimuth={az:.1f}deg(0=N,90=E) in the HDRI's own frame -->
""")
    print(f"[make_sky] wrote {snippet_path}")

    if args.media_root:
        import shutil
        stock = "/usr/share/gz/gz-rendering8/ogre2"
        dst_ogre2 = os.path.join(args.media_root, "ogre2")
        if not os.path.exists(dst_ogre2):
            shutil.copytree(stock, dst_ogre2)
        shutil.copy(dds_path, os.path.join(dst_ogre2, "media", "materials", "textures", "skybox.dds"))
        print(f"[make_sky] updated media overlay at {args.media_root}")

    print("[make_sky] done.")


if __name__ == "__main__":
    main()
