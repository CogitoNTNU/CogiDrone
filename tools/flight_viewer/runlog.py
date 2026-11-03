"""Writer for the flight run-log the viewer reads (format: README.md in this folder).

Only numpy + Pillow. Thread safe (track samples and camera frames usually come from different
threads). Typical use:

    log = RunLog("~/flight_runs/latest", meta={"source": "sim", "world": "bymarka_dense_s"})
    log.set_origin(lat, lon, alt)                      # local NED/ENU origin (PX4 EKF origin)
    log.track(t, p_ned, R_frd_to_ned)                   # every pose; rate-limited to --track-hz
    fid = log.frame(t, rgb, p_ned, R_ned, cam_ned, Rc)  # every camera frame; sampled
    fid = log.frame(..., force=True)                    # the frame of a sighting: always kept
    log.sighting(t, ..., frame=fid)
    log.close(persons=[...])
"""

import csv
import datetime as dt
import json
import math
import os
import shutil
import threading

import numpy as np

EARTH_R = 6371000.0  # PX4's map projection radius
FORMAT_VERSION = 2

# v2: raw observations per sighting so the viewer can re-solve positions after the flight
SIGHT_V1 = ["t", "person", "method", "conf", "u0", "v0", "u1", "v1", "e", "n", "u", "lat", "lon", "range_m",
            "r", "g", "b", "frame", "crop", "cam_e", "cam_n", "cam_u", "cam_qw", "cam_qx", "cam_qy", "cam_qz"]
SIGHT_V2 = ["cam_lat", "cam_lon", "cam_alt",
            "ray_f_e", "ray_f_n", "ray_f_u", "ray_c_e", "ray_c_n", "ray_c_u", "ray_h_e", "ray_h_n", "ray_h_u",
            "head_u0", "head_v0", "head_u1", "head_v1", "head_conf", "head_px",
            "d_head", "d_body", "d_depth", "gnd_e", "gnd_n", "gnd_u", "d_gnd"]
TRACK = ["t", "lat", "lon", "alt", "e", "n", "u", "roll", "pitch", "yaw", "src", "eph", "epv"]

# Camera optical (x right, y down, z forward) -> camera FRD (x forward, y right, z down)
OPT_TO_FRD = np.array([[0.0, 0.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]])
# NED <-> ENU (its own inverse)
NED_ENU = np.array([[0.0, 1.0, 0.0], [1.0, 0.0, 0.0], [0.0, 0.0, -1.0]])


def ned_to_latlon(x, y, lat0, lon0):
    """PX4's azimuthal equidistant projection (lib/geo): local NED x/y -> lat/lon degrees."""
    la0 = math.radians(lat0)
    xr, yr = x / EARTH_R, y / EARTH_R
    c = math.hypot(xr, yr)
    if c == 0:
        return lat0, lon0
    sc, cc = math.sin(c), math.cos(c)
    lat = math.asin(cc * math.sin(la0) + xr * sc * math.cos(la0) / c)
    lon = math.radians(lon0) + math.atan2(yr * sc, c * math.cos(la0) * cc - xr * math.sin(la0) * sc)
    return math.degrees(lat), math.degrees(lon)


def latlon_to_ned(lat, lon, lat0, lon0):
    """Inverse of ned_to_latlon: (north, east) metres."""
    la, lo, la0, lo0 = map(math.radians, (lat, lon, lat0, lon0))
    cos_c = math.sin(la0) * math.sin(la) + math.cos(la0) * math.cos(la) * math.cos(lo - lo0)
    c = math.acos(max(-1.0, min(1.0, cos_c)))
    k = 1.0 if c == 0 else c / math.sin(c)
    x = k * (math.cos(la0) * math.sin(la) - math.sin(la0) * math.cos(la) * math.cos(lo - lo0)) * EARTH_R
    y = k * math.cos(la) * math.sin(lo - lo0) * EARTH_R
    return x, y


def rot_to_quat(R):
    """3x3 rotation -> (w, x, y, z)."""
    t = np.trace(R)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        q = (0.25 * s, (R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s)
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = math.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2
        q = ((R[2, 1] - R[1, 2]) / s, 0.25 * s, (R[0, 1] + R[1, 0]) / s, (R[0, 2] + R[2, 0]) / s)
    elif R[1, 1] > R[2, 2]:
        s = math.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2
        q = ((R[0, 2] - R[2, 0]) / s, (R[0, 1] + R[1, 0]) / s, 0.25 * s, (R[1, 2] + R[2, 1]) / s)
    else:
        s = math.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2
        q = ((R[1, 0] - R[0, 1]) / s, (R[0, 2] + R[2, 0]) / s, (R[1, 2] + R[2, 1]) / s, 0.25 * s)
    q = np.array(q)
    q /= np.linalg.norm(q)
    return q if q[0] >= 0 else -q


def euler_ned(R):
    """FRD -> NED rotation -> (roll, pitch, yaw) degrees, yaw clockwise from north."""
    roll = math.atan2(R[2, 1], R[2, 2])
    pitch = -math.asin(max(-1.0, min(1.0, R[2, 0])))
    yaw = math.atan2(R[1, 0], R[0, 0])
    return math.degrees(roll), math.degrees(pitch), math.degrees(yaw)


def cam_rot_enu(R_cam_frd_ned):
    """Camera FRD -> NED rotation -> camera OPTICAL (x right, y down, z forward) -> local ENU."""
    return NED_ENU @ R_cam_frd_ned @ OPT_TO_FRD


def cam_pose_enu(p_cam_ned, R_cam_frd_ned):
    """Camera centre (NED) + camera FRD -> NED rotation -> (e, n, u), quaternion (w, x, y, z) of
    the camera OPTICAL frame (x right, y down, z forward) in local ENU."""
    return NED_ENU @ np.asarray(p_cam_ned, float), rot_to_quat(cam_rot_enu(R_cam_frd_ned))


def _f(v, n=2):
    return f"{v:.{n}f}"


class RunLog:
    """One run directory. The previous run in `out` is replaced; keep=True also copies the
    finished run to <out>/../<start time>_<world>/ on close()."""

    def __init__(self, out="~/flight_runs/latest", meta=None, keep=False, track_hz=5.0,
                 frame_every=3.0, frame_min_move=2.0, frame_min_turn=15.0, max_frames=2000,
                 frame_width=640, jpeg_quality=80):
        self.out = os.path.abspath(os.path.expanduser(out))
        if os.path.isdir(self.out):
            shutil.rmtree(self.out)
        os.makedirs(os.path.join(self.out, "frames"))
        os.makedirs(os.path.join(self.out, "crops"))
        self.keep = keep
        self.lock = threading.Lock()
        self.track_dt = 1.0 / track_hz if track_hz > 0 else 0.0
        self.frame_every, self.min_move, self.min_turn = frame_every, frame_min_move, frame_min_turn
        self.max_frames, self.frame_width, self.q = max_frames, frame_width, jpeg_quality
        self.t0 = None
        self.origin = None
        self.meta = {
            "format": "cogidrone-flight-run", "version": FORMAT_VERSION,
            "start_time": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
            "source": "sim", "world": "",
            "sampling": {"track_hz": track_hz, "frame_every_s": frame_every,
                         "frame_min_move_m": frame_min_move, "frame_min_turn_deg": frame_min_turn,
                         "max_frames": max_frames, "frame_width_px": frame_width,
                         "jpeg_quality": jpeg_quality},
        }
        self.meta.update(meta or {})
        self.n_track = self.n_frames = self.n_sight = 0
        self.last_track_t = -1e9
        self.last_try_t = -1e9
        self.last_kept = None  # (position, forward vector)
        self.frame_by_t = {}  # image time -> frame id (one saved image per camera frame)
        self.files = {}
        self.writers = {}
        self._csv("track", TRACK)
        self._csv("frames", ["id", "t", "file", "e", "n", "u", "roll", "pitch", "yaw",
                             "cam_e", "cam_n", "cam_u", "cam_qw", "cam_qx", "cam_qy", "cam_qz", "kind"])
        self._csv("sightings", SIGHT_V1 + SIGHT_V2)
        self.est = {"head_height_m": 0.23, "body_height_m": 1.70}
        self.write_meta()

    # --- plumbing ---------------------------------------------------------------------------

    def _csv(self, name, header):
        f = open(os.path.join(self.out, f"{name}.csv"), "w", newline="")
        w = csv.writer(f, lineterminator="\n")
        w.writerow(header)
        self.files[name], self.writers[name] = f, w

    def _row(self, name, row):
        self.writers[name].writerow(row)
        self.files[name].flush()

    def write_meta(self, **extra):
        with self.lock:
            self.meta.update(extra)
            self.meta["counts"] = {"track": self.n_track, "frames": self.n_frames, "sightings": self.n_sight}
            tmp = os.path.join(self.out, "meta.json.part")
            with open(tmp, "w") as f:
                json.dump(self.meta, f, indent=1, default=float)
            os.replace(tmp, os.path.join(self.out, "meta.json"))

    def set_origin(self, lat, lon, alt):
        """Local frame origin (PX4: the EKF origin, ref_lat/ref_lon/ref_alt). lat/lon None (or
        not finite) = no global position: the log stays in the local metric frame (lat/lon
        columns empty); the viewer then uses an anchor (view.py --anchor) or a plain grid."""
        ok = lat is not None and lon is not None and math.isfinite(lat) and math.isfinite(lon) \
            and not (lat == 0 and lon == 0)
        alt = float(alt) if alt is not None and math.isfinite(alt) else 0.0
        self.origin = (float(lat), float(lon), alt) if ok else (None, None, alt)
        self.write_meta(origin={"lat": lat, "lon": lon, "alt": alt} if ok else None)

    def set_position_source(self, source, global_ok=True, **info):
        """Where the poses come from: 'px4_ekf_gps' (PX4 EKF with GPS, lat/lon valid),
        'px4_ekf_local' (no GPS: local frame only), 'sim_truth', 'vio' (e.g. cuVSLAM), ..."""
        self.write_meta(position={"source": source, "global": bool(global_ok), **info})

    def set_estimators(self, **est):
        """Constants of the range estimates (head_height_m, body_height_m, head_model, ...)."""
        self.est.update(est)
        self.write_meta(estimators=dict(self.est))

    def _latlon(self, n, e):
        if not self.origin or self.origin[0] is None:
            return "", ""
        la, lo = ned_to_latlon(n, e, self.origin[0], self.origin[1])
        return f"{la:.7f}", f"{lo:.7f}"

    def set_camera(self, width, height, fx, fy, cx, cy, **mount):
        """Intrinsics of the FULL resolution image (sighting boxes are in these pixels)."""
        cam = {"width": int(width), "height": int(height), "fx": fx, "fy": fy, "cx": cx, "cy": cy}
        cam.update(mount)
        if self.meta.get("camera") != cam:
            self.write_meta(camera=cam)

    # --- samples ----------------------------------------------------------------------------

    def track(self, t, p_ned, R_ned, src="", eph=None, epv=None):
        """Body position (local NED) + FRD -> NED rotation. Rate-limited to track_hz.
        src/eph/epv: position source and its 1-sigma horizontal/vertical accuracy (m)."""
        with self.lock:
            if t - self.last_track_t < self.track_dt - 1e-6 or self.origin is None:
                return
            self.last_track_t = t
            if self.t0 is None:
                self._rel_locked(t)
            tr = t - self.t0
            lat, lon = self._latlon(p_ned[0], p_ned[1])
            r, p, y = euler_ned(R_ned)
            q = lambda v: "" if v is None or not math.isfinite(v) else _f(v)
            self._row("track", [_f(tr, 2), lat, lon, _f(self.origin[2] - p_ned[2]),
                                _f(p_ned[1]), _f(p_ned[0]), _f(-p_ned[2]), _f(r, 1), _f(p, 1), _f(y, 1),
                                src, q(eph), q(epv)])
            self.n_track += 1

    def _rel_locked(self, t):
        self.t0 = t
        self.meta["t0"] = t  # the logging clock's value at t = 0
        self.meta["t0_wall"] = dt.datetime.now().astimezone().isoformat(timespec="milliseconds")
        return 0.0

    def frame(self, t, rgb, p_ned, R_ned, cam_ned, R_cam_ned, force=False):
        """Save a camera frame (RGB uint8 HxWx3) if the sampling rules want it, or always with
        force=True (frames of sightings). Returns the frame id or None.
        p_ned/R_ned: body; cam_ned/R_cam_ned: camera centre and camera FRD -> NED."""
        with self.lock:
            if t in self.frame_by_t:
                return self.frame_by_t[t]
            if self.t0 is None:
                self._rel_locked(t)
            if not force:
                if self.n_frames >= self.max_frames or t - self.last_try_t < self.frame_every - 1e-6:
                    return None
                self.last_try_t = t  # a skipped try still waits frame_every for the next one
                if self.last_kept is not None:
                    p0, f0 = self.last_kept
                    fwd = R_cam_ned[:, 0]
                    turn = math.degrees(math.acos(max(-1.0, min(1.0, float(np.dot(fwd, f0))))))
                    if np.linalg.norm(np.asarray(p_ned) - p0) < self.min_move and turn < self.min_turn:
                        return None
            elif self.n_frames >= self.max_frames * 2:  # hard cap incl. sighting frames
                return None
            fid = self.n_frames
            self.n_frames += 1
            self.frame_by_t[t] = fid
            if len(self.frame_by_t) > 64:
                self.frame_by_t.pop(next(iter(self.frame_by_t)))
            if not force:
                self.last_kept = (np.array(p_ned, float), np.array(R_cam_ned[:, 0], float))
            tr = t - self.t0
        name = f"frames/f{fid:05d}.jpg"
        self._save_jpg(rgb, os.path.join(self.out, name), self.frame_width)
        r, p, y = euler_ned(R_ned)
        ce, q = cam_pose_enu(cam_ned, R_cam_ned)
        with self.lock:
            self._row("frames", [fid, _f(tr), name, _f(p_ned[1]), _f(p_ned[0]), _f(-p_ned[2]),
                                 _f(r, 1), _f(p, 1), _f(y, 1), *(_f(v) for v in ce),
                                 *(_f(v, 5) for v in q), "sighting" if force else "sample"])
        return fid

    def _save_jpg(self, rgb, path, width):
        from PIL import Image
        im = Image.fromarray(np.ascontiguousarray(rgb))
        if width and im.width > width:
            im = im.resize((width, round(im.height * width / im.width)), Image.BILINEAR)
        im.save(path, quality=self.q)

    def _ray(self, R_enu, u, v):
        c = self.meta.get("camera") or {}
        if not c:
            return None
        d = R_enu @ np.array([(u - c["cx"]) / c["fx"], (v - c["cy"]) / c["fy"], 1.0])
        return d / np.linalg.norm(d)

    def sighting(self, t, person, method, conf, box, p_ned, range_m, rgb=None, frame=None,
                 cam_ned=None, R_cam_ned=None, head=None, d_depth=None, ground=None):
        """One detection. box = (u0, v0, u1, v1) full-resolution pixels; p_ned = the flight's
        estimate (method, range_m). With rgb: mean colour of the inner 50 % of the box + a small crop.
        Raw observations for re-solving later (format v2):
          head    (u0, v0, u1, v1, conf) of the head box in full-resolution pixels, or None
          d_depth depth-camera range to the person (box centre ray), m, or None
          ground  (p_ned, range_m) where the feet ray hits the flight's ground model, or None
        Rays (unit, local ENU) through the box bottom centre (feet), box centre and head centre,
        and the head / body-height distances are derived here from the camera intrinsics."""
        with self.lock:  # crop number reserved up front (sightings may come from several threads)
            k = self.n_sight
            self.n_sight += 1
            if self.t0 is None:
                self._rel_locked(t)
        u0, v0, u1, v1 = box
        col, crop = ("", "", ""), ""
        if rgb is not None:
            h, w = rgb.shape[:2]
            du, dv = (u1 - u0) / 4, (v1 - v0) / 4
            a = rgb[max(0, int(v0 + dv)):min(h, int(math.ceil(v1 - dv))),
                    max(0, int(u0 + du)):min(w, int(math.ceil(u1 - du)))]
            if a.size:
                col = tuple(str(int(round(c))) for c in a.reshape(-1, 3).mean(0))
            pu, pv = (u1 - u0) * 0.3 + 4, (v1 - v0) * 0.3 + 4
            c = rgb[max(0, int(v0 - pv)):min(h, int(v1 + pv)), max(0, int(u0 - pu)):min(w, int(u1 + pu))]
            if c.size:
                crop = f"crops/s{k:05d}.jpg"
                from PIL import Image
                im = Image.fromarray(np.ascontiguousarray(c))
                if max(im.size) > 160:
                    s = 160 / max(im.size)
                    im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.BILINEAR)
                elif max(im.size) < 96:  # tiny boxes far away: upscale for the panel
                    s = 96 / max(im.size)
                    im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.NEAREST)
                im.save(os.path.join(self.out, crop), quality=85)
        cam = ("",) * 7
        v2 = {}
        fy = (self.meta.get("camera") or {}).get("fy")
        if cam_ned is not None and R_cam_ned is not None:
            ce, q = cam_pose_enu(cam_ned, R_cam_ned)
            cam = (*(_f(v) for v in ce), *(_f(v, 5) for v in q))
            v2["cam_lat"], v2["cam_lon"] = self._latlon(cam_ned[0], cam_ned[1])
            if self.origin:
                v2["cam_alt"] = _f(self.origin[2] - cam_ned[2])
            Re = cam_rot_enu(R_cam_ned)
            rays = {"f": ((u0 + u1) / 2, v1), "c": ((u0 + u1) / 2, (v0 + v1) / 2)}
            if head is not None:
                rays["h"] = ((head[0] + head[2]) / 2, (head[1] + head[3]) / 2)
            for key, (uu, vv) in rays.items():
                d = self._ray(Re, uu, vv)
                if d is not None:
                    for ax, val in zip("enu", d):
                        v2[f"ray_{key}_{ax}"] = _f(val, 5)
        if fy and v1 - v0 >= 1:
            v2["d_body"] = _f(self.est["body_height_m"] * fy / (v1 - v0))
        if head is not None:
            hp = max(1e-3, head[3] - head[1])
            v2.update({"head_u0": _f(head[0], 1), "head_v0": _f(head[1], 1), "head_u1": _f(head[2], 1),
                       "head_v1": _f(head[3], 1), "head_conf": _f(head[4]), "head_px": _f(hp, 1)})
            if fy:
                v2["d_head"] = _f(self.est["head_height_m"] * fy / hp)
        if d_depth is not None and math.isfinite(d_depth):
            v2["d_depth"] = _f(d_depth)
        if ground is not None:
            g, gr = ground
            v2.update({"gnd_e": _f(g[1]), "gnd_n": _f(g[0]), "gnd_u": _f(-g[2]), "d_gnd": _f(gr)})
        lat, lon = self._latlon(p_ned[0], p_ned[1])
        with self.lock:
            self._row("sightings", [_f(t - self.t0), person, method, _f(conf), *(f"{v:.0f}" for v in box),
                                    _f(p_ned[1]), _f(p_ned[0]), _f(-p_ned[2]), lat, lon, _f(range_m, 1),
                                    *col, "" if frame is None else frame, crop, *cam,
                                    *(v2.get(c, "") for c in SIGHT_V2)])

    def write_csv(self, name, rows, header=None):
        """Whole-file tables written once (persons.csv, truth.csv): list of dicts."""
        header = header or (list(rows[0].keys()) if rows else [])
        with open(os.path.join(self.out, f"{name}.csv"), "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=header, extrasaction="ignore", lineterminator="\n")
            w.writeheader()
            for r in rows:
                w.writerow(r)

    def close(self, persons=None, **meta):
        if persons is not None:
            self.write_csv("persons", persons, ["person", "n_sight", "conf_max", "e", "n", "u", "lat", "lon",
                                                "t_first", "t_last", "pinged", "truth", "err_m"])
        with self.lock:
            for f in self.files.values():
                f.close()
            self.files = {}
            self.meta["end_time"] = dt.datetime.now().astimezone().isoformat(timespec="seconds")
            self.meta["duration_s"] = round(self.last_track_t - self.t0, 2) if self.t0 is not None else 0
        self.write_meta(**meta)
        if self.keep:
            stamp = self.meta["start_time"][:19].replace("-", "").replace(":", "").replace("T", "_")
            name = stamp + (f"_{self.meta['world']}" if self.meta.get("world") else "")
            dst = os.path.join(os.path.dirname(self.out), name)
            shutil.copytree(self.out, dst, dirs_exist_ok=True)
            return dst
        return self.out
