#!/usr/bin/env python3
"""Post-flight head detection on a run-log's saved person crops (and v1 -> v2 upgrade).

    ~/cogidrone_deps/venv/bin/python tools/flight_viewer/heads.py [RUN_DIR] [--model M] [--conf 0.25]

Runs the head detector (Muhammad's python/cv_prototyping/inference/depth_head.py: distance =
head height * fy / head pixel height) on crops/sNNNNN.jpg of every sighting that has no head
yet, and fills the format-v2 columns of sightings.csv: rays (from the logged camera quaternion
and box), head box / conf / px, d_head, d_body and, for 'ground' sightings of v1 logs, the
ground hit (= the logged position). The crops are cut from the full-resolution image (boxes
under ~100 px are kept at full resolution), so this is about what the in-flight detector sees.
Rewrites sightings.csv in place (a copy is kept as sightings.v1.csv the first time).
"""

import argparse
import csv
import json
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from runlog import SIGHT_V1, SIGHT_V2


def qrot(w, x, y, z):
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
                     [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
                     [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run", nargs="?", default=os.path.expanduser("~/flight_runs/latest"))
    ap.add_argument("--model", default=os.path.expanduser("~/cogidrone_deps/models/head_medium.pt"))
    ap.add_argument("--conf", type=float, default=0.25)
    ap.add_argument("--imgsz", type=int, default=320)
    ap.add_argument("--min-px", type=float, default=0.0, help="skip person boxes shorter than this (full-res px)")
    ap.add_argument("--head-height-m", type=float, default=None, help="default: meta estimators or 0.23")
    ap.add_argument("--body-height-m", type=float, default=None, help="default: meta estimators or 1.70")
    ap.add_argument("--redo", action="store_true", help="also re-detect sightings that already have a head")
    ap.add_argument("--device", default="cuda:0")
    a = ap.parse_args()
    run = os.path.abspath(os.path.expanduser(a.run))
    meta = json.load(open(os.path.join(run, "meta.json")))
    cam = meta["camera"]
    fx, fy, cx, cy, W, H = cam["fx"], cam["fy"], cam["cx"], cam["cy"], cam["width"], cam["height"]
    est = meta.get("estimators", {})
    hh = a.head_height_m or est.get("head_height_m", 0.23)
    bh = a.body_height_m or est.get("body_height_m", 1.70)
    path = os.path.join(run, "sightings.csv")
    rows = list(csv.DictReader(open(path)))
    if rows and "ray_f_e" not in rows[0] and not os.path.exists(os.path.join(run, "sightings.v1.csv")):
        shutil.copy(path, os.path.join(run, "sightings.v1.csv"))
    from PIL import Image
    from ultralytics import YOLO
    model = YOLO(a.model)
    ms, tried, found = [], 0, 0
    f = lambda v, n=2: f"{v:.{n}f}"
    for r in rows:
        u0, v0, u1, v1 = (float(r[k]) for k in ("u0", "v0", "u1", "v1"))
        R = None
        if r.get("cam_qw"):
            R = qrot(*(float(r[k]) for k in ("cam_qw", "cam_qx", "cam_qy", "cam_qz")))

        def ray(key, uu, vv):
            if R is None:
                return
            d = R @ np.array([(uu - cx) / fx, (vv - cy) / fy, 1.0])
            d /= np.linalg.norm(d)
            for ax, val in zip("enu", d):
                r[f"ray_{key}_{ax}"] = f(val, 5)

        ray("f", (u0 + u1) / 2, v1)
        ray("c", (u0 + u1) / 2, (v0 + v1) / 2)
        r["d_body"] = f(bh * fy / max(1.0, v1 - v0))
        if r.get("method") == "ground" and not r.get("gnd_e"):
            r.update({"gnd_e": r["e"], "gnd_n": r["n"], "gnd_u": r["u"], "d_gnd": r["range_m"]})
        if r.get("method") == "depth" and not r.get("d_depth"):
            r["d_depth"] = r["range_m"]
        if (r.get("head_px") and not a.redo) or not r.get("crop") or v1 - v0 < a.min_px:
            continue
        cp = os.path.join(run, r["crop"])
        if not os.path.exists(cp):
            continue
        # crop geometry of RunLog.sighting: box + 0.3*size + 4 px, clipped, then scaled
        pu, pv = (u1 - u0) * 0.3 + 4, (v1 - v0) * 0.3 + 4
        cu0, cv0 = max(0, int(u0 - pu)), max(0, int(v0 - pv))
        cu1, cv1 = min(W, int(u1 + pu)), min(H, int(v1 + pv))
        im = Image.open(cp).convert("RGB")
        kx, ky = im.width / max(1, cu1 - cu0), im.height / max(1, cv1 - cv0)
        t0 = time.monotonic()
        res = model.predict(np.asarray(im)[:, :, ::-1], imgsz=a.imgsz, conf=a.conf, device=a.device, verbose=False)[0]
        ms.append((time.monotonic() - t0) * 1000)
        tried += 1
        best = None
        for (x0, y0, x1, y1), c in zip(res.boxes.xyxy.tolist(), res.boxes.conf.tolist()):
            g = (cu0 + x0 / kx, cv0 + y0 / ky, cu0 + x1 / kx, cv0 + y1 / ky)
            gu, gv = (g[0] + g[2]) / 2, (g[1] + g[3]) / 2
            if u0 - 2 <= gu <= u1 + 2 and v0 - 2 <= gv <= v0 + 0.6 * (v1 - v0) and (best is None or c > best[4]):
                best = (*g, c)
        for k in ("head_u0", "head_v0", "head_u1", "head_v1", "head_conf", "head_px", "d_head",
                  "ray_h_e", "ray_h_n", "ray_h_u"):
            r[k] = ""
        if best:
            found += 1
            hp = best[3] - best[1]
            r.update({"head_u0": f(best[0], 1), "head_v0": f(best[1], 1), "head_u1": f(best[2], 1),
                      "head_v1": f(best[3], 1), "head_conf": f(best[4]), "head_px": f(hp, 1),
                      "d_head": f(hh * fy / max(1e-3, hp))})
            ray("h", (best[0] + best[2]) / 2, (best[1] + best[3]) / 2)
    tmp = path + ".part"
    with open(tmp, "w", newline="") as fo:
        w = csv.DictWriter(fo, fieldnames=SIGHT_V1 + SIGHT_V2, extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") or "" for k in SIGHT_V1 + SIGHT_V2})
    os.replace(tmp, path)
    est.update({"head_height_m": hh, "body_height_m": bh})
    meta["estimators"] = est
    meta["heads_post"] = {"model": a.model, "conf": a.conf, "imgsz": a.imgsz, "tried": tried, "found": found,
                          "ms_median": round(float(np.median(ms)), 1) if ms else None}
    meta["version"] = max(2, meta.get("version", 1))
    json.dump(meta, open(os.path.join(run, "meta.json"), "w"), indent=1, default=float)
    print(f"{run}: {len(rows)} sightings, head detector on {tried} crops, head found in {found}"
          f"{f', {np.median(ms):.0f} ms median' if ms else ''}")


if __name__ == "__main__":
    main()
