#!/usr/bin/env python3
"""Throwaway test: fly a lawnmower path, detect people with stock YOLO and log where they are.

Delete the whole scripts/search_test/ folder when it is no longer useful; nothing else uses it.

    # 1. sim without any controller (the C++ core or `offboard` would fight this script over PX4)
    ros2 launch cogidrone_bringup sim.launch.py world:=sar headless:=true autonomy:=false
    # 2. this script (takes off, flies the path, lands, prints the summary)
    python3 scripts/search_test/search_test.py --world sar --area 20,-40,140,60 --alt 15

One window (--show, on when $DISPLAY is set): the drone camera with the YOLO boxes and a status
line (height, speed, pings, camera rate, real-time factor). The same picture is published on
/search_test/image. YOLO always works on the newest frame and drops the rest, so it never lags.

Every sighting goes to sightings.csv, sightings within --cluster-m are merged into persons
(running mean) in persons.csv, and with --world the persons are compared against the world's
<world>_people.yaml. Output: ~/search_test_runs/<time>_<world>/.

Flight run-log for the 3D viewer (tools/flight_viewer, format in its README.md): track, sampled
camera frames, every sighting with its frame/crop/colour/camera pose, persons and the sim truth go
to --log-dir (default ~/flight_runs/latest, replaced every run; --keep also keeps a timestamped
copy next to it). View it with:  python3 tools/flight_viewer/view.py

Frames (careful, three of them):
  PX4 local NED   x north, y east, z down, origin = where the EKF started (the launch spot).
  Body FRD        x forward, y right, z down. VehicleAttitude.q rotates FRD -> NED.
  Camera optical  x right, y down, z forward (pixels). In FRD terms a pixel ray is
                  (1, (u-cx)/fx, (v-cy)/fy) before the mount pitch.
  Gazebo world    ENU, x east, y north. The drone spawns at the world origin, so
                  world (x, y) ~= local (y_ned, x_ned); for the evaluation we go through
                  lat/lon instead, which also catches an EKF origin that is not the spawn spot.

Person position, per detection:
  depth   median of the depth image over the middle of the box (<= --depth-max m) along the
          ray through the box centre.
  ground  else the ray through the box bottom-centre (the feet) hits the ground. The ground is
          flat at the takeoff height (minus --body-height, the IMU over the ground), or the
          real terrain with --dem bymarka (Kartverket DTM the Bymarka worlds are built from).
"""

import argparse
import bisect
import csv
import datetime as dt
import json
import math
import os
import re
import subprocess
import sys
import threading
import time

import numpy as np
import rclpy
from geometry_msgs.msg import PointStamped
from px4_msgs.msg import (
    OffboardControlMode,
    TrajectorySetpoint,
    VehicleAttitude,
    VehicleCommand,
    VehicleLocalPosition,
    VehicleStatus,
)
from rclpy.callback_groups import MutuallyExclusiveCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    HistoryPolicy,
    QoSProfile,
    ReliabilityPolicy,
    qos_profile_sensor_data,
)
from sensor_msgs.msg import CameraInfo, Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools", "flight_viewer"))
from runlog import RunLog

EARTH_R = 6371000.0  # PX4's map projection radius
PX4_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1,
)


# ---------------------------------------------------------------------------------------------
# Geometry


def quat_to_rot(q):
    """PX4 quaternion (w, x, y, z), FRD -> NED, as a 3x3 matrix."""
    w, x, y, z = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
        [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
        [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)],
    ])


def mount_rot(pitch_down_deg):
    """Camera FRD -> body FRD for a camera tilted pitch_down_deg below the nose."""
    a = math.radians(-pitch_down_deg)  # rotation about body y (right); positive = nose up
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def pixel_ray(u, v, K):
    """Unnormalised ray in camera FRD whose forward component is 1 (= planar depth of 1 m)."""
    fx, fy, cx, cy = K
    return np.array([1.0, (u - cx) / fx, (v - cy) / fy])


def ned_to_latlon(x, y, lat0, lon0):
    """PX4's azimuthal equidistant map projection (lib/geo), local NED -> lat/lon degrees."""
    la0, lo0 = math.radians(lat0), math.radians(lon0)
    xr, yr = x / EARTH_R, y / EARTH_R
    c = math.hypot(xr, yr)
    if c == 0:
        return lat0, lon0
    sc, cc = math.sin(c), math.cos(c)
    lat = math.asin(cc * math.sin(la0) + xr * sc * math.cos(la0) / c)
    lon = lo0 + math.atan2(yr * sc, c * math.cos(la0) * cc - xr * math.sin(la0) * sc)
    return math.degrees(lat), math.degrees(lon)


def latlon_to_ned(lat, lon, lat0, lon0):
    """Inverse of ned_to_latlon (PX4 map_projection_project)."""
    la, lo, la0, lo0 = map(math.radians, (lat, lon, lat0, lon0))
    cos_c = math.sin(la0) * math.sin(la) + math.cos(la0) * math.cos(la) * math.cos(lo - lo0)
    cos_c = max(-1.0, min(1.0, cos_c))
    c = math.acos(cos_c)
    k = 1.0 if c == 0 else c / math.sin(c)
    x = k * (math.cos(la0) * math.sin(la) - math.sin(la0) * math.cos(la) * math.cos(lo - lo0)) * EARTH_R
    y = k * math.cos(la) * math.sin(lo - lo0) * EARTH_R
    return x, y


class FlatGround:
    def __init__(self, z_ned):
        self.z_ned = z_ned

    def hit(self, origin, d, max_range):
        """Intersection of origin + t*d with the plane z = z_ned (NED), or None."""
        if d[2] <= 1e-6:
            return None  # ray at or above the horizon
        t = (self.z_ned - origin[2]) / d[2]
        p = origin + t * d
        return p if 0 < np.linalg.norm(p - origin) <= max_range else None


class DemGround:
    """Bymarka worlds only: Kartverket 1 m DTM (the cached file make_world.py built the terrain
    from). World z = DTM - origin elevation; local NED -> world through the launch spot."""

    def __init__(self, world_sdf, home_ned, body_height):
        sys.path.insert(0, os.path.join(REPO, "sim", "tools", "bymarka"))
        import data
        from PIL import Image as PILImage

        lat, lon, elev = world_origin(world_sdf)
        self.oe, self.on = data.latlon_to_utm(lat, lon)
        self.oz = elev
        path = os.path.join(data.CACHE, "dtm_562900_7032500_564900_7034500_1m.tif")
        PILImage.MAX_IMAGE_PIXELS = None
        self.dtm = np.array(PILImage.open(path), np.float32)
        self.e0, self.n1 = 562900.0, 7034500.0
        # Local NED z of the world's z = 0 (ground at the launch spot).
        self.home = home_ned
        self.z0_ned = home_ned[2] + body_height

    def height_ned(self, x, y):
        """Ground z (NED) under local NED (x north, y east)."""
        e = self.oe + (y - self.home[1])
        n = self.on + (x - self.home[0])
        c, r = e - self.e0 - 0.5, self.n1 - n - 0.5
        r0, c0 = int(math.floor(r)), int(math.floor(c))
        if not (0 <= r0 < self.dtm.shape[0] - 1 and 0 <= c0 < self.dtm.shape[1] - 1):
            return None
        fr, fc = r - r0, c - c0
        h = (self.dtm[r0, c0] * (1 - fr) * (1 - fc) + self.dtm[r0, c0 + 1] * (1 - fr) * fc
             + self.dtm[r0 + 1, c0] * fr * (1 - fc) + self.dtm[r0 + 1, c0 + 1] * fr * fc)
        return self.z0_ned - (float(h) - self.oz)

    def hit(self, origin, d, max_range):
        d = d / np.linalg.norm(d)
        step, t, prev = 1.0, 0.0, None
        while t <= max_range:
            p = origin + t * d
            g = self.height_ned(p[0], p[1])
            if g is None:
                return None
            if p[2] >= g:  # below the ground (NED: larger z = lower)
                if prev is None:
                    return None
                lo, hi = prev, t
                for _ in range(20):
                    mid = 0.5 * (lo + hi)
                    q = origin + mid * d
                    if q[2] >= self.height_ned(q[0], q[1]):
                        hi = mid
                    else:
                        lo = mid
                return origin + hi * d
            prev = t
            t += step
        return None


def world_origin(sdf_path):
    s = open(sdf_path).read()
    get = lambda tag: float(re.search(rf"<{tag}>([^<]+)</{tag}>", s).group(1))
    return get("latitude_deg"), get("longitude_deg"), get("elevation")


# ---------------------------------------------------------------------------------------------
# Path


def lawnmower(x0, y0, x1, y1, spacing):
    """Waypoints (east, north) in local ENU metres, legs along north."""
    pts, e, k = [], x0, 0
    while e <= x1 + 1e-6:
        a, b = (y0, y1) if k % 2 == 0 else (y1, y0)
        pts += [(e, a), (e, b)]
        e += spacing
        k += 1
    return pts


# ---------------------------------------------------------------------------------------------
# Node


class SearchTest(Node):
    def __init__(self, a, out):
        super().__init__("search_test")
        self.a, self.out = a, out
        self.lock = threading.Lock()
        ctl, img = MutuallyExclusiveCallbackGroup(), MutuallyExclusiveCallbackGroup()
        self.pos = self.att = self.status = None
        self.pose_buf = []  # (t_s, x, y, z, q) for time matching
        self.att_buf = []
        self.K = self.K_depth = None
        self.depth = None  # (stamp_s, array)
        self.home = None
        self.ground = None
        self.R_mount = mount_rot(a.cam_pitch)
        # Camera centres in body FRD (color imager 15 mm right of the depth origin).
        dz = -(a.cam_z - a.base_z)
        self.depth_off = np.array([a.cam_x, 0.0, dz])
        self.color_off = np.array([a.cam_x, 0.015, dz])
        self.sightings, self.persons = 0, []
        # Sim only: where the true people are, to tell "not in view" from "YOLO missed it".
        self.truth = load_truth(a.truth) if a.truth else []
        self.vis = {}  # truth id -> {"frames", "detected", "best_range", ...}

        self.mode_pub = self.create_publisher(OffboardControlMode, "/fmu/in/offboard_control_mode", PX4_QOS)
        self.sp_pub = self.create_publisher(TrajectorySetpoint, "/fmu/in/trajectory_setpoint", PX4_QOS)
        self.cmd_pub = self.create_publisher(VehicleCommand, "/fmu/in/vehicle_command", PX4_QOS)
        self.ping_pub = self.create_publisher(PointStamped, "/search_test/person_pings", 10)
        sub = self.create_subscription
        sub(VehicleLocalPosition, "/fmu/out/vehicle_local_position_v1", self.on_pos, PX4_QOS, callback_group=ctl)
        sub(VehicleAttitude, "/fmu/out/vehicle_attitude", self.on_att, PX4_QOS, callback_group=ctl)
        sub(VehicleStatus, "/fmu/out/vehicle_status_v1", self.on_status, PX4_QOS, callback_group=ctl)
        sub(CameraInfo, a.info_topic, self.on_info, qos_profile_sensor_data, callback_group=ctl)
        sub(CameraInfo, a.depth_info_topic, self.on_depth_info, qos_profile_sensor_data, callback_group=ctl)
        sub(Image, a.depth_topic, self.on_depth, QoSProfile(reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST, depth=1), callback_group=ctl)
        # Reliable, newest frame only. Best effort lost ~1/3 of the 720p frames: a 2.7 MB image is
        # ~40 UDP fragments and one lost fragment drops the whole frame (21 vs 30 Hz measured).
        latest = QoSProfile(reliability=ReliabilityPolicy.RELIABLE, history=HistoryPolicy.KEEP_LAST, depth=1)
        sub(Image, a.image_topic, self.on_image, latest, callback_group=img)
        self.view_pub = self.create_publisher(Image, "/search_test/image", latest)
        # Newest camera frame for the YOLO thread; the annotated frame for the window.
        self.frame, self.frame_ev = None, threading.Event()
        self.view, self.view_new = None, False
        self.cam_t = []  # (wall s, stamp s) of received frames, for camera Hz and RTF
        self.rate_log, self.t_rate = [], 0.0  # (cam Hz, RTF, YOLO fps) once a second on the path
        self.yolo_t = []

        from ultralytics import YOLO
        self.yolo = YOLO(a.model)
        self.yolo.predict(np.zeros((480, 640, 3), np.uint8), device=a.device, verbose=False)  # warm up
        self.stop = False
        self.worker = threading.Thread(target=self.yolo_loop, daemon=True)
        self.worker.start()
        # Head detector (Muhammad's depth_head.py: head size -> distance) on the person crops, in
        # its own thread with a short queue so it never stalls the YOLO loop (full queue = the
        # sighting is logged without a head).
        self.head, self.head_q, self.head_ms, self.head_n = None, None, [], [0, 0, 0]  # tried, found, skipped
        if a.head_model and os.path.exists(a.head_model):
            import queue
            self.head = YOLO(a.head_model)
            self.head.predict(np.zeros((160, 96, 3), np.uint8), imgsz=a.head_imgsz, device=a.device, verbose=False)
            self.head_q = queue.Queue(maxsize=8)
            self.head_worker = threading.Thread(target=self.head_loop, daemon=True)
            self.head_worker.start()
        elif a.head_model:
            self.log(f"head model {a.head_model} not found: no head distances")

        self.rl = None
        if a.runlog:
            self.rl = RunLog(a.log_dir, keep=a.keep, track_hz=a.track_hz, frame_every=a.frame_every,
                             frame_min_move=a.frame_min_move, frame_min_turn=a.frame_min_turn,
                             max_frames=a.max_frames, frame_width=a.frame_width,
                             meta={"source": "sim" if a.world else "real", "world": a.world,
                                   "tool": "scripts/search_test/search_test.py", "search_test_out": out,
                                   "args": {k: v for k, v in vars(a).items() if isinstance(v, (int, float, str, list))}})
            self.rl.set_estimators(head_height_m=a.head_height_m, body_height_m=a.body_height_m,
                                   head_model=a.head_model if self.head else "", head_conf=a.head_conf,
                                   head_every=a.head_every, head_min_px=a.head_min_px, depth_max=a.depth_max)
            self.log(f"run-log for the viewer: {self.rl.out}")
        self.f_sight = open(os.path.join(out, "sightings.csv"), "w", newline="")
        self.w_sight = csv.writer(self.f_sight)
        self.w_sight.writerow([
            "t_s", "person", "method", "conf", "u0", "v0", "u1", "v1",
            "drone_n", "drone_e", "drone_d", "drone_yaw_deg",
            "n", "e", "d", "lat", "lon", "range_m",
        ])

        # Flight
        self.wps = ([tuple(map(float, w.split(","))) for w in a.route.split(";")] if a.route
                    else lawnmower(*a.area, a.spacing))
        self.state, self.t_state = "WAIT", time.time()
        self.wp_i, self.carrot = 0, None
        self.done = False
        self.create_timer(0.1, self.tick, callback_group=ctl)
        self.log(f"path: {len(self.wps)} waypoints over area {a.area} at {a.alt} m, {a.speed} m/s; "
                 f"camera pitch {a.cam_pitch} deg down")

    def log(self, s):
        self.get_logger().info(s)

    def start_runlog(self, p):
        """Origin, takeoff spot and (sim) the true people for the viewer's run-log."""
        a, rl = self.a, self.rl
        # PX4 local NED + its reference: lat/lon only with a global position (GPS). Without one
        # (no GPS, or later a VIO pose) the log stays in the local metric frame.
        if p.xy_global:
            rl.set_origin(p.ref_lat, p.ref_lon, p.ref_alt if p.z_global else 0.0)
        else:
            rl.set_origin(None, None, 0.0)
        rl.set_position_source("px4_ekf_gps" if p.xy_global else "px4_ekf_local", bool(p.xy_global),
                               topic="/fmu/out/vehicle_local_position_v1",
                               note="sim: PX4 EKF on simulated GPS" if a.world else "")
        extra = {"takeoff": {"e": round(p.y, 3), "n": round(p.x, 3), "u": round(-p.z, 3)},
                 "ground_u": round(-(p.z + a.body_height), 3)}
        if a.world_sdf:
            lat, lon, elev = world_origin(a.world_sdf)
            extra["world_origin"] = {"lat": lat, "lon": lon, "alt": elev}
        rl.write_meta(**extra)
        if self.truth:
            rows = []
            for t in self.truth:
                q = self.truth_ned(t)
                rows.append({"id": t["id"], "state": t["state"], "zone": t["zone"], "e": round(q[1], 2),
                             "n": round(q[0], 2), "u": round(-(q[2] + 0.4), 2)})
            rl.write_csv("truth", rows, ["id", "state", "zone", "e", "n", "u"])

    # --- inputs ---------------------------------------------------------------------------------

    def on_pos(self, m):
        self.pos = m
        with self.lock:
            self.pose_buf.append((m.timestamp * 1e-6, m.x, m.y, m.z))
            del self.pose_buf[:-400]
        if self.rl is not None and self.home is not None and self.att is not None:
            self.rl.track(m.timestamp * 1e-6, (m.x, m.y, m.z), quat_to_rot(np.array(self.att.q, float)),
                          src="px4_ekf_gps" if m.xy_global else "px4_ekf_local", eph=m.eph, epv=m.epv)

    def on_att(self, m):
        self.att = m
        with self.lock:
            self.att_buf.append((m.timestamp * 1e-6, np.array(m.q, float)))
            del self.att_buf[:-400]

    def on_status(self, m):
        self.status = m

    def on_info(self, m):
        self.K = (m.k[0], m.k[4], m.k[2], m.k[5])

    def on_depth_info(self, m):
        self.K_depth = (m.k[0], m.k[4], m.k[2], m.k[5])

    def on_depth(self, m):
        if m.encoding == "32FC1":
            a = np.frombuffer(m.data, np.float32).reshape(m.height, m.width)
        elif m.encoding == "16UC1":  # real D435i: mm
            a = np.frombuffer(m.data, np.uint16).reshape(m.height, m.width).astype(np.float32) / 1000.0
            a[a == 0] = np.nan
        else:
            return
        self.depth = (m.header.stamp.sec + m.header.stamp.nanosec * 1e-9, a)

    def pose_at(self, t_img):
        """(position NED, R body->NED, matched) at the image time. In SITL the PX4 clock and the
        image stamps are both sim time; if they are not comparable, use the latest pose."""
        with self.lock:
            pb, ab = list(self.pose_buf), list(self.att_buf)
        if not pb or not ab:
            return None
        if abs(pb[-1][0] - t_img) > 1.0:
            p, q = pb[-1], ab[-1][1]
            self.pose_t = p[0]  # PX4 time of the pose used (the run-log's clock)
            return np.array(p[1:4]), quat_to_rot(q), False

        def interp(buf, t, f):
            ts = [b[0] for b in buf]
            i = bisect.bisect_left(ts, t)
            if i <= 0:
                return f(buf[0], buf[0], 0)
            if i >= len(buf):
                return f(buf[-1], buf[-1], 0)
            a, b = buf[i - 1], buf[i]
            w = (t - a[0]) / max(b[0] - a[0], 1e-9)
            return f(a, b, w)

        p = interp(pb, t_img, lambda a, b, w: (1 - w) * np.array(a[1:4]) + w * np.array(b[1:4]))
        q = interp(ab, t_img, lambda a, b, w: ((1 - w) * a[1] + w * (b[1] if np.dot(a[1], b[1]) >= 0 else -b[1])))
        self.pose_t = t_img
        return p, quat_to_rot(q / np.linalg.norm(q)), True

    # --- perception -----------------------------------------------------------------------------

    def on_image(self, m):
        t_img = m.header.stamp.sec + m.header.stamp.nanosec * 1e-9
        with self.lock:
            self.frame = m
            self.cam_t.append((time.monotonic(), t_img))
            del self.cam_t[:-60]
        self.frame_ev.set()

    def yolo_loop(self):
        """YOLO on the newest frame, frames that arrive meanwhile are dropped."""
        while not self.stop:
            if not self.frame_ev.wait(0.2):
                continue
            with self.lock:
                m, self.frame = self.frame, None
                self.frame_ev.clear()
            if m is None:
                continue
            try:
                self.process(m)
            except Exception as ex:  # keep flying if one frame fails
                self.get_logger().error(f"frame failed: {ex}", throttle_duration_sec=5.0)

    def rates(self):
        """Camera rate (frames per wall second), real-time factor and YOLO rate over ~2 s."""
        with self.lock:
            ct, yt = list(self.cam_t), list(self.yolo_t)
        now = time.monotonic()
        ct = [c for c in ct if now - c[0] < 2.5]
        cam = rtf = yolo = 0.0
        if len(ct) >= 3 and ct[-1][0] > ct[0][0]:
            cam = (len(ct) - 1) / (ct[-1][0] - ct[0][0])
            rtf = (ct[-1][1] - ct[0][1]) / (ct[-1][0] - ct[0][0])
        yt = [t for t in yt if now - t < 2.5]
        if len(yt) >= 3:
            yolo = (len(yt) - 1) / (yt[-1] - yt[0])
        return cam, rtf, yolo

    def draw(self, bgr, boxes, confs):
        import cv2
        img = np.ascontiguousarray(bgr)
        for (u0, v0, u1, v1), c in zip(boxes, confs):
            p0, p1 = (int(u0), int(v0)), (int(u1), int(v1))
            cv2.rectangle(img, p0, p1, (0, 0, 255), 2)
            label = f"person {c:.2f}"
            (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 1)
            x, y = min(p0[0], img.shape[1] - tw - 6), max(p0[1], th + 40)  # keep it on screen
            cv2.rectangle(img, (x, y - th - 6), (x + tw + 6, y), (0, 0, 255), -1)
            cv2.putText(img, label, (x + 3, y - 4), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1,
                        cv2.LINE_AA)
        p = self.pos
        cam, rtf, yolo = self.rates()
        alt = (self.home[2] - p.z) if (p is not None and self.home is not None) else float("nan")
        spd = math.hypot(p.vx, p.vy) if p is not None else float("nan")
        pings = sum(1 for c in self.persons if c["pinged"])
        line = (f"{self.state}  alt {alt:5.1f} m  {spd:4.1f} m/s  pings {pings}  |  cam {cam:4.1f} Hz "
                f"{img.shape[1]}x{img.shape[0]}  RTF {rtf:4.2f}  YOLO {yolo:4.1f} fps")
        cv2.rectangle(img, (0, 0), (img.shape[1], 34), (0, 0, 0), -1)
        cv2.putText(img, line, (8, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.65, (255, 255, 255), 1, cv2.LINE_AA)
        return img

    def process(self, m):
        t_img = m.header.stamp.sec + m.header.stamp.nanosec * 1e-9
        rows = np.frombuffer(m.data, np.uint8).reshape(m.height, m.step)
        rgb = rows[:, : m.width * 3].reshape(m.height, m.width, 3)
        bgr = rgb[:, :, ::-1] if m.encoding == "rgb8" else rgb
        imgsz = self.a.imgsz or max(960, m.width)
        res = self.yolo.predict(bgr, classes=[0], conf=self.a.conf, imgsz=imgsz,
                                device=self.a.device, verbose=False)[0]
        with self.lock:
            self.yolo_t.append(time.monotonic())
            del self.yolo_t[:-60]
        boxes, confs = res.boxes.xyxy.tolist(), res.boxes.conf.tolist()
        if self.a.show or self.view_pub.get_subscription_count() > 0:
            view = self.draw(bgr, boxes, confs)
            if self.view_pub.get_subscription_count() > 0:
                out = Image()
                out.header = m.header
                out.height, out.width, out.encoding = view.shape[0], view.shape[1], "bgr8"
                out.step = out.width * 3
                out.data = view.tobytes()
                self.view_pub.publish(out)
            with self.lock:
                self.view, self.view_new = view, True
        if self.K is None or self.home is None:
            return
        pose = self.pose_at(t_img)
        if pose is None:
            return
        p_body, R, matched = pose
        t_log = self.pose_t
        Rc = R @ self.R_mount
        cam_ned = p_body + R @ self.color_off
        if self.rl is not None:
            self.rl.set_camera(m.width, m.height, *self.K, pitch_down_deg=self.a.cam_pitch,
                               offset_frd=[round(v, 3) for v in self.color_off])
            self.rl.frame(t_log, rgb, p_body, R, cam_ned, Rc)
        if self.state not in ("PATH", "RETURN"):
            return
        if self.truth:
            self.check_truth(p_body, R, boxes, rgb)
        if len(res.boxes) == 0:
            return
        depth = self.depth
        if depth is not None and abs(depth[0] - t_img) > 0.2:
            depth = None  # stale depth frame
        yaw = math.degrees(math.atan2(R[1, 0], R[0, 0]))
        for (u0, v0, u1, v1), conf in zip(boxes, confs):
            est = self.locate(u0, v0, u1, v1, p_body, R, depth)
            if est is None:
                continue
            p, method, rng, obs = est
            lat, lon = ned_to_latlon(p[0], p[1], self.pos.ref_lat, self.pos.ref_lon)
            pid, new = self.cluster(p, method, conf)
            self.sightings += 1
            self.w_sight.writerow([
                f"{t_img:.2f}", pid, method, f"{conf:.2f}", *(f"{v:.0f}" for v in (u0, v0, u1, v1)),
                *(f"{v:.2f}" for v in p_body), f"{yaw:.1f}",
                *(f"{v:.2f}" for v in p), f"{lat:.7f}", f"{lon:.7f}", f"{rng:.1f}",
            ])
            self.f_sight.flush()
            c = self.persons[pid]
            c.setdefault("t0_log", t_log)
            c["t1_log"] = t_log
            if self.rl is not None:
                fid = self.rl.frame(t_log, rgb, p_body, R, cam_ned, Rc, force=True)
                job = dict(t=t_log, person=pid, method=method, conf=conf, box=(u0, v0, u1, v1), p_ned=p,
                           range_m=rng, rgb=rgb, frame=fid, cam_ned=cam_ned, R_cam_ned=Rc,
                           d_depth=obs.get("d_depth"), ground=obs.get("ground"))
                self.log_sighting(job)
            if new:
                self.ping(pid, c, rgb, (u0, v0, u1, v1), conf)
            msg = PointStamped()
            msg.header.stamp = m.header.stamp
            msg.header.frame_id = "local_enu"
            msg.point.x, msg.point.y, msg.point.z = float(c["e"]), float(c["n"]), float(-c["d"])
            self.ping_pub.publish(msg)

    # --- head distance (run-log only) -------------------------------------------------------------

    def log_sighting(self, job):
        """Run-log row now, or after the head detector if it wants this sighting."""
        a, box = self.a, job["box"]
        want = (self.head_q is not None and box[3] - box[1] >= a.head_min_px
                and (self.sightings - 1) % max(1, a.head_every) == 0)
        if want:
            try:
                self.head_q.put_nowait(job)
                return
            except Exception:  # queue full: no head for this one
                self.head_n[2] += 1
        self.rl.sighting(**job)

    def head_loop(self):
        while True:
            job = self.head_q.get()
            if job is None:
                return
            try:
                job["head"] = self.find_head(job["rgb"], job["box"])
            except Exception as ex:
                self.get_logger().error(f"head detector failed: {ex}", throttle_duration_sec=5.0)
            rl = self.rl
            if rl is not None:
                rl.sighting(**job)

    def find_head(self, rgb, box):
        """Best head in the upper 60 % of the person box -> (u0, v0, u1, v1, conf) full-res px."""
        a = self.a
        u0, v0, u1, v1 = box
        h, w = rgb.shape[:2]
        pu, pv = (u1 - u0) * 0.3 + 4, (v1 - v0) * 0.3 + 4  # same crop as the run-log crops (heads.py)
        cu0, cv0 = max(0, int(u0 - pu)), max(0, int(v0 - pv))
        crop = np.ascontiguousarray(rgb[cv0:min(h, int(v1 + pv)), cu0:min(w, int(u1 + pu))][:, :, ::-1])
        t0 = time.monotonic()
        r = self.head.predict(crop, imgsz=a.head_imgsz, conf=a.head_conf, device=a.device, verbose=False)[0]
        self.head_ms.append((time.monotonic() - t0) * 1000)
        del self.head_ms[:-200]
        self.head_n[0] += 1
        best = None
        for (x0, y0, x1, y1), c in zip(r.boxes.xyxy.tolist(), r.boxes.conf.tolist()):
            gu, gv = cu0 + (x0 + x1) / 2, cv0 + (y0 + y1) / 2
            if u0 - 2 <= gu <= u1 + 2 and v0 - 2 <= gv <= v0 + 0.6 * (v1 - v0) and (best is None or c > best[4]):
                best = (cu0 + x0, cv0 + y0, cu0 + x1, cv0 + y1, c)
        self.head_n[1] += best is not None
        return best

    def flush_heads(self, timeout=5.0):
        """Let the head thread finish its queue (before the run-log is closed)."""
        if self.head_q is None:
            return
        t_end = time.monotonic() + timeout
        while not self.head_q.empty() and time.monotonic() < t_end:
            time.sleep(0.05)
        self.head_q.put(None)
        self.head_worker.join(timeout=max(0.1, t_end - time.monotonic()))
        if self.head_n[0] or self.head_n[2]:
            ms = sorted(self.head_ms) or [0]
            self.log(f"head detector: {self.head_n[0]} crops, head found in {self.head_n[1]}, "
                     f"{self.head_n[2]} skipped (queue full), {ms[len(ms) // 2]:.0f} ms median")
        self.head_q = None

    def truth_ned(self, t):
        """A true person (world ENU) in local NED, through the world origin's lat/lon."""
        if not hasattr(self, "_w0"):
            lat0w, lon0w, _ = world_origin(self.a.world_sdf)
            self._w0 = latlon_to_ned(lat0w, lon0w, self.pos.ref_lat, self.pos.ref_lon)
        x, y = t["pts"][0]
        # world z = 0 is the ground at the spawn spot; the person's middle ~0.4 m over its z
        d = self.home[2] + self.a.body_height - t["z"] - 0.4
        return np.array([self._w0[0] + y, self._w0[1] + x, d])

    def check_truth(self, p_body, R, boxes, rgb):
        Rc = R @ self.R_mount
        origin = p_body + R @ self.color_off
        fx, fy, cx, cy = self.K
        for t in self.truth:
            v = Rc.T @ (self.truth_ned(t) - origin)  # camera FRD
            rng = float(np.linalg.norm(v))
            if v[0] <= 0.5 or rng > self.a.max_range:
                continue
            u, w = cx + fx * v[1] / v[0], cy + fy * v[2] / v[0]
            if not (0 <= u < 2 * cx and 0 <= w < 2 * cy):
                continue
            hit = any(u0 - 15 <= u <= u1 + 15 and v0 - 15 <= w <= v1 + 15 for u0, v0, u1, v1 in boxes)
            s = self.vis.setdefault(t["id"], {"frames": 0, "detected": 0, "best_range": 1e9})
            s["frames"] += 1
            s["detected"] += hit
            if rng < s["best_range"]:  # keep a picture of the closest view
                s["best_range"] = rng
                s["img"], s["uv"], s["boxes"], s["hit"] = rgb.copy(), (u, w), boxes, hit

    def save_truth_views(self):
        from PIL import Image as PILImage
        from PIL import ImageDraw
        for tid, s in self.vis.items():
            im = PILImage.fromarray(np.ascontiguousarray(s.pop("img")))
            dr = ImageDraw.Draw(im)
            for b in s.pop("boxes"):
                dr.rectangle(b, outline=(255, 0, 0), width=2)
            u, w = s.pop("uv")
            dr.ellipse((u - 12, w - 12, u + 12, w + 12), outline=(0, 255, 255), width=2)
            im.save(os.path.join(self.out, f"truth_{tid}.jpg"), quality=85)

    def locate(self, u0, v0, u1, v1, p_body, R, depth):
        """-> (position NED, method, range, observations) or None. Depth first (if valid), else
        the feet ray on the ground; observations = {d_depth, ground: (p, range)} for the run-log."""
        a = self.a
        Rc = R @ self.R_mount  # camera FRD -> NED
        obs, best = {}, None
        # 1) depth along the ray through the box centre
        if depth is not None and self.K_depth is not None:
            fx, fy, cx, cy = self.K
            fxd, fyd, cxd, cyd = self.K_depth
            img = depth[1]
            # middle half of the box, mapped into the depth image by direction (parallax of the
            # 15 mm baseline is far below a pixel at these ranges)
            du, dv = (u1 - u0) / 4, (v1 - v0) / 4
            uu = [cxd + fxd * ((u - cx) / fx) for u in (u0 + du, u1 - du)]
            vv = [cyd + fyd * ((v - cy) / fy) for v in (v0 + dv, v1 - dv)]
            c0, c1 = max(0, int(uu[0])), min(img.shape[1], int(math.ceil(uu[1])) + 1)
            r0, r1 = max(0, int(vv[0])), min(img.shape[0], int(math.ceil(vv[1])) + 1)
            patch = img[r0:r1, c0:c1]
            vals = patch[np.isfinite(patch) & (patch > 0.3) & (patch <= a.depth_max)]
            if patch.size and vals.size >= max(3, 0.3 * patch.size):
                z = float(np.median(vals))
                d_cam = pixel_ray((u0 + u1) / 2, (v0 + v1) / 2, self.K) * z
                p = p_body + R @ self.depth_off + Rc @ d_cam
                # depth hits the body; drop the person to the ground under it is not needed
                # for x/y, which is what we compare
                obs["d_depth"] = float(np.linalg.norm(d_cam))
                best = (p, "depth", obs["d_depth"])
        # 2) ray through the feet onto the ground (always computed: the run-log keeps both)
        origin = p_body + R @ self.color_off
        d = Rc @ pixel_ray((u0 + u1) / 2, v1, self.K)
        p = self.ground.hit(origin, d, a.max_range)
        if p is not None:
            obs["ground"] = (p, float(np.linalg.norm(p - origin)))
            best = best or (p, "ground", obs["ground"][1])
        return None if best is None else (*best, obs)

    def cluster(self, p, method, conf):
        best, bd = None, 1e9
        for i, c in enumerate(self.persons):
            dd = math.hypot(c["n"] - p[0], c["e"] - p[1])
            if dd < bd:
                best, bd = i, dd
        w = 3.0 if method == "depth" else 1.0  # depth fixes are better
        if best is not None and bd <= self.a.cluster_m:
            c = self.persons[best]
            W = c["w"] + w
            for k, v in zip("ned", p):
                c[k] = (c[k] * c["w"] + v * w) / W
            c["w"], c["n_sight"] = W, c["n_sight"] + 1
            c["conf_max"] = max(c["conf_max"], conf)
            c["methods"][method] = c["methods"].get(method, 0) + 1
            new = c["n_sight"] == self.a.min_sightings and not c["pinged"]
            if new:
                c["pinged"] = True
            return best, new
        c = {"n": p[0], "e": p[1], "d": p[2], "w": w, "n_sight": 1, "conf_max": conf,
             "methods": {method: 1}, "pinged": self.a.min_sightings <= 1, "t_first": time.time()}
        self.persons.append(c)
        return len(self.persons) - 1, c["pinged"]

    def ping(self, pid, c, rgb, box, conf):
        lat, lon = ned_to_latlon(c["n"], c["e"], self.pos.ref_lat, self.pos.ref_lon)
        e, n = c["e"] - self.home[1], c["n"] - self.home[0]
        print(f"\033[1;32mPING person {pid}: {lat:.6f}, {lon:.6f}  ({e:+.1f} E, {n:+.1f} N m from "
              f"takeoff)  conf {conf:.2f}  {c['n_sight']} sightings\033[0m", flush=True)
        try:
            from PIL import Image as PILImage
            from PIL import ImageDraw
            im = PILImage.fromarray(np.ascontiguousarray(rgb))
            ImageDraw.Draw(im).rectangle(box, outline=(255, 0, 0), width=2)
            im.save(os.path.join(self.out, f"person_{pid:02d}.jpg"), quality=85)
        except Exception as ex:  # a picture is nice to have, not needed
            self.log(f"could not save the picture: {ex}")

    # --- flight ---------------------------------------------------------------------------------

    def stamp(self):
        return self.get_clock().now().nanoseconds // 1000

    def command(self, cmd, p1=0.0, p2=0.0):
        m = VehicleCommand()
        m.command, m.param1, m.param2 = cmd, float(p1), float(p2)
        m.target_system = m.target_component = m.source_system = m.source_component = 1
        m.from_external, m.timestamp = True, self.stamp()
        self.cmd_pub.publish(m)

    def setpoint(self, n, e, d, yaw):
        mo = OffboardControlMode()
        mo.position, mo.timestamp = True, self.stamp()
        self.mode_pub.publish(mo)
        sp = TrajectorySetpoint()
        sp.position = [float(n), float(e), float(d)]
        sp.velocity = [math.nan] * 3
        sp.yaw, sp.timestamp = float(yaw), self.stamp()
        self.sp_pub.publish(sp)

    def go(self, state):
        self.log(f"{self.state} -> {state}")
        self.state, self.t_state = state, time.time()

    def tick(self):
        p, s, a = self.pos, self.status, self.a
        if p is None or s is None:
            return
        armed = s.arming_state == VehicleStatus.ARMING_STATE_ARMED
        offboard = s.nav_state == VehicleStatus.NAVIGATION_STATE_OFFBOARD
        if self.state == "WAIT":
            if time.time() - self.t_state > 20 and not getattr(self, "_warned", False):
                self._warned = True
                self.log("still waiting: PX4 preflight checks fail (seen after a landing in the same "
                         "sim; restart the sim) or no position fix yet")
            if p.xy_valid and p.z_valid and s.pre_flight_checks_pass and time.time() - self.t_state > 2:
                self.home = np.array([p.x, p.y, p.z])
                self.yaw0 = p.heading
                if self.rl is not None:
                    self.start_runlog(p)
                if a.dem:
                    self.ground = DemGround(a.world_sdf, self.home, a.body_height)
                    self.log("ground: Kartverket DTM")
                else:
                    self.ground = FlatGround(p.z + a.body_height)
                    self.log(f"ground: flat, NED z {p.z + a.body_height:.2f} (takeoff spot)")
                self.go("ARM")
            return
        n0, e0, d0 = self.home
        alt_d = d0 - a.alt
        if self.state == "ARM":
            self.setpoint(n0, e0, d0 - 0.5, self.yaw0)
            if time.time() - self.t_state > 1 and int((time.time() - self.t_state) * 10) % 10 == 0:
                self.command(VehicleCommand.VEHICLE_CMD_DO_SET_MODE, 1, 6)
                self.command(VehicleCommand.VEHICLE_CMD_COMPONENT_ARM_DISARM, 1)
            if armed and offboard:
                self.go("CLIMB")
        elif self.state == "CLIMB":
            self.setpoint(n0, e0, alt_d, self.yaw0)
            if abs(p.z - alt_d) < 1.0:
                self.carrot = np.array([p.x, p.y])
                self.go("PATH")
        elif self.state in ("PATH", "RETURN"):
            if time.time() - self.t_rate >= 1.0:
                self.t_rate = time.time()
                self.rate_log.append(self.rates())
            if not (armed and offboard):
                self.log("lost offboard/arming, stopping")
                self.go("END")
                return
            if self.state == "PATH":
                e, n = self.wps[self.wp_i]
                goal = np.array([n0 + n, e0 + e])
            else:
                goal = np.array([n0, e0])
            dvec = goal - self.carrot
            dist = np.linalg.norm(dvec)
            lag = math.hypot(p.x - self.carrot[0], p.y - self.carrot[1])
            step = a.speed * 0.1 if lag < 6.0 else 0.0  # wait for the drone if it lags
            if dist <= step:
                self.carrot = goal.copy()
                if self.state == "PATH":
                    self.wp_i += 1
                    if self.wp_i % 2 == 0 or self.wp_i == len(self.wps):
                        self.log(f"waypoint {self.wp_i}/{len(self.wps)}, {len(self.persons)} candidates, "
                                 f"{self.sightings} sightings")
                    if self.wp_i >= len(self.wps):
                        self.go("RETURN")
                elif lag < 2.0:
                    self.go("LAND")
                    self.command(VehicleCommand.VEHICLE_CMD_NAV_LAND)
            else:
                self.carrot = self.carrot + dvec / dist * step
                self.yaw_sp = math.atan2(dvec[1], dvec[0])
            self.setpoint(self.carrot[0], self.carrot[1], alt_d, getattr(self, "yaw_sp", self.yaw0))
        elif self.state == "LAND":
            if not armed or time.time() - self.t_state > 120:
                self.go("END")
        if self.state == "END" and not self.done:
            self.done = True


# ---------------------------------------------------------------------------------------------
# Evaluation


def load_truth(path):
    import yaml
    people = yaml.safe_load(open(path))["people"]
    out = []
    for p in people:
        pts = [(p["x"], p["y"])] + [tuple(q) for q in p.get("path", [])[1:]]
        out.append({"id": p["id"], "state": p["state"], "zone": p.get("zone", ""), "pts": pts,
                    "z": p.get("z", 0.0)})
    return out


def dist_to_truth(x, y, t):
    """Distance from (x, y) to a person; walking people: to their path segment."""
    pts = t["pts"]
    if len(pts) == 1:
        return math.hypot(x - pts[0][0], y - pts[0][1])
    best = 1e9
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        vx, vy = bx - ax, by - ay
        w = max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / max(vx * vx + vy * vy, 1e-9)))
        best = min(best, math.hypot(x - ax - w * vx, y - ay - w * vy))
    return best


def evaluate(node, a, out):
    lines = []
    P = lambda s="": (lines.append(s), print(s))
    pos = node.pos
    persons = [c for c in node.persons if c["n_sight"] >= a.min_sightings]
    P(f"\n=== search_test summary ({a.world or 'no world'}) ===")
    P(f"sightings logged: {node.sightings}, candidate clusters: {len(node.persons)}, "
      f"persons (>= {a.min_sightings} sightings): {len(persons)}")
    rows = []
    lat0w = lon0w = None
    if a.world_sdf:
        lat0w, lon0w, _ = world_origin(a.world_sdf)
    for i, c in enumerate(node.persons):
        lat, lon = ned_to_latlon(c["n"], c["e"], pos.ref_lat, pos.ref_lon)
        row = {"person": i, "n_sight": c["n_sight"], "conf_max": round(c["conf_max"], 2),
               "methods": c["methods"], "local_n": round(c["n"], 2), "local_e": round(c["e"], 2),
               "lat": round(lat, 7), "lon": round(lon, 7)}
        if lat0w is not None:
            wn, we = latlon_to_ned(lat, lon, lat0w, lon0w)  # world ENU = (east, north)
            row["world_x"], row["world_y"] = round(we, 2), round(wn, 2)
        rows.append(row)
    with open(os.path.join(out, "persons.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) + ["truth", "err_m"] if rows else ["person"])
        w.writeheader()
        truth = load_truth(a.truth) if a.truth else None
        x0, y0, x1, y1 = a.area
        m = a.eval_margin
        stats = {}
        if truth:
            for r in rows:
                if "world_x" not in r:
                    continue
                d = [(dist_to_truth(r["world_x"], r["world_y"], t), t["id"]) for t in truth]
                dmin, tid = min(d)
                r["truth"], r["err_m"] = tid, round(dmin, 2)
        for r in rows:
            w.writerow(r)
    for r in rows:
        if r["n_sight"] >= a.min_sightings:
            P(f"  person {r['person']:2d}: {r['lat']:.6f}, {r['lon']:.6f}  world ({r.get('world_x', 0):7.1f}, "
              f"{r.get('world_y', 0):7.1f})  {r['n_sight']:3d} sightings {r['methods']}  "
              f"nearest truth {r.get('truth', '-')} {r.get('err_m', '-')} m")
    if truth:
        # People in the searched rectangle (+ margin), in world coordinates (spawn = origin).
        inside = [t for t in truth if any(x0 - m <= px <= x1 + m and y0 - m <= py <= y1 + m for px, py in t["pts"])]
        conf_rows = [r for r in rows if r["n_sight"] >= a.min_sightings and "err_m" in r]
        found, errs = {}, []
        for t in truth:
            near = [(dist_to_truth(r["world_x"], r["world_y"], t), r["person"]) for r in conf_rows]
            near = [n for n in near if n[0] <= a.match_m]
            if near:
                found[t["id"]] = min(near)
        for tid, (e, _) in found.items():
            errs.append(e)
        tp = [r for r in conf_rows if r["err_m"] <= a.match_m]
        fp = [r for r in conf_rows if r["err_m"] > a.match_m]
        found_inside = [t["id"] for t in inside if t["id"] in found]
        P(f"\nground truth: {a.truth}")
        P(f"people in the searched area (+{m:.0f} m): {len(inside)}: {[t['id'] for t in inside]}")
        P(f"found within {a.match_m} m: {len(found_inside)}/{len(inside)} in the area"
          f" (recall {len(found_inside) / max(1, len(inside)):.0%}), {len(found)} in total: "
          + ", ".join(f"{k} {v[0]:.1f} m" for k, v in sorted(found.items())))
        if errs:
            P(f"position error of found people: mean {np.mean(errs):.2f} m, median {np.median(errs):.2f} m, "
              f"max {np.max(errs):.2f} m")
        P(f"persons that match nobody within {a.match_m} m (false or badly placed): {len(fp)}"
          + (": " + ", ".join(f"#{r['person']} {r['err_m']} m from {r['truth']}" for r in fp) if fp else ""))
        P(f"duplicate persons for the same truth: {len(tp) - len(set(r['truth'] for r in tp))}")
        missed = [t["id"] for t in inside if t["id"] not in found]
        P(f"missed in the area: {missed}")
        stats = {"inside": len(inside), "found_inside": len(found_inside), "found_total": len(found),
                 "mean_err": float(np.mean(errs)) if errs else None,
                 "max_err": float(np.max(errs)) if errs else None, "false": len(fp)}
    if node.rate_log:
        r = np.array(node.rate_log)
        P(f"\nwhile flying ({len(r)} s): camera {r[:, 0].mean():.1f} Hz (min {r[:, 0].min():.1f}), "
          f"real-time factor {r[:, 1].mean():.2f} (min {r[:, 1].min():.2f}), YOLO {r[:, 2].mean():.1f} fps")
    if node.vis:
        P("\nin the camera view (geometry only, trees/buildings not checked; cyan circle in truth_*.jpg):")
        state = {t["id"]: t["state"] for t in node.truth}
        for tid, s in sorted(node.vis.items()):
            P(f"  {tid} ({state[tid]}): in view {s['frames']} frames, YOLO box on it in {s['detected']}, "
              f"closest {s['best_range']:.0f} m{'  -> found' if tid in (found if truth else {}) else ''}")
        node.save_truth_views()
    if node.rl is not None:
        node.flush_heads()
        rl_rows = []
        for r, c in zip(rows, node.persons):
            rl_rows.append({"person": r["person"], "n_sight": c["n_sight"], "conf_max": r["conf_max"],
                            "e": round(c["e"], 2), "n": round(c["n"], 2), "u": round(-c["d"], 2),
                            "lat": r["lat"], "lon": r["lon"], "t_first": round(c.get("t0_log", 0) - (node.rl.t0 or 0), 2),
                            "t_last": round(c.get("t1_log", 0) - (node.rl.t0 or 0), 2), "pinged": int(c["pinged"]),
                            "truth": r.get("truth", ""), "err_m": r.get("err_m", "")})
        P(f"run-log: {node.rl.close(persons=rl_rows, stats=stats if truth else None)}"
          f"  (view: python3 tools/flight_viewer/view.py)")
        node.rl = None
    with open(os.path.join(out, "summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump({"args": vars(a), "persons": rows, "stats": stats if truth else None}, f, indent=1, default=str)
    print(f"\nlogs: {out}")


# ---------------------------------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--area", default="-40,-40,40,40",
                    help="search rectangle x0,y0,x1,y1: east,north m from takeoff (default %(default)s)")
    ap.add_argument("--route", default="",
                    help="fixed waypoints 'e,n;e,n;...' (m east/north of takeoff) instead of the lawnmower")
    ap.add_argument("--spacing", type=float, default=40.0, help="lawnmower leg spacing, m")
    ap.add_argument("--alt", type=float, default=15.0, help="height over takeoff, m")
    ap.add_argument("--speed", type=float, default=4.0, help="m/s")
    ap.add_argument("--cam-pitch", type=float, default=0.0,
                    help="camera tilt, deg down: same value as the launch's cam_pitch")
    ap.add_argument("--cam-x", type=float, default=0.12, help="same as the launch's cam_x")
    ap.add_argument("--cam-z", type=float, default=0.242, help="same as the launch's cam_z")
    ap.add_argument("--base-z", type=float, default=0.24, help="body origin (IMU) over the model origin, m")
    ap.add_argument("--body-height", type=float, default=0.24,
                    help="body origin over the ground when landed, m (x500: 0.24)")
    ap.add_argument("--model", default=os.environ.get("COGIDRONE_YOLO_MODEL",
                                                      os.path.expanduser("~/cogidrone_deps/models/yolo11n.pt")))
    ap.add_argument("--device", default=os.environ.get("COGIDRONE_YOLO_DEVICE", "cuda:0"))
    ap.add_argument("--conf", type=float, default=0.3, help="YOLO person confidence")
    ap.add_argument("--imgsz", type=int, default=0,
                    help="YOLO input size; 0 = image width, at least 960 (640x480 is upscaled: small people)")
    ap.add_argument("--show", action=argparse.BooleanOptionalAction, default=bool(os.environ.get("DISPLAY")),
                    help="window with the camera, YOLO boxes and status (default: on if $DISPLAY is set)")
    ap.add_argument("--depth-max", type=float, default=10.0, help="trust depth up to this range, m")
    ap.add_argument("--max-range", type=float, default=150.0, help="ignore ground hits further than this, m")
    ap.add_argument("--cluster-m", type=float, default=4.0, help="sightings closer than this = same person")
    ap.add_argument("--min-sightings", type=int, default=3, help="sightings before a person is pinged")
    ap.add_argument("--world", default="", help="sim world name: enables the ground-truth evaluation")
    ap.add_argument("--dem", action="store_true", help="Bymarka worlds: real terrain instead of flat ground")
    ap.add_argument("--match-m", type=float, default=5.0, help="evaluation: found = within this of the truth")
    ap.add_argument("--eval-margin", type=float, default=0.0, help="evaluation: grow the area by this, m")
    ap.add_argument("--no-depth-bridge", action="store_true",
                    help="do not start a Gazebo depth bridge (real drone / depth already bridged)")
    ap.add_argument("--depth-bridge", action="store_true",
                    help="start it even when --alt is above --depth-max (normally skipped: the ground "
                         "is out of depth range, and Gazebo then does not render depth: ~10 %% faster)")
    ap.add_argument("--image-topic", dest="image_topic", default="/camera/camera/color/image_raw")
    ap.add_argument("--info-topic", dest="info_topic", default="/camera/camera/color/camera_info")
    ap.add_argument("--depth-topic", dest="depth_topic", default="/camera/camera/depth/image_rect_raw")
    ap.add_argument("--depth-info-topic", dest="depth_info_topic", default="/camera/camera/depth/camera_info")
    ap.add_argument("--out", default=os.path.expanduser("~/search_test_runs"))
    g = ap.add_argument_group("run-log for tools/flight_viewer")
    g.add_argument("--runlog", action=argparse.BooleanOptionalAction, default=True, help="write it (default on)")
    g.add_argument("--log-dir", default=os.path.expanduser("~/flight_runs/latest"),
                   help="replaced every run (default %(default)s)")
    g.add_argument("--keep", action="store_true", help="also keep a timestamped copy next to --log-dir")
    g.add_argument("--track-hz", type=float, default=5.0, help="track samples per second (default 5)")
    g.add_argument("--frame-every", type=float, default=3.0, help="try to save a camera frame every N s (default 3)")
    g.add_argument("--frame-min-move", type=float, default=2.0,
                   help="... but only if moved >= this many m since the last kept frame (default 2)")
    g.add_argument("--frame-min-turn", type=float, default=15.0,
                   help="... or the camera turned >= this many deg (default 15)")
    g.add_argument("--max-frames", type=int, default=2000, help="cap on sampled frames (default 2000)")
    h = ap.add_argument_group("head distance (Muhammad's depth_head.py) for the run-log")
    h.add_argument("--head-model", default=os.path.expanduser("~/cogidrone_deps/models/head_medium.pt"),
                   help="head detector weights; off if missing or '' (default %(default)s)")
    h.add_argument("--head-conf", type=float, default=0.25, help="head detector confidence (default 0.25)")
    h.add_argument("--head-every", type=int, default=1, help="run it on every Nth sighting (default 1)")
    h.add_argument("--head-min-px", type=float, default=30.0,
                   help="only person boxes at least this tall, full-res px (default 30)")
    h.add_argument("--head-imgsz", type=int, default=320, help="head detector input size (default 320)")
    h.add_argument("--head-height-m", type=float, default=0.23,
                   help="head height for distance = H * fy / head px (default 0.23, depth_head.py)")
    h.add_argument("--body-height-m", type=float, default=1.70,
                   help="person height for distance = H * fy / box px (default 1.70; not --body-height)")
    g.add_argument("--frame-width", type=int, default=640, help="saved frames are scaled to this width, px")
    a = ap.parse_args()
    a.area = [float(v) for v in a.area.split(",")]
    a.world_sdf = os.path.join(REPO, "sim", "worlds", f"{a.world}.sdf") if a.world else ""
    a.truth = os.path.join(REPO, "sim", "worlds", f"{a.world}_people.yaml") if a.world else ""
    if a.world and not os.path.exists(a.truth):
        a.truth = ""
    out = os.path.join(a.out, dt.datetime.now().strftime("%Y%m%d_%H%M%S") + (f"_{a.world}" if a.world else ""))
    os.makedirs(out, exist_ok=True)

    bridge = None
    if a.alt > a.depth_max + 2 and not a.depth_bridge:
        a.no_depth_bridge = True
        print(f"flying at {a.alt:g} m, depth only reaches {a.depth_max:g} m: no depth bridge (--depth-bridge to force)")
    if not a.no_depth_bridge:
        # Depth is not in gz_bridge.yaml (Gazebo renders it only while someone listens).
        cfg = os.path.join(out, "depth_bridge.yaml")
        with open(cfg, "w") as f:
            f.writelines(f"- {{ros_topic_name: '{ros}', gz_topic_name: '{gz}', ros_type_name: '{rt}', "
                        f"gz_type_name: '{gt}', direction: GZ_TO_ROS}}\n" for ros, gz, rt, gt in (
                (a.depth_topic, "/cogidrone/camera/depth/image", "sensor_msgs/msg/Image", "gz.msgs.Image"),
                (a.depth_info_topic, "/cogidrone/camera/depth/camera_info", "sensor_msgs/msg/CameraInfo",
                 "gz.msgs.CameraInfo"),
            ))
        bridge = subprocess.Popen(["ros2", "run", "ros_gz_bridge", "parameter_bridge", "--ros-args",
                                   "-p", f"config_file:={cfg}"], stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL, start_new_session=True)

    rclpy.init()
    node = SearchTest(a, out)
    ex = MultiThreadedExecutor(num_threads=3)
    ex.add_node(node)
    win = "search_test: drone camera + YOLO"
    if a.show:
        import cv2
        cv2.namedWindow(win, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(win, 1280, 720)
    try:
        while rclpy.ok() and not node.done:
            ex.spin_once(timeout_sec=0.02)
            if a.show:
                with node.lock:
                    view, node.view_new = (node.view if node.view_new else None), False
                if view is not None:
                    cv2.imshow(win, view)
                cv2.waitKey(1)
    except KeyboardInterrupt:
        print("Ctrl-C: landing")
        node.command(VehicleCommand.VEHICLE_CMD_NAV_LAND)
    finally:
        try:
            if node.pos is not None:
                evaluate(node, a, out)
        finally:
            node.stop = True
            node.worker.join(timeout=2.0)
            node.f_sight.close()
            if node.rl is not None:  # no evaluation (never got a position): still close the log
                node.flush_heads()
                node.rl.close()
            if a.show:
                cv2.destroyAllWindows()
            if bridge:
                os.killpg(bridge.pid, 15)
            node.destroy_node()
            rclpy.try_shutdown()


if __name__ == "__main__":
    main()
