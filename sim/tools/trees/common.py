"""Shared paths for the tree tooling."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
ASSET_DIR = os.path.join(REPO, "sim", "assets", "trees")
TEX_DIR = os.path.join(ASSET_DIR, "textures")
MESH_DIR = os.path.join(ASSET_DIR, "meshes")
CACHE = os.path.join(os.environ.get("SIMASSETS_CACHE", os.path.expanduser("~/cogidrone_deps/simassets-cache")), "trees")
