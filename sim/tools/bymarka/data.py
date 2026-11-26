"""Download and cache the open map data for the Bymarka world.

All sources are open data (see sim/ATTRIBUTION.md):
    Kartverket DTM/DOM 1 m (terrain / surface incl. trees)   CC BY 4.0
    Kartverket topographic map (WMS, used for lakes and bogs) CC BY 4.0
    NIBIO SR16 forest resource map (tree species, height)    NLOD
    OpenStreetMap (paths, roads, streams, buildings)          ODbL

Everything is cached in ~/cogidrone_deps/simassets-cache/bymarka (or $COGI_ASSET_CACHE), so
re-running the world generator does not download again.
"""

import json
import math
import os
import urllib.parse
import urllib.request

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

CACHE = os.path.join(
    os.environ.get("COGI_ASSET_CACHE", os.path.expanduser("~/cogidrone_deps/simassets-cache")), "bymarka"
)
WCS = "https://wcs.geonorge.no/skwms1/wcs.hoyde-{kind}-nhm-25832"
TOPO_WMS = "https://wms.geonorge.no/skwms1/wms.topo"
SR16_WMS = "https://wms.nibio.no/cgi-bin/sr16"
OVERPASS = ("https://overpass-api.de/api/interpreter", "https://overpass.kumi.systems/api/interpreter",
            "https://overpass.private.coffee/api/interpreter")
UA = {"User-Agent": "cogidrone-sim/1.0 (NTNU student drone project)"}


def _get(url, path, timeout=300):
    path = os.path.join(CACHE, path)
    if not os.path.exists(path):
        os.makedirs(CACHE, exist_ok=True)
        print(f"  downloading {os.path.basename(path)}")
        req = urllib.request.Request(url, headers=UA)
        data = urllib.request.urlopen(req, timeout=timeout).read()
        with open(path + ".part", "wb") as f:
            f.write(data)
        os.replace(path + ".part", path)
    return path


def _bbox(b):
    return ",".join(str(int(v)) for v in b)


def height(kind, bbox, res):
    """Kartverket height model ('dtm' = bare ground, 'dom' = surface incl. trees/buildings) as a
    float32 array, row 0 = north. bbox = (xmin, ymin, xmax, ymax) in EPSG:25832, res in m."""
    w, h = int((bbox[2] - bbox[0]) / res), int((bbox[3] - bbox[1]) / res)
    cov = {"dtm": "NHM_DTM_25832", "dom": "NHM_DOM_25832"}[kind]
    q = urllib.parse.urlencode({
        "service": "WCS", "version": "1.0.0", "request": "GetCoverage", "coverage": cov,
        "crs": "EPSG:25832", "bbox": _bbox(bbox), "width": w, "height": h, "format": "GeoTIFF",
    })
    p = _get(WCS.format(kind=kind) + "?" + q, f"{kind}_{_bbox(bbox).replace(',', '_')}_{res}m.tif")
    a = np.array(Image.open(p), np.float32)
    # Nodata (sea, outside Norway) comes as NaN or huge values; resampled tiles also get junk
    # like 99975 m along the coast. Norway's highest point is 2469 m.
    a[~((a > -20) & (a < 2500))] = np.nan
    return a


def wms(url, layer, bbox, px, name, version="1.3.0"):
    crs_key = "crs" if version == "1.3.0" else "srs"
    q = urllib.parse.urlencode({
        "service": "WMS", "version": version, "request": "GetMap", "layers": layer, "styles": "",
        crs_key: "EPSG:25832", "bbox": _bbox(bbox), "width": px, "height": px, "format": "image/png",
    })
    return np.asarray(Image.open(_get(url + "?" + q, name)).convert("RGB")).astype(np.int16)


def topo(bbox, px):
    return wms(TOPO_WMS, "topo", bbox, px, f"topo_{_bbox(bbox).replace(',', '_')}_{px}.png")


def sr16(layer, bbox, px):
    return wms(SR16_WMS, layer, bbox, px, f"sr16_{layer}_{_bbox(bbox).replace(',', '_')}_{px}.png", "1.1.1")


# --- UTM zone 32N (EPSG:25832) <-> WGS84, Krüger series (mm accuracy) -------------------------
_A, _F = 6378137.0, 1 / 298.257222101
_K0, _E0, _LON0 = 0.9996, 500000.0, math.radians(9.0)
_N = _F / (2 - _F)
_AA = _A / (1 + _N) * (1 + _N**2 / 4 + _N**4 / 64)
_ALPHA = (_N / 2 - 2 * _N**2 / 3 + 5 * _N**3 / 16, 13 * _N**2 / 48 - 3 * _N**3 / 5, 61 * _N**3 / 240)
_BETA = (_N / 2 - 2 * _N**2 / 3 + 37 * _N**3 / 96, _N**2 / 48 + _N**3 / 15, 17 * _N**3 / 480)
_DELTA = (2 * _N - 2 * _N**2 / 3 - 2 * _N**3, 7 * _N**2 / 3 - 8 * _N**3 / 5, 56 * _N**3 / 15)


def utm_to_latlon(e, n):
    xi, eta = n / (_K0 * _AA), (e - _E0) / (_K0 * _AA)
    xp, ep = xi, eta
    for j, b in enumerate(_BETA, 1):
        xp -= b * math.sin(2 * j * xi) * math.cosh(2 * j * eta)
        ep -= b * math.cos(2 * j * xi) * math.sinh(2 * j * eta)
    chi = math.asin(math.sin(xp) / math.cosh(ep))
    lat = chi + sum(d * math.sin(2 * j * chi) for j, d in enumerate(_DELTA, 1))
    lon = _LON0 + math.atan2(math.sinh(ep), math.cos(xp))
    return math.degrees(lat), math.degrees(lon)


def latlon_to_utm(lat, lon):
    lat, lon = math.radians(lat), math.radians(lon)
    ecc = 2 * math.sqrt(_N) / (1 + _N)
    t = math.sinh(math.atanh(math.sin(lat)) - ecc * math.atanh(ecc * math.sin(lat)))
    xp, ep = math.atan2(t, math.cos(lon - _LON0)), math.atanh(math.sin(lon - _LON0) / math.sqrt(1 + t * t))
    xi, eta = xp, ep
    for j, a in enumerate(_ALPHA, 1):
        xi += a * math.sin(2 * j * xp) * math.cosh(2 * j * ep)
        eta += a * math.cos(2 * j * xp) * math.sinh(2 * j * ep)
    return _E0 + _K0 * _AA * eta, _K0 * _AA * xi


def osm(bbox):
    """OSM ways inside bbox (EPSG:25832) as a list of {tags, pts: [(e, n), ...]}."""
    s, w = utm_to_latlon(bbox[0], bbox[1])
    n, e = utm_to_latlon(bbox[2], bbox[3])
    s, w, n, e = s - 0.003, w - 0.006, n + 0.003, e + 0.006
    q = f"""[out:json][timeout:120];
(way["highway"]({s},{w},{n},{e}); way["waterway"]({s},{w},{n},{e}); way["building"]({s},{w},{n},{e});
 way["natural"]({s},{w},{n},{e}); way["landuse"]({s},{w},{n},{e}); way["power"="line"]({s},{w},{n},{e});
 way["man_made"]({s},{w},{n},{e}); way["leisure"]({s},{w},{n},{e}); way["piste:type"]({s},{w},{n},{e}););
(._;>;); out body;"""
    name = f"osm_{_bbox(bbox).replace(',', '_')}.json"
    for i, server in enumerate(OVERPASS):  # public servers are often busy: try the mirrors
        try:
            p = _get(server + "?" + urllib.parse.urlencode({"data": q}), name)
            break
        except Exception as e:  # noqa: BLE001
            print(f"  {server}: {e}")
            if i == len(OVERPASS) - 1:
                raise
    with open(p) as f:
        d = json.load(f)
    nodes = {el["id"]: latlon_to_utm(el["lat"], el["lon"]) for el in d["elements"] if el["type"] == "node"}
    return [
        {"tags": el.get("tags", {}), "pts": [nodes[i] for i in el["nodes"] if i in nodes]}
        for el in d["elements"] if el["type"] == "way"
    ]


# --- CC0 photo textures (ambientCG, Poly Haven) -----------------------------------------------


def ambientcg(asset, res="2K"):
    """{'color', 'normal', 'rough'} float32 arrays (0..1) of an ambientCG material (CC0)."""
    import io
    import zipfile

    out = {}
    names = {"color": "_Color.jpg", "normal": "_NormalGL.jpg", "rough": "_Roughness.jpg"}
    cached = {k: os.path.join(CACHE, "tex", f"{asset}_{k}.png") for k in names}
    if not all(os.path.exists(p) for p in cached.values()):
        zpath = _get(f"https://ambientcg.com/get?file={asset}_{res}-JPG.zip", f"{asset}_{res}.zip")
        z = zipfile.ZipFile(zpath)
        os.makedirs(os.path.join(CACHE, "tex"), exist_ok=True)
        for k, suffix in names.items():
            src = next(f for f in z.namelist() if f.endswith(suffix))
            Image.open(io.BytesIO(z.read(src))).convert("RGB").save(cached[k])
    for k, p in cached.items():
        out[k] = np.asarray(Image.open(p).convert("RGB"), np.float32) / 255
    return out


def polyhaven_tex(asset, res="2k"):
    """Same as ambientcg() for a Poly Haven texture (CC0)."""
    names = {"color": "Diffuse", "normal": "nor_gl", "rough": "Rough"}
    cached = {k: os.path.join(CACHE, "tex", f"ph_{asset}_{k}.png") for k in names}
    if not all(os.path.exists(p) for p in cached.values()):
        files = json.load(urllib.request.urlopen(urllib.request.Request(
            f"https://api.polyhaven.com/files/{asset}", headers=UA), timeout=60))
        os.makedirs(os.path.join(CACHE, "tex"), exist_ok=True)
        for k, key in names.items():
            url = files[key][res]["jpg"]["url"]
            p = _get(url, f"ph_{asset}_{key}_{res}.jpg")
            Image.open(p).convert("RGB").save(cached[k])
    return {k: np.asarray(Image.open(p).convert("RGB"), np.float32) / 255 for k, p in cached.items()}
