#!/usr/bin/env python3
"""Assemble the ogre2 media overlay directory needed for
GZ_RENDERING_RESOURCE_PATH to serve our custom sky.

Gazebo Harmonic's ogre2 render engine does NOT support multiple
colon-separated paths in GZ_RENDERING_RESOURCE_PATH (tested: passing
"A:B" makes it treat "A:B" as one literal (broken) path, it does not
search each entry) -- so the override must be a single directory that
contains Ogre2's *entire* media tree, not just our skybox.dds.

This script copies the stock gz-rendering8 ogre2/media tree (shaders,
Hlms scripts, PBR materials -- everything else in the scene still needs
these) from the system install and drops our custom
sim/assets/sky/skybox.dds on top of the stock one. The result is NOT
committed to the repo (it's mostly a verbatim copy of files already on
the machine) -- run this once per machine/gz-rendering-version to produce
a local overlay directory, then point GZ_RENDERING_RESOURCE_PATH at it.

Usage:
    python3 build_media_overlay.py [--stock /usr/share/gz/gz-rendering8/ogre2] \
        [--out ~/cogidrone_deps/simassets-cache/sky/media_root] \
        [--dds ../../assets/sky/skybox.dds]

Then:
    export GZ_RENDERING_RESOURCE_PATH=<out>
    gz sim -s -r your_world.sdf   # world needs <scene><sky></sky></scene>
"""
import argparse
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stock", default="/usr/share/gz/gz-rendering8/ogre2",
                     help="stock ogre2 media tree to copy (adjust version for other gz-rendering installs)")
    ap.add_argument("--out", default=os.path.expanduser("~/cogidrone_deps/simassets-cache/sky/media_root"))
    ap.add_argument("--dds", default=os.path.join(HERE, "..", "..", "assets", "sky", "skybox.dds"))
    args = ap.parse_args()

    if not os.path.isdir(args.stock):
        raise SystemExit(f"stock ogre2 media dir not found: {args.stock} "
                          f"(pass --stock to point at your gz-rendering installation)")
    dds = os.path.abspath(args.dds)
    if not os.path.isfile(dds):
        raise SystemExit(f"custom skybox.dds not found: {dds} (run make_sky.py first)")

    dst_ogre2 = os.path.join(os.path.abspath(args.out), "ogre2")
    if os.path.exists(dst_ogre2):
        print(f"[build_media_overlay] {dst_ogre2} already exists, removing and rebuilding")
        shutil.rmtree(dst_ogre2)
    print(f"[build_media_overlay] copying {args.stock} -> {dst_ogre2}")
    shutil.copytree(args.stock, dst_ogre2)

    dest_tex = os.path.join(dst_ogre2, "media", "materials", "textures", "skybox.dds")
    shutil.copy(dds, dest_tex)
    print(f"[build_media_overlay] installed custom sky -> {dest_tex}")

    overlay_root = os.path.abspath(args.out)
    print(f"[build_media_overlay] done. export GZ_RENDERING_RESOURCE_PATH={overlay_root}")


if __name__ == "__main__":
    main()
