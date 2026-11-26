# Tree assets and forest chunk builder

Realistic boreal trees (Norway spruce, Scots pine, birch, grey alder, juniper) for the Gazebo
Harmonic sim, and a builder that merges tens of thousands of placed trees into a few hundred
chunk meshes. Gazebo has no mesh LOD and pays per entity, so this is done offline.

```
sim/tools/trees/
  fetch_sources.py   download the CC0 source textures (Poly Haven, ambientCG) into the cache
  textures.py        compose bark.jpg + foliage.png atlases from them -> sim/assets/trees/textures, atlas.json
  treegen.py         procedural trees (near tier) + baked impostors (far tier) -> sim/assets/trees
  raster.py          tiny numpy rasteriser used to bake impostors / previews (no Blender, no GPU)
  glb.py             minimal glTF 2.0 binary writer
  forest_builder.py  build_forest(): instances -> one Gazebo model of merged chunk meshes; --demo CLI
  bench_forest.py    headless Gazebo test: renders camera images and measures the real-time factor
sim/assets/trees/
  textures/bark.jpg (1024x2048), foliage.png (2048x2048 RGBA), far.png (2048x2048 RGBA)
  meshes/<species>_<v>.glb, <species>_<v>_far.glb   single trees (1.0 m tall, Z-up, origin at trunk base)
  library.npz, trees.json, atlas.json                 data read by forest_builder.py
  ATTRIBUTION.md                                      sources and licences (all CC0)
```

## Rebuilding the assets

Only needed if you change the generators. Tools live in a separate venv (numpy, Pillow, scipy):

```bash
python3 -m venv ~/cogidrone_deps/simtools-venv
~/cogidrone_deps/simtools-venv/bin/pip install numpy pillow scipy
cd sim/tools/trees
python3 fetch_sources.py                               # -> ~/cogidrone_deps/simassets-cache/trees (~40 MB)
~/cogidrone_deps/simtools-venv/bin/python textures.py  # atlases, ~20 s
~/cogidrone_deps/simtools-venv/bin/python treegen.py   # meshes + impostors + preview.jpg, ~1 min
```

Everything is seeded, so re-running gives the same files.

## Building a forest

`forest_builder.py` needs only numpy (system python3 is fine).

```python
import sys; sys.path.insert(0, "sim/tools/trees")
from forest_builder import build_forest

instances = [
    {"species": "spruce", "x": 12.0, "y": -40.5, "z": 231.2, "height": 24.0},           # minimum
    {"species": "pine", "x": 30.0, "y": 8.0, "z": 240.0, "height": 18.0,
     "yaw": 1.2, "tint": (0.97, 0.93, 0.76), "variant": 1, "tier": "far"},              # everything
]
stats = build_forest(instances, "sim/models", "bymarka_forest", chunk_size=100.0,
                     tier="auto", near_center=(0.0, 0.0), near_radius=650.0, collision="mesh")
```

Instance keys: `species` (`spruce|pine|birch|alder|juniper`), `x, y, z` (trunk base, z = ground
height, metres), `height` (m). Optional: `yaw` (rad, default random), `tint` (r,g,b multiplier,
snapped to the nearest of 4 foliage tint materials, default random), `variant` (int, default
random), `seed` (int for the random defaults, default = list index), `tier` (`near|far`).

Call options: `tier` default for instances without one (`near`, `far`, `auto` = near inside
`near_radius` of `near_center`), `collision` (`mesh` = one merged trunk-prism collision mesh per
chunk, recommended; `cylinders` = one SDF cylinder per tree, only for a few thousand trees;
`none`), `collide_far` (default False: far trees get no collision), `tints`, `lean_deg`,
`cast_shadows`, `seed`. Returns a stats dict (tree counts, triangles, files, MB, seconds).

The output is `out_dir/model_name/{model.config, model.sdf, meshes/*.glb, materials/textures/*}`:
one static model, one link; per chunk one visual for bark, one per used foliage tint and one per
used far tint. Add `out_dir` to `GZ_SIM_RESOURCE_PATH` and `<include><uri>model://model_name</uri></include>`.
The mesh files are large (about 60 kB per near tree) and should not be committed; build them
as part of world generation.

Test forest: `python3 sim/tools/trees/forest_builder.py --demo 5000 [--near-radius 300] [--gallery]`
(written to `~/cogidrone_deps/simassets-cache/forest_demo` unless `--out` is given).

## Headless test

```bash
export GZ_CONFIG_PATH=/usr/share/gz GZ_IP=127.0.0.1 GZ_PARTITION=trees_test
python3 sim/tools/trees/bench_forest.py --model-dir ~/cogidrone_deps/simassets-cache/forest_demo \
   --model forest_demo --out /tmp/forest --hz 15 \
   --cam "0 0 3 0 0.3" --cam "-300 -300 40 0.3 0.785" --cam "-560 -560 150 0.35 0.785"
```

## Notes

* glTF positions are used as-is by Gazebo, so all meshes are stored Z-up.
* Gazebo ignores vertex colours, so tint variation is done with a few SDF material variants
  (`<diffuse>` multiplies the albedo map). SDF colours must be <= 1.
* Any `albedo_map` gets an alpha test at 0.5; there is no alpha-to-coverage, so the sprites have
  an opaque "underlayer" mass and the impostor alpha is boosted so trees don't dissolve in the
  small mip levels.
* Per-tree SDF collision cylinders kill physics speed (20 000 cylinders: RTF 0.07); the merged
  per-chunk collision mesh keeps RTF 1.0.
