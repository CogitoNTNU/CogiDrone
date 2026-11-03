"""Score candidate FrSky XM+ positions on the built drone.

For each candidate (on the landing-gear poles and under the arms) the receiver sits on the outside of
the tube with its two antennas pointing down in a 90 deg V (one down-forward, one down-back). We score:
  - sky view toward the pilot: fraction of downward/outward directions (elevation -3..-80 deg, all
    azimuths, low angles weighted up) where at least one antenna tip has a clear line (diversity),
  - distance to 2.4 GHz noise: Jetson (USB 3 + Wi-Fi), D435i, the two USB cables,
  - distance to the SiK 433 MHz antenna (100 mW transmitter),
  - signal-wire length back to the Pixhawk.
World frame: metres, forward +X, left +Y, up +Z.
"""
import bpy, json, math, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

dg = bpy.context.evaluated_depsgraph_get()
SKIP = ("Prop disc", "Depth camera FOV", "Label", "Ground", "FrSky XM+", "XM+ antenna", "Battery strap",
        "Jetson battery strap", "Cam ", "Sun")
verts, polys = [], []
for o in bpy.data.objects:
    if o.type not in ("MESH",) or o.name.startswith(SKIP) or o.hide_render:
        continue
    me = o.evaluated_get(dg).to_mesh()
    mw = o.matrix_world
    off = len(verts)
    verts += [mw @ v.co for v in me.vertices]
    polys += [[off + i for i in p.vertices] for p in me.polygons]
    o.evaluated_get(dg).to_mesh_clear()
bvh = BVHTree.FromPolygons(verts, polys)


def pts_of(name_prefix, n=40):
    out = []
    for o in bpy.data.objects:
        if o.name.startswith(name_prefix) and not o.name.startswith(("Jetson batt", "Jetson fuse")):
            if o.type == "CURVE":
                for s in o.data.splines:
                    for p in s.bezier_points:
                        out.append(o.matrix_world @ p.co)
            else:
                out += [o.matrix_world @ Vector(c) for c in o.bound_box]
                for c in o.children_recursive:
                    if c.type == "MESH":
                        out += [c.matrix_world @ Vector(b) for b in c.bound_box]
    return out


NOISE = {
    "Jetson (USB3 + Wi-Fi)": pts_of("Jetson Orin") + pts_of("Jetson carrier") + pts_of("Jetson heatsink") + pts_of("Jetson USB") + pts_of("Jetson module"),
    "D435i": pts_of("D435 mesh") + pts_of("Intel RealSense"),
    "USB3 cable": pts_of("D435i USB3"),
    "UART-USB cable": pts_of("UART-USB"),
}
SIK = [o.matrix_world.translation for o in bpy.data.objects if o.name.startswith("SiK antenna")]
PX = bpy.data.objects["Pixhawk 6C (arrow forward)"].matrix_world.translation


def axis_of(o):
    """Principal axis of a tube mesh: returns (top, bottom) end points in world space."""
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    c = sum(vs, Vector()) / len(vs)
    # crude PCA by power iteration
    d = Vector((0.3, 0.2, 1.0))
    for _ in range(30):
        acc = Vector()
        for v in vs[:: max(1, len(vs) // 3000)]:
            r = v - c
            acc += r * r.dot(d)
        d = acc.normalized()
    proj = [(v - c).dot(d) for v in vs]
    a, b = c + d * min(proj), c + d * max(proj)
    return (a, b) if a.z > b.z else (b, a)


cands = []
# landing-gear poles
for o in bpy.data.objects:
    if o.type == "MESH" and o.name.startswith("GUAN-CHENG"):
        top, bot = axis_of(o)
        side = "left" if top.y > 0 else "right"
        for f in (0.15, 0.3, 0.45, 0.6, 0.75):
            p = top.lerp(bot, f)
            out = Vector((0, 1 if side == "left" else -1, 0))
            cands.append((f"{side} landing-gear pole, {int(f * 100)}% down", p + out * 0.016, 0.008))
# arms: underside, 90..150 mm from the centre
for o in bpy.data.objects:
    if o.type == "MESH" and o.name.startswith("HMX5V-GUAN-DINGWEI"):
        a, b = axis_of(o)
        inner, outer = (a, b) if a.xy.length < b.xy.length else (b, a)
        d = (outer - inner).normalized()
        name = ("front" if outer.x > 0 else "rear") + "-" + ("left" if outer.y > 0 else "right")
        for r in (0.10, 0.13, 0.16):
            t = (r - inner.xy.length) / max(1e-6, (outer.xy.length - inner.xy.length))
            p = inner.lerp(outer, max(0.0, min(1.0, t)))
            cands.append((f"{name} arm, {int(r * 1000)} mm out, underneath", p - Vector((0, 0, 0.016)), 0.008))

ANT_LEN = 0.095  # coax from the receiver to the tip of the active element (XM+ ~ 10 cm leads)
dirs = []
for el in (-3, -10, -20, -30, -45, -60, -80):
    w = 3 if el >= -20 else (2 if el >= -45 else 1)
    for az in range(0, 360, 15):
        e, a = math.radians(el), math.radians(az)
        dirs.append((Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e))), w))
wsum = sum(w for _, w in dirs)

rows = []
for name, p, _ in cands:
    tips = [p + Vector((s * math.sin(math.radians(45)), 0, -math.cos(math.radians(45)))) * ANT_LEN for s in (1, -1)]
    # antenna tips must not be inside or touching something
    blocked_tip = any(bvh.find_nearest(t)[3] is not None and bvh.find_nearest(t)[3] < 0.004 for t in tips)
    vis = 0.0
    for d, w in dirs:
        if any(bvh.ray_cast(t + d * 0.003, d, 3.0)[0] is None for t in tips):
            vis += w
    vis /= wsum
    nd = {k: min(((q - pp).length for q in v for pp in [p] + tips), default=9) for k, v in NOISE.items()}
    noise_min = min(nd.values())
    sik = min(((s - p).length for s in SIK), default=9)
    cable = (abs(p.x - PX.x) + abs(p.y - PX.y) + abs(p.z - PX.z)) + 0.04
    score = vis * 100 - max(0, 0.12 - noise_min) * 400 - max(0, 0.10 - sik) * 200 - max(0, cable - 0.30) * 50
    rows.append({"where": name, "pos_m": [round(v, 3) for v in p], "clear_view_%": round(vis * 100, 1),
                 "nearest_noise": min(nd, key=nd.get), "noise_mm": round(noise_min * 1000), "sik_mm": round(sik * 1000),
                 "cable_mm": round(cable * 1000), "tip_clash": blocked_tip, "score": round(score, 1)})

rows.sort(key=lambda r: -r["score"])
print("RXPLACE" + json.dumps(rows))
