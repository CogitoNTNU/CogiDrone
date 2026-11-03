#!/usr/bin/env python3
"""Background for the flight viewer: a height grid + a colour texture around a run.

    python3 tools/flight_viewer/terrain.py [run_dir] [--margin 150] [--force]

Writes <run_dir>/terrain/: terrain.json (extent, grid size, source), height.bin (float32
little-endian, row 0 = north, local ENU "u" metres) and color.jpg (north up). Results are also
cached in ~/.cache/cogidrone_flight_viewer/ by area, so re-viewing or re-flying the same area
is instant. view.py runs this automatically when terrain/ is missing.

Sources:
  sim, Bymarka worlds  the cached Kartverket 1 m DTM the world was built from + the baked
                       terrain textures (sim/models/bymarka_terrain/materials/bake), offline.
  kartverket (Norway)  Kartverket WCS DTM + Norge i bilder ortophoto WMS (needs an authorised
                       network; set $COGI_ORTHO_WMS to another WMS 'url|layer'), falls back to
                       the Kartverket topo map. Internet; cached afterwards.
  global / osm         anywhere: AWS Terrain Tiles heights (terrarium PNG, open, no key) + Esri
                       World Imagery (global) or OpenStreetMap tiles. Internet; tiles cached.
  flat                 offline / no position: a plane at the takeoff ground height with a
                       metric grid (10 m lines, 50 m lines labelled).
  --background auto    Bymarka sim -> Kartverket (inside Norway) -> global -> flat, first that works.
  --anchor LAT,LON     logs without lat/lon (no GPS, VIO): put local (0, 0) there.
"""

import argparse
import csv
import hashlib
import json
import math
import os
import re
import shutil
import sys
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image, ImageDraw

Image.MAX_IMAGE_PIXELS = None
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
BYMARKA = os.path.join(REPO, "sim", "tools", "bymarka")
WORLDS = os.path.join(REPO, "sim", "worlds")
BAKE = os.path.join(REPO, "sim", "models", "bymarka_terrain", "materials", "bake")
CACHE = os.path.expanduser("~/.cache/cogidrone_flight_viewer")
GRID_MAX = 512  # height samples per side
TEX_MAX = 4096  # colour texture px per side
NIB_WMS = "https://wms.geonorge.no/skwms1/wms.nib|ortofoto"
sys.path.insert(0, HERE)
from runlog import latlon_to_ned, ned_to_latlon


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def world_origin(world):
    s = open(os.path.join(WORLDS, f"{world}.sdf")).read()
    get = lambda tag: float(re.search(rf"<{tag}>([^<]+)</{tag}>", s).group(1))
    return get("latitude_deg"), get("longitude_deg"), get("elevation")


def run_extent(run, meta, margin):
    """Local ENU (e0, n0, e1, n1) around everything in the run, + margin, squared up a bit."""
    pts = []
    for name, ke, kn in (("track", "e", "n"), ("frames", "cam_e", "cam_n"), ("sightings", "e", "n"),
                         ("persons", "e", "n")):
        for r in read_csv(os.path.join(run, f"{name}.csv")):
            try:
                pts.append((float(r[ke]), float(r[kn])))
            except (KeyError, ValueError):
                pass
    if not pts:
        t = meta.get("takeoff", {})
        pts = [(t.get("e", 0.0), t.get("n", 0.0))]
    a = np.array(pts)
    e0, n0 = a.min(0) - margin
    e1, n1 = a.max(0) + margin
    # at least 2 x margin, and not thinner than 1:2
    w, h = e1 - e0, n1 - n0
    s = max(w, h) / 2
    if w < s:
        e0, e1 = (e0 + e1) / 2 - s / 2, (e0 + e1) / 2 + s / 2
    if h < s:
        n0, n1 = (n0 + n1) / 2 - s / 2, (n0 + n1) / 2 + s / 2
    return [round(v / 10) * 10 for v in (e0, n0, e1, n1)]


class Mapping:
    """Local ENU (e, n) -> UTM 32N (E, N) as an affine map, A @ [e, n] + b."""

    def __init__(self, A, b):
        self.A, self.b = np.asarray(A, float), np.asarray(b, float)

    def __call__(self, e, n):
        e, n = np.asarray(e, float), np.asarray(n, float)
        return self.A[0, 0] * e + self.A[0, 1] * n + self.b[0], self.A[1, 0] * e + self.A[1, 1] * n + self.b[1]


def sim_mapping(meta, data):
    """Bymarka sim: the world is built on the UTM grid and Gazebo treats world x/y as east/north,
    so local ENU -> world is a shift (through lat/lon, like search_test's evaluation)."""
    lat0, lon0, _ = world_origin(meta["world"])
    o = meta["origin"]
    wn, we = latlon_to_ned(lat0, lon0, o["lat"], o["lon"])  # world origin in local NED
    oe, on = data.latlon_to_utm(lat0, lon0)
    return Mapping(np.eye(2), (oe - we, on - wn))


def real_mapping(meta, data, ext):
    """Real flight: true lat/lon -> UTM, fitted as affine over the area (UTM is ~linear at km)."""
    o = meta["origin"]
    src, dst = [], []
    for e in np.linspace(ext[0], ext[2], 5):
        for n in np.linspace(ext[1], ext[3], 5):
            lat, lon = ned_to_latlon(n, e, o["lat"], o["lon"])
            src.append((e, n, 1.0))
            dst.append(data.latlon_to_utm(lat, lon))
    M, *_ = np.linalg.lstsq(np.array(src), np.array(dst), rcond=None)
    return Mapping(M[:2].T, M[2])


def sample(grid, x0, y1, res, E, N):
    """Bilinear sample of a north-up raster (top-left corner x0, y1, pixel size res) at UTM points."""
    c = (E - x0) / res - 0.5
    r = (y1 - N) / res - 0.5
    h, w = grid.shape
    r0 = np.clip(np.floor(r).astype(int), 0, h - 2)
    c0 = np.clip(np.floor(c).astype(int), 0, w - 2)
    fr, fc = np.clip(r - r0, 0, 1), np.clip(c - c0, 0, 1)
    v = (grid[r0, c0] * (1 - fr) * (1 - fc) + grid[r0, c0 + 1] * (1 - fr) * fc
         + grid[r0 + 1, c0] * fr * (1 - fc) + grid[r0 + 1, c0 + 1] * fr * fc)
    inside = (r >= -0.5) & (r <= h - 0.5) & (c >= -0.5) & (c <= w - 0.5)
    return np.where(inside, v, np.nan)


def warp(img, img_x0, img_y1, img_res, mapping, ext, size):
    """Resample a north-up UTM image onto the local ENU texture grid (size = (w, h))."""
    e0, n0, e1, n1 = ext
    sx, sy = (e1 - e0) / size[0], (n1 - n0) / size[1]
    A, b = mapping.A, mapping.b
    # output pixel (x, y) -> e = e0 + x*sx, n = n1 - y*sy -> UTM -> image pixel
    # image px = (E - img_x0)/res, py = (img_y1 - N)/res
    a_ = (A[0, 0] * sx / img_res, -A[0, 1] * sy / img_res, (A[0, 0] * e0 + A[0, 1] * n1 + b[0] - img_x0) / img_res)
    d_ = (-A[1, 0] * sx / img_res, A[1, 1] * sy / img_res, (img_y1 - (A[1, 0] * e0 + A[1, 1] * n1 + b[1])) / img_res)
    return img.transform(size, Image.AFFINE, (*a_, *d_), resample=Image.BILINEAR, fillcolor=(96, 96, 96))


def utm_box(mapping, ext, pad=20):
    E, N = mapping(np.array([ext[0], ext[2], ext[0], ext[2]]), np.array([ext[1], ext[1], ext[3], ext[3]]))
    return (math.floor(E.min() - pad), math.floor(N.min() - pad), math.ceil(E.max() + pad), math.ceil(N.max() + pad))


def grid_points(ext, nx, ny):
    """Mesh vertices: corners included, row 0 = north."""
    e = np.linspace(ext[0], ext[2], nx)
    n = np.linspace(ext[3], ext[1], ny)
    return np.meshgrid(e, n)


def anchor_heights(Z, mapping, meta, dtm_sample):
    """DTM elevations -> local 'u': the DTM under the takeoff spot = the takeoff ground height."""
    t = meta.get("takeoff", {"e": 0.0, "n": 0.0})
    E, N = mapping(t["e"], t["n"])
    z_t = float(dtm_sample(np.array([E]), np.array([N]))[0])
    return Z - z_t + float(meta.get("ground_u", 0.0)), z_t


def bymarka_sim(meta, ext, nx, ny, tex):
    sys.path.insert(0, BYMARKA)
    import data
    mp = sim_mapping(meta, data)
    dtm = np.array(Image.open(os.path.join(data.CACHE, "dtm_562900_7032500_564900_7034500_1m.tif")), np.float32)
    x0, y1 = 562900.0, 7034500.0
    f = lambda E, N: sample(dtm, x0, y1, 1.0, E, N)
    ge, gn = grid_points(ext, nx, ny)
    Z = f(*mp(ge, gn))
    U, z_t = anchor_heights(Z, mp, meta, f)
    # colour: mosaic of the baked 250 m tiles (4096 px masters) at the texture's scale
    box = utm_box(mp, ext)
    core_x0, core_y1, size, tiles = 562900.0, 7034500.0, 250.0, 8
    s = tex[0] / (ext[2] - ext[0])  # px per m
    j0, j1 = int((box[0] - core_x0) // size), int((box[2] - core_x0) // size)
    i0, i1 = int((core_y1 - box[3]) // size), int((core_y1 - box[1]) // size)
    j0, i0, j1, i1 = max(0, j0), max(0, i0), min(tiles - 1, j1), min(tiles - 1, i1)
    tp = max(16, round(size * s))
    mosaic = Image.new("RGB", ((j1 - j0 + 1) * tp, (i1 - i0 + 1) * tp), (96, 96, 96))
    for i in range(i0, i1 + 1):
        for j in range(j0, j1 + 1):
            p = os.path.join(BAKE, f"tile_{i}_{j}_color.jpg")
            if os.path.exists(p):
                im = Image.open(p)
                im.draft("RGB", (tp, tp))  # fast JPEG downscale on decode
                mosaic.paste(im.convert("RGB").resize((tp, tp), Image.LANCZOS), ((j - j0) * tp, (i - i0) * tp))
    color = warp(mosaic, core_x0 + j0 * size, core_y1 - i0 * size, size / tp, mp, ext, tex)
    # Lakes are a separate water model in the sim (not in the bake): paint them like
    # make_world.build_masks finds them (topo map water colour, cached 1 m topo of the core).
    try:
        from scipy import ndimage
        topo = data.topo((562900, 7032500, 564900, 7034500), 2000)
        water = np.abs(topo - np.array([220, 242, 242])).sum(2) < 25
        water = ndimage.binary_opening(ndimage.binary_closing(water, iterations=3), iterations=2)
        lab, n = ndimage.label(water)
        sizes = ndimage.sum(water, lab, range(1, n + 1))
        water = np.isin(lab, np.nonzero(sizes > 150)[0] + 1)
        wm = Image.fromarray((ndimage.gaussian_filter(water.astype(np.float32), 1.0) * 255).astype(np.uint8))
        a = warp(wm.convert("RGB"), core_x0, core_y1, 1.0, mp, ext, tex).convert("L")
        a = np.asarray(a, np.float32)[..., None] / 255 * 0.85
        c = np.asarray(color, np.float32)
        color = Image.fromarray((c * (1 - a) + np.array([40, 70, 100], np.float32) * a).astype(np.uint8))
    except Exception as ex:  # no cached topo: lakes stay ground-coloured
        print(f"  lakes not painted: {ex}")
    return U, color, {"heights": "Kartverket DTM 1 m (sim world source)",
                      "texture": "baked sim terrain (bymarka_terrain)", "attribution": "© Kartverket"}


def wms_image(url, layer, box, px, fmt="image/jpeg"):
    w = px
    h = max(1, round(px * (box[3] - box[1]) / (box[2] - box[0])))
    q = urllib.parse.urlencode({
        "service": "WMS", "version": "1.3.0", "request": "GetMap", "layers": layer, "styles": "",
        "crs": "EPSG:25832", "bbox": ",".join(str(v) for v in box), "width": w, "height": h, "format": fmt,
    })
    req = urllib.request.Request(url + "?" + q, headers={"User-Agent": "cogidrone-flight-viewer/1.0"})
    r = urllib.request.urlopen(req, timeout=120)
    if not r.headers.get("content-type", "").startswith("image/"):
        raise RuntimeError(f"{url} {layer}: {r.read(300).decode('latin-1', 'replace').strip()[:200]}")
    import io
    return Image.open(io.BytesIO(r.read())).convert("RGB")


def real(meta, ext, nx, ny, tex):
    sys.path.insert(0, BYMARKA)
    import data
    mp = real_mapping(meta, data, ext)
    box = utm_box(mp, ext)
    res = max(1, math.ceil(max(box[2] - box[0], box[3] - box[1]) / 1000))
    dtm = data.height("dtm", box, res)
    dtm = np.where(np.isfinite(dtm), dtm, np.nanmedian(dtm))
    f = lambda E, N: sample(dtm, box[0], box[3], float(res), E, N)
    ge, gn = grid_points(ext, nx, ny)
    Z = f(*mp(ge, gn))
    U, z_t = anchor_heights(Z, mp, meta, f)
    px = min(4096, max(512, round((box[2] - box[0]) / 0.25)))
    info = {"heights": f"Kartverket DTM {res} m (WCS)", "attribution": "© Kartverket"}
    srcs = [os.environ.get("COGI_ORTHO_WMS", NIB_WMS), f"{data.TOPO_WMS}|topo"]
    img, err = None, []
    for s in srcs:
        url, layer = s.split("|")
        try:
            img = wms_image(url, layer, box, min(px, 4000), "image/jpeg" if "nib" in url else "image/png")
            info["texture"] = f"{layer} ({url})"
            break
        except Exception as ex:
            err.append(str(ex)[:160])
            print(f"  {layer}: not available ({str(ex)[:120]})")
    if img is None:
        raise RuntimeError("; ".join(err))
    color = warp(img, box[0], box[3], (box[2] - box[0]) / img.width, mp, ext, tex)
    return U, color, info


# --- global sources: web-mercator tiles (no key) ----------------------------------------------

TILES = {
    # AWS Terrain Tiles (Mapzen terrarium encoding, open data on AWS, no key): height =
    # R*256 + G + B/256 - 32768 m. Mixed sources (SRTM, ...): ~30 m in most of the world.
    "terrarium": ("https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png", 15,
                  "AWS Terrain Tiles (Mapzen terrarium; SRTM, GMTED, ...)"),
    "esri": ("https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}", 19,
             "Esri World Imagery (Esri, Maxar, Earthstar Geographics, GIS User Community)"),
    "osm": ("https://tile.openstreetmap.org/{z}/{x}/{y}.png", 18, "© OpenStreetMap contributors"),
}
TILE_UA = {"User-Agent": "cogidrone-flight-viewer/1.0 (post-flight viewer; cached)"}


def merc(lat, lon, z):
    """lat/lon -> global web-mercator pixel (256 px tiles) at zoom z."""
    n = 256.0 * 2 ** z
    lat = np.clip(np.asarray(lat, float), -85.05, 85.05)
    x = (np.asarray(lon, float) + 180.0) / 360.0 * n
    y = (1.0 - np.arcsinh(np.tan(np.radians(lat))) / math.pi) / 2.0 * n
    return x, y


def merc_mapping(meta, ext, z):
    """Local ENU (e, n) -> web-mercator pixel at zoom z, affine fit (conformal, fine at km)."""
    o = meta["origin"]
    src, dst = [], []
    for e in np.linspace(ext[0], ext[2], 5):
        for n in np.linspace(ext[1], ext[3], 5):
            lat, lon = ned_to_latlon(n, e, o["lat"], o["lon"])
            src.append((e, n, 1.0))
            dst.append([float(v) for v in merc(lat, lon, z)])
    M, *_ = np.linalg.lstsq(np.array(src), np.array(dst), rcond=None)
    return Mapping(M[:2].T, M[2])


def tile_mosaic(name, z, mp, ext, max_tiles):
    """Fetch (cached) the tiles covering ext at zoom z (lowered until <= max_tiles).
    -> (image, x0_px, y0_px, z, mapping) with the mosaic's top-left global pixel."""
    url, zmax, _ = TILES[name]
    while True:
        X, Y = mp(np.array([ext[0], ext[2], ext[0], ext[2]]), np.array([ext[1], ext[1], ext[3], ext[3]]))
        tx0, tx1 = int(X.min() // 256), int(X.max() // 256)
        ty0, ty1 = int(Y.min() // 256), int(Y.max() // 256)
        if (tx1 - tx0 + 1) * (ty1 - ty0 + 1) <= max_tiles or z <= 1:
            break
        z -= 1
        mp = Mapping(mp.A / 2, mp.b / 2)
    img = Image.new("RGB", ((tx1 - tx0 + 1) * 256, (ty1 - ty0 + 1) * 256), (96, 96, 96))
    got = 0
    for ty in range(ty0, ty1 + 1):
        for tx in range(tx0, tx1 + 1):
            cp = os.path.join(CACHE, "tiles", name, str(z), str(tx), f"{ty}.img")
            if not os.path.exists(cp):
                req = urllib.request.Request(url.format(z=z, x=tx, y=ty), headers=TILE_UA)
                try:
                    data = urllib.request.urlopen(req, timeout=30).read()
                except Exception as ex:
                    print(f"  {name} tile {z}/{tx}/{ty}: {str(ex)[:80]}")
                    continue
                os.makedirs(os.path.dirname(cp), exist_ok=True)
                with open(cp, "wb") as f:
                    f.write(data)
            try:
                img.paste(Image.open(cp).convert("RGB"), ((tx - tx0) * 256, (ty - ty0) * 256))
                got += 1
            except Exception:
                pass
    if not got:
        raise RuntimeError(f"no {name} tiles (offline?)")
    return img, tx0 * 256, ty0 * 256, z, mp


def zoom_for(meta, m_per_px, zmax):
    lat = meta["origin"]["lat"]
    z = math.log2(156543.03 * math.cos(math.radians(lat)) / max(0.05, m_per_px))
    return int(max(1, min(zmax, math.ceil(z))))


def global_tiles(meta, ext, nx, ny, tex, imagery="esri"):
    """Anywhere: AWS terrarium heights + Esri World Imagery (or OSM) colour, both no-key tiles."""
    # heights
    zh = zoom_for(meta, max(ext[2] - ext[0], ext[3] - ext[1]) / 256, TILES["terrarium"][1])
    img, X0, Y0, zh, mp = tile_mosaic("terrarium", zh, merc_mapping(meta, ext, zh), ext, 36)
    a = np.asarray(img, np.float32)
    dem = a[..., 0] * 256 + a[..., 1] + a[..., 2] / 256 - 32768
    f = lambda E, N: sample(dem, X0, -Y0, 1.0, E, -N)  # pixel grid as a north-up raster (N = -Y)

    def dem_at(e, n):
        X, Y = mp(e, n)
        return f(X, Y)

    ge, gn = grid_points(ext, nx, ny)
    Z = dem_at(ge, gn)
    t = meta.get("takeoff") or {"e": 0.0, "n": 0.0}
    z_t = float(dem_at(np.array([t["e"]]), np.array([t["n"]]))[0])
    U = Z - z_t + float(meta.get("ground_u", 0.0))
    mpp = 156543.03 * math.cos(math.radians(meta["origin"]["lat"])) / 2 ** zh
    info = {"heights": f"{TILES['terrarium'][2]}, zoom {zh} (~{mpp:.0f} m/px)"}
    # colour
    zc = zoom_for(meta, (ext[2] - ext[0]) / tex[0], TILES[imagery][1])
    cimg, cx0, cy0, zc, cmp = tile_mosaic(imagery, zc, merc_mapping(meta, ext, zc), ext, 144)
    e0, n0, e1, n1 = ext
    sx, sy = (e1 - e0) / tex[0], (n1 - n0) / tex[1]
    A, b = cmp.A, cmp.b
    co = (A[0, 0] * sx, -A[0, 1] * sy, A[0, 0] * e0 + A[0, 1] * n1 + b[0] - cx0,
          A[1, 0] * sx, -A[1, 1] * sy, A[1, 0] * e0 + A[1, 1] * n1 + b[1] - cy0)
    color = cimg.transform(tex, Image.AFFINE, co, resample=Image.BILINEAR, fillcolor=(96, 96, 96))
    cmpp = 156543.03 * math.cos(math.radians(meta["origin"]["lat"])) / 2 ** zc
    info.update({"texture": f"{TILES[imagery][2]}, zoom {zc} (~{cmpp:.1f} m/px)",
                 "attribution": f"Imagery: {TILES[imagery][2]} · Heights: AWS Terrain Tiles"})
    return U, color, info


def global_osm(meta, ext, nx, ny, tex):
    return global_tiles(meta, ext, nx, ny, tex, "osm")


# --- offline ----------------------------------------------------------------------------------


def flat(meta, ext, nx, ny, tex):
    """No map: a plane at the takeoff ground height (or, without one, just under the first track
    point) with a metric grid (10 m lines, 50 m lines labelled)."""
    g = meta.get("ground_u")
    U = np.full((ny, nx), float(g if g is not None else meta.get("_track_u0", 0.0)), np.float32)
    w, h = tex
    im = Image.new("RGB", (w, h), (118, 128, 112))
    d = ImageDraw.Draw(im)
    sx, sy = w / (ext[2] - ext[0]), h / (ext[3] - ext[1])
    for step, col, wd in ((10, (132, 142, 126), max(1, round(sx * 0.15))), (50, (168, 176, 158), max(2, round(sx * 0.35)))):
        for e in range(int(math.ceil(ext[0] / step) * step), int(ext[2]) + 1, step):
            x = (e - ext[0]) * sx
            d.line([(x, 0), (x, h)], fill=col, width=wd)
        for n in range(int(math.ceil(ext[1] / step) * step), int(ext[3]) + 1, step):
            y = (ext[3] - n) * sy
            d.line([(0, y), (w, y)], fill=col, width=wd)
    try:
        from PIL import ImageFont
        font = ImageFont.load_default(size=max(10, round(sx * 2.2)))
    except Exception:
        font = None
    for e in range(int(math.ceil(ext[0] / 50) * 50), int(ext[2]) + 1, 50):
        for n in range(int(math.ceil(ext[1] / 50) * 50), int(ext[3]) + 1, 50):
            d.text(((e - ext[0]) * sx + 4, (ext[3] - n) * sy + 2), f"{e:+d} E\n{n:+d} N", fill=(200, 206, 190), font=font)
    return U, im, {"heights": "flat (takeoff ground height)", "texture": "local metric grid (10 m / 50 m)",
                   "attribution": ""}


NORWAY = (57.5, 4.0, 71.5, 31.5)  # lat0, lon0, lat1, lon1 (Kartverket coverage, roughly)
SOURCES = ("auto", "sim", "kartverket", "global", "osm", "flat")


def chain(source, meta, world):
    """Sources to try, in order."""
    o = meta.get("origin") or {}
    has_ll = o.get("lat") is not None and o.get("lon") is not None
    sim_ok = (meta.get("source") == "sim" and world.startswith("bymarka") and has_ll
              and os.path.exists(os.path.join(WORLDS, f"{world}.sdf")))
    in_no = has_ll and NORWAY[0] <= o["lat"] <= NORWAY[2] and NORWAY[1] <= o["lon"] <= NORWAY[3]
    if source == "auto":
        c = (["bymarka"] if sim_ok else []) + (["kartverket"] if in_no and not sim_ok else []) \
            + (["global"] if has_ll and not sim_ok else [])
    else:
        c = [{"sim": "bymarka", "real": "kartverket"}.get(source, source)] if source != "flat" else []
        if not has_ll and c:
            print(f"terrain: {source} needs a position (no lat/lon in meta.origin; use --anchor LAT,LON)")
            c = []
    return c + ["flat"]


def build(run, margin=150.0, force=False, quiet=False, source="auto", anchor=None, out_name="terrain"):
    """Build <run>/<out_name>/. source: auto | sim | kartverket | global | osm | flat.
    anchor (lat, lon[, alt]): where local (0, 0) is when the log has no lat/lon (local-only
    position, e.g. no GPS / VIO) - or to override it. Returns (out dir, terrain.json dict)."""
    out = os.path.join(run, out_name)
    meta = json.load(open(os.path.join(run, "meta.json")))
    if anchor is None and meta.get("anchor"):
        a = meta["anchor"]
        anchor = (a["lat"], a["lon"], a.get("alt", 0.0))
    anchored = False
    if anchor is not None:
        anchor = tuple(float(v) for v in anchor) + ((0.0,) if len(anchor) == 2 else ())
        meta["origin"] = {"lat": anchor[0], "lon": anchor[1], "alt": anchor[2]}
        anchored = True
    tr = read_csv(os.path.join(run, "track.csv"))
    if tr and tr[0].get("u"):
        meta["_track_u0"] = float(tr[0]["u"]) - 0.25
    ext = run_extent(run, meta, margin)
    world = meta.get("world", "")
    o = meta.get("origin") or {}
    kinds = chain(source, meta, world)
    key = json.dumps([6, kinds, world, ext, round(o.get("lat") or 0, 6), round(o.get("lon") or 0, 6),
                      round(float(meta.get("ground_u") or 0), 2), meta.get("takeoff")], sort_keys=True)
    h = hashlib.sha1(key.encode()).hexdigest()[:16]
    cached = os.path.join(CACHE, h)
    tj_path = os.path.join(out, "terrain.json")
    if not force and os.path.exists(tj_path):
        tj = json.load(open(tj_path))
        if tj.get("key") == h:
            return out, tj
    if not force and os.path.exists(os.path.join(cached, "terrain.json")):
        shutil.copytree(cached, out, dirs_exist_ok=True)
        return out, json.load(open(tj_path))
    w, hh = ext[2] - ext[0], ext[3] - ext[1]
    res = max(1.0, max(w, hh) / (GRID_MAX - 1))
    nx, ny = int(round(w / res)) + 1, int(round(hh / res)) + 1
    tex_px = min(TEX_MAX, int(2 ** math.ceil(math.log2(max(w, hh) / 0.15))))  # ~15 cm/px, power of 2
    tex = (tex_px, max(64, round(tex_px * hh / w))) if w >= hh else (max(64, round(tex_px * w / hh)), tex_px)
    fns = {"bymarka": bymarka_sim, "kartverket": real, "global": global_tiles, "osm": global_osm, "flat": flat}
    errors = []
    for kind in kinds:
        if not quiet:
            print(f"terrain: {kind}, extent e {ext[0]}..{ext[2]} n {ext[1]}..{ext[3]} m, grid {nx}x{ny}, "
                  f"texture {tex[0]}x{tex[1]}")
        try:
            U, color, info = fns[kind](meta, ext, nx, ny, tex)
            break
        except Exception as ex:
            errors.append(f"{kind}: {str(ex)[:160]}")
            print(f"terrain: {kind} failed ({str(ex)[:160]})")
    U = np.where(np.isfinite(U), U, np.nanmedian(U) if np.isfinite(U).any() else 0.0).astype("<f4")
    os.makedirs(cached, exist_ok=True)
    U.tofile(os.path.join(cached, "height.bin"))
    color.save(os.path.join(cached, "color.jpg"), quality=88)
    tj = {"key": h, "kind": kind, "source": source, "tried": kinds, "errors": errors, "extent": ext,
          "nx": nx, "ny": ny, "res_m": res, "u_min": float(U.min()), "u_max": float(U.max()),
          "tex_size": list(color.size), "origin": meta.get("origin"), "anchored": anchored, **info}
    json.dump(tj, open(os.path.join(cached, "terrain.json"), "w"), indent=1)
    if os.path.isdir(out):
        shutil.rmtree(out)
    shutil.copytree(cached, out)
    return out, tj


def parse_anchor(s):
    if not s:
        return None
    v = [float(x) for x in s.split(",")]
    if len(v) not in (2, 3):
        raise ValueError("--anchor LAT,LON[,ALT]")
    return tuple(v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", nargs="?", default=os.path.expanduser("~/flight_runs/latest"))
    ap.add_argument("--margin", type=float, default=150.0, help="m around the flown area")
    ap.add_argument("--force", action="store_true", help="rebuild even if cached")
    ap.add_argument("--background", default="auto", choices=SOURCES, help="map source (default auto)")
    ap.add_argument("--anchor", default="", help="LAT,LON[,ALT] of local (0, 0) for logs without lat/lon")
    a = ap.parse_args()
    out, tj = build(a.run, a.margin, a.force, source=a.background, anchor=parse_anchor(a.anchor))
    print(out, tj["kind"], tj.get("texture", ""))


if __name__ == "__main__":
    main()
