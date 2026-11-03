#!/usr/bin/env python3
"""Headless Gazebo test for a built forest model: writes a flat test world with cameras,
runs `gz sim -s`, grabs one frame per camera and measures the real-time factor.

    GZ_PARTITION=trees_agent python3 bench_forest.py --model-dir DIR --model NAME --out OUT_PREFIX \
        --cam "x y z pitch yaw" [--cam ...] [--hz 15] [--seconds 40]

Uses the system python (gz.transport13 / gz.msgs10). Kills only the gz process it started.
"""
import argparse
import os
import re
import signal
import subprocess
import sys
import time

import numpy as np


def world_sdf(model, cams, hz, shadows=True, far=1500):
    cams = [c if len(c) == 6 else tuple(c) + (hz,) for c in cams]
    cam_xml = "\n".join(f"""
    <model name="cam{i}"><static>true</static><pose>{x} {y} {z} 0 {p} {yw}</pose>
      <link name="l"><sensor name="c" type="camera"><update_rate>{chz}</update_rate><always_on>1</always_on>
        <camera><horizontal_fov>1.2</horizontal_fov><image><width>640</width><height>480</height></image>
          <clip><near>0.1</near><far>{far}</far></clip></camera></sensor></link></model>"""
                        for i, (x, y, z, p, yw, chz) in enumerate(cams))
    return f"""<?xml version="1.0"?>
<sdf version="1.9"><world name="trees_test">
  <physics name="1ms" type="ignored"><max_step_size>0.004</max_step_size><real_time_factor>1.0</real_time_factor></physics>
  <plugin filename="gz-sim-physics-system" name="gz::sim::systems::Physics"/>
  <plugin filename="gz-sim-sensors-system" name="gz::sim::systems::Sensors"><render_engine>ogre2</render_engine></plugin>
  <plugin filename="gz-sim-scene-broadcaster-system" name="gz::sim::systems::SceneBroadcaster"/>
  <scene><ambient>0.45 0.47 0.5 1</ambient><background>0.62 0.74 0.88 1</background><grid>false</grid>
    <sky></sky></scene>
  <light type="directional" name="sun"><cast_shadows>{'true' if shadows else 'false'}</cast_shadows><pose>0 0 100 0 0 0</pose>
    <diffuse>1.0 0.96 0.88 1</diffuse><specular>0.2 0.2 0.2 1</specular>
    <direction>-0.5 0.35 -0.75</direction></light>
  <model name="ground"><static>true</static><link name="l">
    <collision name="c"><geometry><plane><normal>0 0 1</normal><size>6000 6000</size></plane></geometry></collision>
    <visual name="v"><geometry><plane><normal>0 0 1</normal><size>6000 6000</size></plane></geometry>
      <material><ambient>0.25 0.27 0.16 1</ambient><diffuse>0.25 0.27 0.16 1</diffuse></material></visual></link></model>
  <include><uri>model://{model}</uri></include>
  {cam_xml}
</world></sdf>
"""


COUNT = {}
NODE = []


def grab(world, n, proc, timeout=240):
    from gz.transport13 import Node
    from gz.msgs10.image_pb2 import Image as Img
    got = {}
    node = Node()
    COUNT.update({i: 0 for i in range(n)})

    def mk(i):
        def cb(msg):
            COUNT[i] += 1
            got[i] = np.frombuffer(msg.data, np.uint8).reshape(msg.height, msg.width, 3).copy()
        return cb
    for i in range(n):
        node.subscribe(Img, f"/world/{world}/model/cam{i}/link/l/sensor/c/image", mk(i))
    t0 = time.time()
    while len(got) < n and time.time() - t0 < timeout:
        if proc.poll() is not None:
            raise RuntimeError("gz sim exited, see log")
        time.sleep(0.5)
    first = time.time() - t0
    NODE.append(node)  # keep subscriptions alive for the frame-rate count
    time.sleep(6)  # let textures/mips settle, keep the latest frames
    return dict(got), first


def rtf(seconds):
    out = subprocess.run(["gz", "topic", "-e", "-n", str(seconds * 5), "-t", "/stats"], capture_output=True,
                         text=True, timeout=seconds * 4 + 30).stdout
    v = [float(x) for x in re.findall(r"real_time_factor: ([0-9.e-]+)", out)]
    return (float(np.mean(v[len(v) // 3:])) if v else float("nan")), len(v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model-dir", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cam", action="append", required=True, help='"x y z pitch yaw [hz]"')
    ap.add_argument("--hz", type=float, default=15)
    ap.add_argument("--rtf-seconds", type=int, default=20)
    ap.add_argument("--no-shadows", action="store_true")
    ap.add_argument("--timeout", type=int, default=600)
    a = ap.parse_args()
    cams = [tuple(float(v) for v in c.split()) for c in a.cam]
    wf = a.out + "_world.sdf"
    open(wf, "w").write(world_sdf(a.model, cams, a.hz, not a.no_shadows))
    env = dict(os.environ)
    env.setdefault("GZ_PARTITION", "trees_agent")
    env["GZ_IP"] = "127.0.0.1"
    env["GZ_SIM_RESOURCE_PATH"] = a.model_dir + ":" + env.get("GZ_SIM_RESOURCE_PATH", "")
    os.environ.update(env)
    log = open(a.out + "_gz.log", "w")
    t0 = time.time()
    proc = subprocess.Popen(["timeout", str(a.timeout), "gz", "sim", "-s", "-r", wf], stdout=log, stderr=log, env=env,
                            start_new_session=True)
    try:
        frames, first = grab("trees_test", len(cams), proc)
        load = time.time() - t0 - 6
        print(f"first frames after {load:.1f} s (got {sorted(frames)})", flush=True)
        c0 = dict(COUNT); t1 = time.time()
        r, nmsg = rtf(a.rtf_seconds)
        hz = [(COUNT[i] - c0[i]) / (time.time() - t1) for i in sorted(COUNT)]
        rss = subprocess.run(["ps", "-o", "rss=", "-g", str(proc.pid)], capture_output=True, text=True).stdout.split()
        rss_gb = sum(int(x) for x in rss) / 1e6
        print(f"RTF {r:.3f} ({nmsg} samples), camera wall-clock Hz {[round(h, 1) for h in hz]}, gz RSS {rss_gb:.1f} GB",
              flush=True)
        from PIL import Image
        tiles = [frames.get(i, np.zeros((480, 640, 3), np.uint8)) for i in range(len(cams))]
        for i, t in enumerate(tiles):
            Image.fromarray(t).save(f"{a.out}_cam{i}.png")
        cols = min(len(tiles), 3); rows = (len(tiles) + cols - 1) // cols
        W = np.zeros((rows * 480, cols * 640, 3), np.uint8)
        for i, t in enumerate(tiles):
            W[(i // cols) * 480:(i // cols + 1) * 480, (i % cols) * 640:(i % cols + 1) * 640] = t
        Image.fromarray(W).save(a.out + "_montage.jpg", quality=90)
        print(f"RESULT load_s={load:.1f} rtf={r:.3f} cam_hz={np.mean(hz):.1f} rss_gb={rss_gb:.1f}")
    finally:
        os.killpg(proc.pid, signal.SIGINT)
        try:
            proc.wait(15)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)


if __name__ == "__main__":
    sys.exit(main())
