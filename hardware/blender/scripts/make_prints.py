"""Design the CogiDrone 3D-printed parts and export STLs (millimetres).

  xmplus_pole_case.stl  - FrSky XM+ case that saddles onto the X500 V2 landing-gear pole (16 mm carbon tube),
                          with two antenna guides at +-45 deg (a 90 deg V pointing down). Held by two zip ties.
  xmplus_pole_lid.stl   - lid with two zip-tie grooves.
  jetson_spacer_10mm.stl- 4 spacers, M2.5, 10 mm, for hanging the Jetson under the platform board.

Run: blender -b --factory-startup --python make_prints.py -- <out dir>
Case local axes: +X = along the pole toward the body (cable end), +Y = across (fore/aft on the drone),
+Z = away from the pole. Origin = pole axis.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

POLE_D = 16.0 + 0.4          # 16 mm tube + clearance
RX = (21.5, 12.0, 3.5)       # XM+ board
POCKET = (24.0, 14.0, 5.5)   # room for the board, solder joints and wires
WALL, FLOOR = 1.6, 2.0
CH_D = 2.2                   # antenna coax channel (IPEX 1.13 coax)
ARM_OD, ARM_LEN = 5.2, 20.0
LID_T = 1.6


def cube(name, size, loc):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object
    o.name = name
    o.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return o


def cylinder(name, r, h, loc, direction, verts=48):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, location=loc, vertices=verts)
    o = bpy.context.active_object
    o.name = name
    o.rotation_euler = Vector((0, 0, 1)).rotation_difference(Vector(direction).normalized()).to_euler()
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    return o


def boolean(target, tool, op):
    m = target.modifiers.new(tool.name, "BOOLEAN")
    m.operation = op
    m.solver = "EXACT"
    m.object = tool
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(tool, do_unlink=True)


def export(o, name):
    for x in bpy.data.objects:
        x.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.wm.stl_export(filepath=os.path.join(OUT, name), export_selected_objects=True, apply_modifiers=True)


# ---------------------------------------------------------------- XM+ pole case
r = POLE_D / 2
base_z = r + 0.8                       # case floor sits just above the pole surface
L = POCKET[0] + 2 * WALL
W = POCKET[1] + 2 * WALL
H = FLOOR + POCKET[2]
body = cube("XM+ pole case", (L, W, H), (0, 0, base_z + H / 2))
# saddle block under the floor, then cut the pole out of it
sad = cube("saddle", (L, W, r + 0.8), (0, 0, (r + 0.8) / 2 + 0.6))
boolean(body, sad, "UNION")
pole = cylinder("pole cut", r, 200, (0, 0, 0), (1, 0, 0), 96)
boolean(body, pole, "DIFFERENCE")
# trim the saddle so it only wraps the top 40 % of the tube
trim = cube("trim", (L + 2, W + 2, 20), (0, 0, -10 + r * 0.35))
boolean(body, trim, "DIFFERENCE")
# antenna V at the bottom (-X) end: two guide arms at +-45 deg from -X (a 90 deg V), tilted 10 deg away
# from the pole so the antennas clear it. Arms are added before the pocket is cut, so the pocket stays clear.
z_ch = base_z + FLOOR + 1.6
starts = []
for s in (1, -1):
    t = 0.0  # arms already clear the tube by ~2 mm
    d = Vector((-math.cos(math.radians(45)) * math.cos(t), s * math.sin(math.radians(45)) * math.cos(t), math.sin(t)))
    p0 = Vector((-POCKET[0] / 2 + 2.0, s * 3.5, z_ch))
    starts.append((p0, d))
    arm = cylinder(f"arm {s}", ARM_OD / 2, ARM_LEN, p0 + d * (ARM_LEN / 2), d, 32)
    boolean(body, arm, "UNION")
# pocket
pocket = cube("pocket", POCKET, (0, 0, base_z + FLOOR + POCKET[2] / 2 + 0.01))
boolean(body, pocket, "DIFFERENCE")
# cable exit notch at the top (+X) end
notch = cube("cable notch", (WALL * 3, 7.0, 3.5), (L / 2, 0, base_z + H - 1.7))
boolean(body, notch, "DIFFERENCE")
for i, (p0, d) in enumerate(starts):
    ch = cylinder(f"channel {i}", CH_D / 2, ARM_LEN + 8, p0 + d * (ARM_LEN / 2 + 2), d, 24)
    boolean(body, ch, "DIFFERENCE")
# nothing may stick out above the lid plane
cap = cube("cap", (200, 200, 20), (0, 0, base_z + H + 10))
boolean(body, cap, "DIFFERENCE")
# keep the arms from poking under the floor plane into the pole
under = cube("under", (L * 3, W * 4, 20), (0, 0, base_z - 10))
keep = body  # arms sit above base_z already
bpy.data.objects.remove(under, do_unlink=True)
export(body, "xmplus_pole_case.stl")
body_dims = list(body.dimensions)

# ---------------------------------------------------------------- lid with zip-tie grooves
lid = cube("XM+ pole lid", (L, W, LID_T), (0, 0, base_z + H + LID_T / 2 + 0.2))
for x in (-6.0, 6.0):
    g = cube("groove", (4.4, W + 2, 0.8), (x, 0, base_z + H + LID_T + 0.2 - 0.39))
    boolean(lid, g, "DIFFERENCE")
lnotch = cube("lid notch", (WALL * 3, 7.0, 3), (L / 2, 0, base_z + H + 0.2))
boolean(lid, lnotch, "DIFFERENCE")
export(lid, "xmplus_pole_lid.stl")

# ---------------------------------------------------------------- Jetson spacers (4x, M2.5, 10 mm)
sp_objs = []
for i in range(4):
    o = cylinder(f"spacer {i}", 3.0, 10.0, (i * 9.0 + 100, 0, 5), (0, 0, 1), 48)
    hole = cylinder(f"spacer hole {i}", 1.45, 12, (i * 9.0 + 100, 0, 5), (0, 0, 1), 32)
    boolean(o, hole, "DIFFERENCE")
    sp_objs.append(o)
for o in sp_objs[1:]:
    boolean(sp_objs[0], o, "UNION")
sp_objs[0].name = "Jetson spacers 10mm"
export(sp_objs[0], "jetson_spacer_10mm_x4.stl")

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "prints.blend"))
print("PRINTS", [round(v, 1) for v in body_dims])
