#!/usr/bin/env python3
"""Download the CC0 source textures used to build the tree atlases.

Everything lands in ~/cogidrone_deps/simassets-cache/trees (override with SIMASSETS_CACHE).
Only stdlib is needed. Re-running skips files that are already there.

Sources (all CC0, see sim/assets/trees/ATTRIBUTION.md):
  Poly Haven  fir_tree_01, pine_tree_01, tree_small_02  (twig/leaf colour + alpha, bark)
  ambientCG   Bark006, Bark007, Bark014                (bark colour)
"""
import io
import json
import os
import sys
import urllib.request
import zipfile

CACHE = os.path.join(os.environ.get("SIMASSETS_CACHE", os.path.expanduser("~/cogidrone_deps/simassets-cache")), "trees")

# (asset, map key, resolution, format)
POLYHAVEN = [
    ("fir_tree_01", "twig_diff", "2k", "png"),
    ("fir_tree_01", "twig_alpha", "2k", "png"),
    ("fir_tree_01", "bark_diff", "1k", "jpg"),
    ("pine_tree_01", "twig_diff", "2k", "png"),
    ("pine_tree_01", "twig_alpha", "2k", "png"),
    ("pine_tree_01", "bark_diff", "1k", "jpg"),
    ("tree_small_02", "leaves_diff", "2k", "png"),
    ("tree_small_02", "leaves_alpha", "2k", "png"),
    ("island_tree_01", "leaves_diff", "2k", "png"),
    ("island_tree_01", "leaves_alpha", "2k", "png"),
]
AMBIENTCG = ["Bark006", "Bark007", "Bark014"]
UA = {"User-Agent": "CogiDrone-sim-tools/1.0"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r:
        return r.read()


def main():
    os.makedirs(CACHE, exist_ok=True)
    for asset, key, res, fmt in POLYHAVEN:
        out = os.path.join(CACHE, f"{asset}_{key}_{res}.{fmt}")
        if os.path.exists(out):
            continue
        files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
        url = files[key][res][fmt]["url"]
        print("get", url, flush=True)
        open(out, "wb").write(get(url))
    for aid in AMBIENTCG:
        out = os.path.join(CACHE, f"{aid}_Color.jpg")
        if os.path.exists(out):
            continue
        url = f"https://ambientcg.com/get?file={aid}_1K-JPG.zip"
        print("get", url, flush=True)
        z = zipfile.ZipFile(io.BytesIO(get(url)))
        for n in z.namelist():
            if n.endswith("_Color.jpg"):
                open(out, "wb").write(z.read(n))
    print("sources in", CACHE)


if __name__ == "__main__":
    sys.exit(main())
