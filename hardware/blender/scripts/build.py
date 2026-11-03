"""Build the CogiDrone X500 V2 assembly in Blender.

Run: blender -b --factory-startup --python build.py -- <scratch dir> <out .blend>

Working frame while building = Holybro's STEP frame in millimetres (forward = -X, up = +Z,
bottom plate underside at Z = 0). At the end everything is parented to "CogiDrone X500 V2", which turns
it to forward = +X / left = +Y (ROS base_link style), scales to metres and puts the skids on the ground.
"""
import bpy, sys, os, math, json
from mathutils import Vector, Matrix, Euler

S = sys.argv[sys.argv.index("--") + 1]
OUT = sys.argv[sys.argv.index("--") + 2]
sys.path.insert(0, os.path.join(S, "tools"))
import parts as P

G = os.path.join(S, "cad_glb")
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"


def coll(name, parent=None):
    c = bpy.data.collections.new(name)
    (parent or scene.collection).children.link(c)
    return c


C_FRAME = coll("Frame (Holybro STEP)")
C_FC = coll("Flight electronics")
C_COMP = coll("Companion computer + camera")
C_POWER = coll("Batteries + Jetson power")
C_WIRES = coll("Wiring")
C_HELP = coll("Helpers (FOV, labels)")

M = {
    "carbon": P.mat("Carbon fibre", (0.03, 0.03, 0.035), 0.0, 0.35),
    "alu": P.mat("Aluminium", (0.62, 0.63, 0.66), 1.0, 0.35),
    "metal": P.mat("Steel", (0.5, 0.5, 0.52), 1.0, 0.3),
    "motor": P.mat("Motor", (0.12, 0.12, 0.13), 0.8, 0.35),
    "print": P.mat("Printed part (Holybro)", (0.85, 0.42, 0.12), 0.0, 0.6),
    "pcb": P.mat("PCB green", (0.05, 0.32, 0.12), 0.0, 0.5),
    "pcb_dark": P.mat("PCB dark", (0.05, 0.08, 0.06), 0.0, 0.5),
    "black": P.mat("Black plastic", (0.02, 0.02, 0.02), 0.0, 0.6),
    "white": P.mat("White plastic", (0.85, 0.85, 0.82), 0.0, 0.6),
    "usb3": P.mat("USB3 blue", (0.05, 0.2, 0.75), 0.0, 0.5),
    "red": P.mat("Red", (0.75, 0.05, 0.04), 0.0, 0.5),
    "xt60": P.mat("XT60 yellow", (0.95, 0.75, 0.05), 0.0, 0.5),
    "lipo_flight": P.mat("Flight LiPo (Sunpadow)", (0.12, 0.12, 0.15), 0.2, 0.4),
    "lipo_jetson": P.mat("Jetson LiPo (Etop)", (0.10, 0.25, 0.55), 0.1, 0.45),
    "pixhawk": P.mat("Pixhawk case", (0.8, 0.8, 0.82), 0.0, 0.4),
    "foam": P.mat("Foam pad", (0.15, 0.15, 0.15), 0.0, 0.9),
    "prop": P.mat("Prop", (0.05, 0.05, 0.05), 0.0, 0.4),
    "propdisc": P.mat("Prop disc", (0.6, 0.65, 0.7), 0.0, 0.5, alpha=0.12),
    "w_power": P.mat("Wire power", (0.85, 0.30, 0.05), 0.0, 0.5),
    "w_neg": P.mat("Wire black", (0.04, 0.04, 0.04), 0.0, 0.5),
    "w_data": P.mat("Wire data", (0.15, 0.40, 0.85), 0.0, 0.5),
    "w_x": P.mat("Wire crossover", (0.55, 0.30, 0.80), 0.0, 0.5),
    "w_rf": P.mat("Antenna", (0.2, 0.2, 0.2), 0.0, 0.5),
    "fov": P.mat("Camera FOV", (0.2, 0.5, 1.0), 0.0, 0.5, alpha=0.10),
    "label": P.mat("Label", (0.05, 0.05, 0.08), 0.0, 0.8),
    "make": P.mat("Make yourself (purple)", (0.55, 0.30, 0.80), 0.0, 0.5),
    "buy": P.mat("Buy (orange)", (0.95, 0.55, 0.10), 0.0, 0.5),
}


def into(objs, c):
    for o in objs:
        for u in list(o.users_collection):
            u.objects.unlink(o)
        c.objects.link(o)


def import_glb(name, c):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(G, name + ".glb"))
    new = [o for o in bpy.data.objects if o not in before]
    into(new, c)
    roots = [o for o in new if o.parent is None]
    return new, roots


def group(name, objs, c):
    e = P.empty(name, coll=c)
    for o in objs:
        if o.parent is None:
            o.parent = e
    return e


# ---------------------------------------------------------------- frame (official Holybro STEP)
frame, _ = import_glb("x500v2-frame", C_FRAME)
for o in frame:
    if o.type != "MESH":
        continue
    n = o.name.upper()
    m = M["carbon"]
    if "CAMERA-INTEL" in n:
        m = M["print"]
    elif "2216" in n:
        m = M["motor"]
    elif "PCB" in n:
        m = M["pcb"]
    elif "XT60" in n:
        m = M["xt60"]
    elif n.startswith(("GB70", "M3", "M25", "LM-", "ZSLM")):
        m = M["metal"]
    elif any(k in n for k in ("MUJU", "JIA-LIANJIE", "JIAO-LIANJIE", "JIA-GUAN", "HUAN-GUIJIAO", "ZHIJIA-ZUO", "ZHUANJIETOU", "TUOPAN", "LUOMAO", "GAN-GPS")):
        m = M["alu"]
    elif "JIAO" in n or "EVA" in n or "MAO" in n or "PAD" in n:
        m = M["black"]
    o.data.materials.clear()
    o.data.materials.append(m)
frame_root = group("Holybro X500 V2 frame", frame, C_FRAME)

# key frame geometry (mm, build frame), measured from the STEP import
TOP_PLATE_Z = 32.0
PLATFORM_UNDER_Z = -28.0
PLATFORM_C = Vector((99.0, 0.0))
GPS_TRAY_TOP = 107.0
CAM_MOUNT_FRONT_X = -110.0
BATT_PLATE_UNDER_Z = -46.5
MOTOR_XY = 177.4
MOTOR_TOP_Z = 63.5
GROUND_Z = -215.3

# ---------------------------------------------------------------- props (10" 1045, PX4 Quad X)
# build frame: forward = -X, left = -Y.  front-right = (-x, +y)
motors = {1: (-1, 1, "CCW"), 2: (1, -1, "CCW"), 3: (-1, -1, "CW"), 4: (1, 1, "CW")}
for num, (sx, sy, d) in motors.items():
    c = Vector((sx * MOTOR_XY, sy * MOTOR_XY, MOTOR_TOP_Z + 3))
    disc = P.cyl(f"Prop disc M{num} {d} (10in)", 127.0, 0.6, c, material=M["propdisc"], coll=C_FRAME, verts=64)
    for side in (-1, 1):
        bl = P.box(f"Prop M{num} {d} 1045 blade", (120.0, 18.0, 2.2), c + Vector((side * 64, 0, 0)), M["prop"], coll=C_FRAME, bevel=0.8)
        bl.data.vertices.foreach_set("co", [v for vv in bl.data.vertices for v in (vv.co.x, vv.co.y * (1 - 0.55 * max(0, side * vv.co.x) / 60 if side * vv.co.x > 0 else 1), vv.co.z)])
        bl.rotation_euler = (math.radians(side * (8 if d == "CW" else -8)), 0, 0)
        bl.location = c
        bl.data.transform(Matrix.Translation((side * 64, 0, 0)))
        bl.rotation_euler.z = math.radians(30 + 45 * num)
    P.cyl(f"Prop nut M{num}", 6, 8, c + Vector((0, 0, 3)), material=M["alu"], coll=C_FRAME, verts=16)

# ---------------------------------------------------------------- Pixhawk 6C on its foam pad, top plate centre
P.box("Pixhawk foam pad", (84, 44, 3), (0, 0, TOP_PLATE_Z + 1.5), M["foam"], coll=C_FC)
px, px_roots = import_glb("Pixhawk6C", C_FC)
for o in px:
    if o.type == "MESH":
        o.data.materials.clear(); o.data.materials.append(M["pixhawk"] if "MIANKE" in o.name or "DIKE" in o.name else M["pcb"])
pixhawk = group("Pixhawk 6C (arrow forward)", px, C_FC)
pixhawk.location = (0, 0, TOP_PLATE_Z + 3 + 5.9)
arrow = P.box("Pixhawk forward arrow", (14, 3, 0.6), (-22, 0, TOP_PLATE_Z + 3 + 12.5), M["red"], coll=C_FC)
arrow.rotation_euler = (0, 0, 0)
P.box("Pixhawk forward arrow head", (6, 8, 0.6), (-31, 0, TOP_PLATE_Z + 3 + 12.5), M["red"], coll=C_FC).rotation_euler = (0, 0, math.radians(45))

# ---------------------------------------------------------------- GPS puck on the kit mast tray
gp, _ = import_glb("GPS-M10", C_FC)
for o in gp:
    if o.type == "MESH":
        o.data.materials.clear(); o.data.materials.append(M["black"] if "SK" in o.name or "XK" in o.name else M["pcb"])
gps = group("Holybro M10 GPS (arrow forward)", gp, C_FC)
gps.location = (PLATFORM_C.x, 0, GPS_TRAY_TOP + 6.0)
gps.rotation_euler = (0, 0, math.radians(-90))

# ---------------------------------------------------------------- RealSense D435i on Holybro's camera mount
cam_objs, _ = import_glb("d435", C_COMP)
cam_inner = group("D435 mesh (Intel realsense2_description)", cam_objs, C_COMP)
cam_inner.scale = (1000, 1000, 1000)          # mesh is in metres
cam_inner.rotation_euler = (0, 0, math.radians(-90))  # lens to -X (forward)
CAM_Z = -28.0
d435 = P.empty("Intel RealSense D435i", (CAM_MOUNT_FRONT_X - 25.1, 0, CAM_Z), coll=C_COMP)
cam_inner.parent = d435
# camera_link helper at the lens (what the ROS TF / sim model should match)
cam_link = P.empty("camera_link (real)", (CAM_MOUNT_FRONT_X - 25.1, 0, CAM_Z), coll=C_HELP)

# depth FOV frustum: 87 x 58 deg, 0.6 m
fov_len = 600.0
hw = math.tan(math.radians(87 / 2)) * fov_len
hh = math.tan(math.radians(58 / 2)) * fov_len
mesh = bpy.data.meshes.new("Depth FOV 87x58")
apex = Vector((0, 0, 0))
far = [Vector((-fov_len, sy * hw, sz * hh)) for sy, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
mesh.from_pydata([apex] + far, [], [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 1), (1, 2, 3, 4)])
fov = bpy.data.objects.new("Depth camera FOV (87x58 deg, 0.6 m)", mesh)
fov.data.materials.append(M["fov"])
C_HELP.objects.link(fov)
fov.location = d435.location

# ---------------------------------------------------------------- Jetson Orin Nano, upside down under the rear platform board
jet = P.jetson_orin_nano(C_COMP, M)
JET_SPACER = 10.0   # printed M2.5 spacers between the platform board and the carrier
jet.location = (PLATFORM_C.x, 0, PLATFORM_UNDER_Z - (JET_SPACER - 4.0))   # builder already has 4 mm feet
for sx in (-1, 1):
    for sy in (-1, 1):
        P.cyl("Jetson printed spacer 10mm (M2.5)", 3.0, JET_SPACER, (PLATFORM_C.x + sx * 29, sy * 43, PLATFORM_UNDER_Z - JET_SPACER / 2), material=M["print"], coll=C_COMP, verts=16)
        P.cyl("M2.5x18 screw", 1.25, 18, (PLATFORM_C.x + sx * 29, sy * 43, PLATFORM_UNDER_Z - 7), material=M["metal"], coll=C_COMP, verts=12)
jet.scale = (1000, 1000, 1000)   # builder is in metres
jet.rotation_euler = (math.pi, 0, math.radians(-90))   # long side across the platform (58 x 86 hole pattern), ports to +Y

# ---------------------------------------------------------------- batteries
FB = (139.0, 47.0, 40.0)
FB_X = -20.0   # shifted forward so the rear end clears the Jetson (see CG check)
fbat = P.lipo("Flight battery Sunpadow 4S 5200 (139x47x40)", FB, "lipo_flight", C_POWER, M, "XT60", leads=False)
fbat.location = (FB_X, 0, BATT_PLATE_UNDER_Z - FB[2] / 2)
fbat.rotation_euler = (0, 0, math.pi)  # leads out of the rear end
for i, x in enumerate((-35, 35)):
    P.box(f"Battery strap {i + 1}", (20, 50, 44), (FB_X + x, 0, BATT_PLATE_UNDER_Z - 20), M["black"], coll=C_POWER).display_type = "WIRE"

JB = (120.0, 35.0, 28.0)
P.box("Jetson battery foam pad", (35, 120, 3), (-61, 0, TOP_PLATE_Z + 1.5), M["foam"], coll=C_POWER)
jbat = P.lipo("Jetson battery Etop 4S 2200 (120x35x28)", JB, "lipo_jetson", C_POWER, M, "Deans")
jbat.location = (-61, 0, TOP_PLATE_Z + 3 + JB[2] / 2)
jbat.rotation_euler = (0, 0, math.radians(90))   # across the front edge, leads out to -Y
P.box("Jetson battery strap", (20, 4, 34), (-61, 0, TOP_PLATE_Z + 16), M["black"], coll=C_POWER).display_type = "WIRE"

# ---------------------------------------------------------------- small electronics (sizes from datasheets)
def part(name, size, loc, mat_, c, rot=0.0):
    o = P.box(name, size, loc, mat_, coll=c, bevel=0.6)
    o.rotation_euler = (0, 0, math.radians(rot))
    return o


pm02 = part("PM02 V3 power module", (38, 25, 10), (30, -42, -32), M["pcb_dark"], C_FC)
sik = part("SiK V3 433 MHz air radio", (52, 28, 12), (112, -112, 22), M["alu"], C_FC, 45)
P.cyl("SiK antenna (pointing down)", 4, 95, (130, -130, -30), material=M["w_rf"], coll=C_FC)
# ---- FrSky XM+ in the printed pole case on the right landing-gear pole, 45 % down (see rx_placement.py)
def pole_axis(o):
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    c = sum(vs, Vector()) / len(vs)
    d = Vector((0.3, 0.2, 1.0))
    for _ in range(30):
        acc = Vector()
        for v in vs[:: max(1, len(vs) // 3000)]:
            acc += (v - c) * (v - c).dot(d)
        d = acc.normalized()
    pr = [(v - c).dot(d) for v in vs]
    a, b = c + d * min(pr), c + d * max(pr)
    return (a, b) if a.z > b.z else (b, a)


bpy.context.view_layer.update()
right_pole = [o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("GUAN-CHENG") and o.matrix_world.translation.y + sum((o.matrix_world @ v.co).y for v in o.data.vertices[:50]) / 50 > 0][0]
ptop, pbot = pole_axis(right_pole)
RX_AT = ptop.lerp(pbot, 0.45)
ax = (ptop - pbot).normalized()                       # case +X: up the pole, toward the body
out = Vector((0, 1, 0)); out = (out - ax * out.dot(ax)).normalized()   # case +Z: away from the pole (right side)
ay = out.cross(ax).normalized()
RXM = Matrix((ax, ay, out)).transposed().to_4x4(); RXM.translation = RX_AT
for f, mt in (("xmplus_pole_case.stl", M["print"]), ("xmplus_pole_lid.stl", M["print"])):
    before = set(bpy.data.objects)
    bpy.ops.wm.stl_import(filepath=os.path.join(S, "prints", f))
    o = [x for x in bpy.data.objects if x not in before][0]
    into([o], C_FC); o.data.materials.clear(); o.data.materials.append(mt)
    o.matrix_world = RXM
    o.name = "Printed " + f[:-4]
xm = P.box("FrSky XM+ receiver", (21.5, 12, 3.5), (0, 0, 0), M["pcb_dark"], coll=C_FC)
xm.matrix_world = RXM @ Matrix.Translation((0, 0, 8.2 + 0.8 + 2.0 + 1.75))
for i, sgn in enumerate((1, -1)):
    d_loc = Vector((-math.cos(math.radians(45)), sgn * math.sin(math.radians(45)), 0))
    p0 = Vector((-24.0 / 2 + 2.0, sgn * 3.5, 8.2 + 0.8 + 2.0 + 1.6))
    pts = [RXM @ (Vector((-6, sgn * 3, 12.6))), RXM @ p0, RXM @ (p0 + d_loc * 20), RXM @ (p0 + d_loc * 60), RXM @ (p0 + d_loc * 92)]
    P.wire(f"XM+ antenna {i + 1} (90 deg V, down)", pts, 0.6, M["w_rf"], C_FC)
    P.cyl(f"XM+ antenna {i + 1} active tip", 0.9, 30, RXM @ (p0 + d_loc * 77), material=M["white"], coll=C_FC, verts=8).rotation_euler = (RXM.to_3x3() @ d_loc).to_track_quat("Z", "Y").to_euler()
for k, xx in enumerate((-6.0, 6.0)):
    zt = P.box(f"Zip tie {k + 1} (XM+ case)", (3.6, 1.2, 1.2), (0, 0, 0), M["black"], coll=C_FC)
    zt.display_type = "WIRE"
RX_TOP = RXM @ Vector((24.0 / 2 + 3, 0, 14.0))
conv = part("Holybro UART-to-USB (FT231X)", (30, 20, 6), (40, 30, -21), M["pcb_dark"], C_COMP)
xover = part("TELEM2 crossover cable label (MAKE)", (10, 6, 6), (20, 18, 36), M["make"], C_COMP)
buzz = part("LiPo buzzer 2-8S (BUY)", (46, 25, 12), (-61, -40, TOP_PLATE_Z + 3 + JB[2] + 6), M["buy"], C_POWER, 90)
fuse = part("Inline fuse 3A Biltema (BUY)", (14, 9, 32), (62, 52, -12), M["buy"], C_POWER)
kjell = P.cyl("Kjell 5.5x2.5 DC plug", 5.5, 30, (99, 58, -40), (math.pi / 2, 0, 0), M["black"], coll=C_POWER, verts=16)
deans = part("Deans male pigtail (BUY)", (12, 16, 6), (-61, -70, TOP_PLATE_Z + 8), M["buy"], C_POWER)
sd = part("microSD (CHECK)", (11, 15, 1), (100, 0, -70), M["white"], C_COMP)
sd.hide_render = True

# ---------------------------------------------------------------- wiring (build frame, mm)
bpy.context.view_layer.update()
def port(n):
    return bpy.data.objects["Jetson " + n].matrix_world.translation.copy()
JACK, USB1, USB2 = port("DC barrel jack 9-20V"), port("USB-A x2 (1)"), port("USB-A x2 (2)")
kjell.location = JACK + Vector((16, 0, 0)); kjell.rotation_euler = (0, math.pi / 2, 0)
P.box("Flight battery XT60 (female) + leads", (16, 16, 8), (50, -30, -36), M["xt60"], coll=C_POWER)

W = []
def w(name, pts, r, m):
    W.append(P.wire(name, [Vector(p) for p in pts], r, m, C_WIRES))

# flight power
w("Flight batt leads -> XT60", [(30, 6, -56), (44, 4, -48), (50, -30, -36)], 2.2, M["w_power"])
w("XT60 -> PM02", [(50, -30, -36), (30, -42, -32)], 2.2, M["w_power"])
w("PM02 -> frame XT60 (PDB)", [(30, -42, -32), (29, -47, -26), (29, -47, -24)], 2.2, M["w_power"])
w("PM02 6-pin -> Pixhawk POWER1", [(20, -40, -28), (10, -60, -5), (10, -60, 20), (30, -30, 36), (40, -10, 42)], 1.0, M["w_power"])
# flight controller data
w("Pixhawk GPS1 -> GPS", [(42, 8, 44), (70, 6, 45), (95, 4, 60), (97, 0, 100), (99, 0, 108)], 1.0, M["w_data"])
w("Pixhawk TELEM1 -> SiK", [(42, -12, 44), (70, -40, 40), (100, -95, 28), (112, -112, 22)], 1.0, M["w_data"])
w("Pixhawk RC IN -> XM+ (SBUS), down the right pole", [(42, 14, 44), (50, 40, 40), (30, 60, 20), (12, 62, -5), tuple(ptop + Vector((0, 10, 0))), tuple(RX_TOP + ax * 25 + out * 4), tuple(RX_TOP)], 1.0, M["w_data"])
w("Pixhawk TELEM2 -> crossover -> UART-USB", [(30, 22, 44), (20, 18, 36), (25, 25, 10), (40, 30, -15), (40, 30, -21)], 1.1, M["w_x"])
w("UART-USB -> Jetson USB-A (ttyUSB0)", [(52, 30, -22), (120, 34, -24), (155, USB1.y, USB1.z - 8), tuple(USB1 + Vector((9, 0, 0)))], 1.3, M["w_data"])
w("D435i USB3 -> Jetson USB-A (blue)", [(-110, 0, -40), (-100, 20, -30), (-40, 36, -20), (60, 38, -21), (140, 38, -24), (155, USB2.y, USB2.z + 6), tuple(USB2 + Vector((9, 0, 0)))], 1.8, M["usb3"])
# Jetson power (separate battery): Deans -> fuse -> Kjell plug -> barrel jack
w("Jetson batt + (red, fused)", [(-61, -70, TOP_PLATE_Z + 8), (-61, -85, 36), (0, -85, 36), (50, -70, 36), (70, 60, 30), (62, 52, -8)], 1.6, M["red"])
w("Jetson fuse -> Kjell plug", [(62, 52, -16), (120, 52, -20), (160, JACK.y, JACK.z), tuple(JACK + Vector((31, 0, 0)))], 1.6, M["red"])
w("Jetson batt - (black)", [(-61, -66, TOP_PLATE_Z + 8), (-58, -82, 37), (2, -82, 37), (52, -66, 37), (74, 62, 30), (64, 56, -12), (122, 50, -22), (161, JACK.y + 2, JACK.z + 2), tuple(JACK + Vector((31, 2, 2)))], 1.6, M["w_neg"])
w("Jetson batt balance -> buzzer", [(-61, -60, TOP_PLATE_Z + 10), (-61, -50, TOP_PLATE_Z + 30)], 0.8, M["white"])

# ---------------------------------------------------------------- checks: clearances, prop vs FOV, CG
def world_bb(o):
    pts = []
    stack = [o] + list(o.children_recursive)
    for x in stack:
        if x.type in ("MESH", "CURVE", "FONT"):
            pts += [x.matrix_world @ Vector(c) for c in x.bound_box]
    lo = Vector(map(min, *pts)); hi = Vector(map(max, *pts))
    return lo, hi


bpy.context.view_layer.update()
report = {}
jlo, jhi = world_bb(jet)
blo, bhi = world_bb(fbat)
report["jetson_bbox_mm"] = [list(map(lambda v: round(v, 1), jlo)), list(map(lambda v: round(v, 1), jhi))]
report["flight_batt_bbox_mm"] = [list(map(lambda v: round(v, 1), blo)), list(map(lambda v: round(v, 1), bhi))]
report["gap_flightbatt_to_jetson_x_mm"] = round(jlo.x - bhi.x, 1)
report["jetson_to_ground_mm"] = round(jlo.z - GROUND_Z, 1)
report["flightbatt_to_ground_mm"] = round(blo.z - GROUND_Z, 1)
glo, ghi = world_bb(gps)
report["gps_to_jetson_mm"] = round(glo.z - jhi.z, 1)

# prop tips inside the depth / colour FOV?  sample each prop disc edge
def in_fov(p, hfov, vfov, tilt_deg):
    v = p - d435.location
    v = Matrix.Rotation(math.radians(tilt_deg), 4, "Y") @ v  # undo a downward camera pitch of tilt_deg
    fwd = -v.x
    if fwd <= 0:
        return False
    return abs(math.degrees(math.atan2(v.y, fwd))) < hfov / 2 and abs(math.degrees(math.atan2(v.z, fwd))) < vfov / 2


for tilt in (0, 10):
    hits = {"depth": 0, "colour": 0}
    for num, (sx, sy, _) in motors.items():
        c = Vector((sx * MOTOR_XY, sy * MOTOR_XY, MOTOR_TOP_Z + 3))
        for k in range(180):
            a = 2 * math.pi * k / 180
            for rr in (60, 90, 127):
                p = c + Vector((math.cos(a) * rr, math.sin(a) * rr, 0))
                hits["depth"] += in_fov(p, 87, 58, tilt)
                hits["colour"] += in_fov(p, 69, 42, tilt)
    report[f"prop_points_in_fov_tilt_down{tilt}"] = hits

# camera position vs body centre (midway between the plates), in ROS axes (x fwd, y left, z up), metres
body_c = Vector((0, 0, (0 + TOP_PLATE_Z) / 2))
rel = d435.location - body_c
report["camera_link_vs_body_centre_m"] = [round(-rel.x / 1000, 3), round(-rel.y / 1000, 3), round(rel.z / 1000, 3)]

# centre of gravity along X / Y from the off-centre masses (g); frame, motors, props are symmetric
masses = [("Flight battery", 491, fbat), ("Jetson battery", 213, jbat), ("Jetson dev kit (~176 g)", 176, jet),
          ("D435i", 72, d435), ("Pixhawk 6C", 59, pixhawk), ("GPS + mast", 50, gps), ("SiK radio", 20, sik),
          ("UART-USB + wires", 30, conv), ("Buzzer, fuse, plug, Deans", 35, fuse)]
mx = sum(m * o.matrix_world.translation.x for _, m, o in masses)
my = sum(m * o.matrix_world.translation.y for _, m, o in masses)
mt = sum(m for _, m, _ in masses)
FRAME_KIT_G = 1050  # X500 V2 kit with motors, ESCs, PDB, props (approx.), centred
report["cg_offset_mm_fwd_left"] = [round(-mx / (mt + FRAME_KIT_G), 1), round(-my / (mt + FRAME_KIT_G), 1)]
report["auw_estimate_g"] = mt + FRAME_KIT_G

# ---------------------------------------------------------------- labels (face the render camera)
LABELS = [
    ("Pixhawk 6C", (0, 0, 70)), ("M10 GPS", (99, 0, 135)), ("D435i", (-150, 0, -10)),
    ("Jetson Orin Nano\n(under platform)", (130, 0, -95)), ("Flight LiPo 4S 5200", (-15, 0, -100)),
    ("Jetson LiPo 4S 2200", (-75, 0, 80)), ("SiK 433", (135, -135, 45)), ("XM+ (right pole)", tuple(RX_AT + Vector((0, 40, 0)))),
    ("UART-USB", (40, 55, -5)), ("PM02 V3", (25, -75, -40)), ("Buzzer", (-61, -40, 95)), ("Fuse 3A", (62, 80, 0)),
]
label_objs = []
for text, loc in LABELS:
    t = P.label("Label " + text.split("\n")[0], text, loc, C_HELP, M["label"], size=13)
    label_objs.append(t)

# ---------------------------------------------------------------- final transform: ROS axes, metres, on the ground
root = P.empty("CogiDrone X500 V2", coll=scene.collection)
for o in list(scene.objects):
    if o.parent is None and o is not root:
        o.parent = root
root.rotation_euler = (0, 0, math.pi)
root.scale = (0.001, 0.001, 0.001)
root.location = (0, 0, -GROUND_Z * 0.001)

ground = P.box("Ground", (2.0, 2.0, 0.002), (0, 0, -0.001), P.mat("Ground", (0.42, 0.44, 0.46), 0, 0.9), coll=C_HELP)

# ---------------------------------------------------------------- cameras + light
def add_cam(name, loc, target, lens=40, ortho=None):
    cd = bpy.data.cameras.new(name)
    if ortho:
        cd.type = "ORTHO"; cd.ortho_scale = ortho
    cd.lens = lens
    cd.clip_start = 0.01
    c = bpy.data.objects.new(name, cd)
    C_HELP.objects.link(c)
    c.location = loc
    c.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    return c


cams = {
    "iso_front_left": add_cam("Cam iso front-left", (0.78, 0.66, 0.55), (0.0, 0.0, 0.17), 50),
    "iso_rear_right": add_cam("Cam iso rear-right", (-0.62, -0.66, 0.40), (-0.02, 0.0, 0.17), 50),
    "side_left": add_cam("Cam side (left)", (0, 1.5, 0.18), (0, 0, 0.18), ortho=0.75),
    "top": add_cam("Cam top", (0, 0, 1.6), (0, 0, 0), ortho=0.75),
    "under_rear": add_cam("Cam under rear", (-0.42, -0.30, 0.03), (-0.09, 0.0, 0.17), 35),
    "xm_closeup": add_cam("Cam XM+ pole closeup", (0.16, -0.30, 0.16), (0.0, -0.09, 0.12), 55),
    "jetson_closeup": add_cam("Cam Jetson mount closeup", (-0.30, -0.22, 0.10), (-0.10, 0.0, 0.17), 50),
}
for t in label_objs:
    con = t.constraints.new("TRACK_TO"); con.target = cams["iso_front_left"]; con.track_axis = "TRACK_Z"; con.up_axis = "UP_Y"

sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
sun.data.energy = 4.5
sun.rotation_euler = Euler((math.radians(40), math.radians(10), math.radians(30)))
C_HELP.objects.link(sun)
world = bpy.data.worlds.new("World"); scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.60, 0.68, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6

cams["top"].rotation_euler = (0, 0, math.radians(-90))
scene.camera = cams["iso_front_left"]
bpy.ops.wm.save_as_mainfile(filepath=OUT, compress=True)
print("REPORT" + json.dumps(report))
