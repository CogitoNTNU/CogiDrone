#!/usr/bin/env python3
"""Write a headless test world for prop models: textured ground, sun, static cameras.

    python3 test_world.py --out W.sdf --include props_demo [--include other_model] \
        --cam 0,0,2,0.3,0  --cam 0,-20,15,0.6,1.57  ...     (x,y,z,pitch,yaw)

Cameras are named cam0..camN (topic /world/props_test/model/camI/link/l/sensor/c/image),
640x480; --rate sets their update rate (15 Hz for benchmarking).
Also: python3 test_world.py --gallery-instances gallery.json  (one of every variant in a row)
"""
import argparse
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glb import write_glb  # noqa: E402

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
TEX = os.path.join(REPO, "sim", "models", "sar_terrain", "materials", "textures")


def ground_mesh(path, size=600.0, tile=4.0):
    h = size / 2
    V = np.array([[-h, -h, 0], [h, -h, 0], [h, h, 0], [-h, h, 0]], np.float32)
    UV = V[:, :2] / tile
    write_glb(path, V, np.tile([0, 0, 1], (4, 1)), UV, np.array([[0, 1, 2], [0, 2, 3]]), name="ground")


def gallery_instances(lib_dir=None, spacing=1.0):
    from props_builder import Library
    lib = Library.get(lib_dir)
    out, x = [], 0.0
    for v in sorted(lib.variants, key=lambda e: (e["kind"], e["name"])):
        (x0, y0, _), (x1, y1, _) = v["bounds"]
        w = x1 - x0
        x += w / 2
        out.append({"kind": v["kind"], "variant": v["name"], "x": x - x0 - w / 2, "y": -(y0 + y1) / 2, "z": 0.0,
                    "yaw": 0.0, "name": v["name"]})
        x += w / 2 + spacing
    for d in out:
        d["x"] -= x / 2
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--include", action="append", default=[])
    ap.add_argument("--cam", action="append", default=[])
    ap.add_argument("--rate", type=float, default=1.0)
    ap.add_argument("--shadows", type=int, default=1)
    ap.add_argument("--ground", default="forest_floor")
    ap.add_argument("--gallery-instances")
    a = ap.parse_args()
    if a.gallery_instances:
        with open(a.gallery_instances, "w") as f:
            json.dump(gallery_instances(), f)
        return
    d = os.path.dirname(os.path.abspath(a.out))
    gm = os.path.join(d, "ground_tiled.glb")
    ground_mesh(gm)
    cams = []
    for i, c in enumerate(a.cam):
        x, y, z, p, yw = [float(v) for v in c.split(",")]
        cams.append(f"""  <model name="cam{i}"><static>true</static><pose>{x} {y} {z} 0 {p} {yw}</pose>
    <link name="l"><sensor name="c" type="camera"><update_rate>{a.rate}</update_rate><always_on>1</always_on>
      <camera><horizontal_fov>1.2</horizontal_fov><image><width>640</width><height>480</height></image>
      <clip><near>0.1</near><far>500</far></clip></camera></sensor></link></model>""")
    inc = "\n".join(f"  <include><uri>model://{m}</uri></include>" for m in a.include)
    g = a.ground
    sdf = f"""<?xml version="1.0"?>
<sdf version="1.9"><world name="props_test">
  <physics name="1ms" type="ignored"><max_step_size>0.004</max_step_size><real_time_factor>1.0</real_time_factor></physics>
  <plugin filename="gz-sim-physics-system" name="gz::sim::systems::Physics"/>
  <plugin filename="gz-sim-sensors-system" name="gz::sim::systems::Sensors"><render_engine>ogre2</render_engine></plugin>
  <plugin filename="gz-sim-scene-broadcaster-system" name="gz::sim::systems::SceneBroadcaster"/>
  <scene><ambient>0.45 0.47 0.5 1</ambient><background>0.62 0.72 0.85 1</background><shadows>{'true' if a.shadows else 'false'}</shadows><grid>false</grid></scene>
  <light type="directional" name="sun"><cast_shadows>{'true' if a.shadows else 'false'}</cast_shadows><pose>0 0 100 0 0 0</pose>
    <diffuse>1.0 0.96 0.88 1</diffuse><specular>0.3 0.3 0.3 1</specular><intensity>1.4</intensity>
    <direction>-0.45 0.35 -0.8</direction></light>
  <model name="ground"><static>true</static><link name="l">
    <collision name="c"><geometry><plane><normal>0 0 1</normal><size>600 600</size></plane></geometry></collision>
    <visual name="v"><cast_shadows>false</cast_shadows><geometry><mesh><uri>{gm}</uri></mesh></geometry>
      <material><diffuse>1 1 1 1</diffuse><pbr><metal>
        <albedo_map>{TEX}/{g}_color.jpg</albedo_map><normal_map>{TEX}/{g}_normal.jpg</normal_map>
        <roughness>0.95</roughness><metalness>0</metalness></metal></pbr></material></visual>
  </link></model>
{inc}
{chr(10).join(cams)}
</world></sdf>
"""
    with open(a.out, "w") as f:
        f.write(sdf)


if __name__ == "__main__":
    main()
