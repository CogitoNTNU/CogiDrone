#!/usr/bin/env python3
"""Download the CC0 source assets (Poly Haven) used to build sim/assets/props.

Downloads go to a cache outside the repo (default ~/cogidrone_deps/simassets-cache/props).
Idempotent: files already present are skipped.

    python fetch_assets.py [--cache DIR] [--res 2k]
"""
import argparse
import json
import os
import sys
import urllib.request

API = "https://api.polyhaven.com"

# Poly Haven models (all CC0)
MODELS = [
    "rock_moss_set_01", "rock_moss_set_02", "rock_07",
    "rock_face_01", "rock_face_02",
    "fern_02",
    "tree_stump_01", "tree_stump_02", "dead_tree_trunk", "dry_branches_medium_01",
]
# Foliage models whose diffuse needs a separate alpha mask
ALPHA_MODELS = {"fern_02"}
# Poly Haven PBR textures (CC0) used as tiling detail materials
TEXTURES = ["lichen_rock"]


def get_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "cogidrone-props/1.0"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def fetch(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0:
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    tmp = dst + ".part"
    print("  get", url, flush=True)
    req = urllib.request.Request(url, headers={"User-Agent": "cogidrone-props/1.0"})
    with urllib.request.urlopen(req, timeout=300) as r, open(tmp, "wb") as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
    os.replace(tmp, dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=os.path.expanduser("~/cogidrone_deps/simassets-cache/props"))
    ap.add_argument("--res", default="2k")
    a = ap.parse_args()
    for m in MODELS:
        print(m, flush=True)
        files = get_json(f"{API}/files/{m}")
        g = files["gltf"][a.res]["gltf"]
        d = os.path.join(a.cache, m)
        fetch(g["url"], os.path.join(d, os.path.basename(g["url"])))
        for rel, v in g["include"].items():
            fetch(v["url"], os.path.join(d, rel))
        if m in ALPHA_MODELS:
            al = files["Alpha"][a.res]["png"]
            fetch(al["url"], os.path.join(d, "textures", os.path.basename(al["url"])))
        info = get_json(f"{API}/info/{m}")
        with open(os.path.join(d, "info.json"), "w") as f:
            json.dump(info, f)
    for t in TEXTURES:
        print(t, flush=True)
        files = get_json(f"{API}/files/{t}")
        d = os.path.join(a.cache, "tex_" + t)
        for key, name in (("Diffuse", "diff"), ("nor_gl", "nor_gl"), ("Rough", "rough")):
            v = files[key][a.res]["jpg"]
            fetch(v["url"], os.path.join(d, f"{t}_{name}.jpg"))
    print("done")


if __name__ == "__main__":
    sys.exit(main())
