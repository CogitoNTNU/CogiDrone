"""Procedural parts for the CogiDrone Blender model (sizes in metres, from datasheets / shop pages).

Parts without a public CAD file are built here from their published dimensions.
"""
import bpy, bmesh, math
from mathutils import Vector, Matrix


def mat(name, rgb, metal=0.0, rough=0.5, alpha=1.0, emit=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    b.inputs["Alpha"].default_value = alpha
    if emit:
        b.inputs["Emission Color"].default_value = (*rgb, 1)
        b.inputs["Emission Strength"].default_value = emit
    if alpha < 1:
        m.surface_render_method = "BLENDED"
    m.diffuse_color = (*rgb, alpha)
    return m


def box(name, size, loc=(0, 0, 0), material=None, parent=None, coll=None, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object
    o.name = name
    o.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = o.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
    if material:
        o.data.materials.append(material)
    if parent:
        o.parent = parent
    if coll:
        for c in o.users_collection:
            c.objects.unlink(o)
        coll.objects.link(o)
    return o


def cyl(name, r, h, loc=(0, 0, 0), rot=(0, 0, 0), material=None, parent=None, coll=None, verts=32):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, location=loc, rotation=rot, vertices=verts)
    o = bpy.context.active_object
    o.name = name
    if material:
        o.data.materials.append(material)
    if parent:
        o.parent = parent
    if coll:
        for c in o.users_collection:
            c.objects.unlink(o)
        coll.objects.link(o)
    return o


def empty(name, loc=(0, 0, 0), coll=None, parent=None):
    o = bpy.data.objects.new(name, None)
    o.location = loc
    o.empty_display_size = 0.02
    (coll or bpy.context.scene.collection).objects.link(o)
    if parent:
        o.parent = parent
    return o


def jetson_orin_nano(coll, M):
    """Jetson Orin Nano (Super) dev kit, 100 x 79 mm carrier, ~31 mm tall incl. heatsink and fan.
    Local origin: centre of the carrier's bottom face, +X = front edge without ports, ports on -X edge."""
    root = empty("Jetson Orin Nano Super", coll=coll)
    box("Jetson carrier PCB", (0.100, 0.079, 0.0016), (0, 0, 0.0048), M["pcb"], root, coll)
    for sx in (-1, 1):  # standoffs/feet at the 86 x 58 mm pattern
        for sy in (-1, 1):
            cyl("Jetson standoff", 0.0025, 0.004, (sx * 0.043, sy * 0.029, 0.002), material=M["metal"], parent=root, coll=coll, verts=12)
    box("Jetson module SO-DIMM", (0.0696, 0.045, 0.0012), (0.006, 0.0, 0.0105), M["pcb_dark"], root, coll)
    hs = box("Jetson heatsink", (0.0696, 0.060, 0.016), (0.006, 0.0, 0.019), M["alu"], root, coll)
    for i in range(13):
        box("Jetson heatsink fin", (0.0696, 0.0012, 0.009), (0.006, -0.027 + i * 0.0045, 0.0315), M["alu"], root, coll)
    cyl("Jetson fan", 0.019, 0.006, (0.006, 0.0, 0.0365), material=M["black"], parent=root, coll=coll)
    # port row along the long -Y edge (as on the real dev kit): barrel, USB-C, DP, 2x2 USB-A, RJ45
    x = -0.046
    for name, w, h, mm in (("DC barrel jack 9-20V", 0.009, 0.011, "black"), ("USB-C (keep free)", 0.009, 0.0035, "metal"),
                           ("DisplayPort", 0.017, 0.006, "metal"), ("USB-A x2 (1)", 0.0145, 0.016, "usb3"),
                           ("USB-A x2 (2)", 0.0145, 0.016, "usb3"), ("RJ45", 0.016, 0.014, "metal")):
        box("Jetson " + name, (w, 0.014, h), (x + w / 2, -0.0365, 0.0056 + h / 2), M[mm], root, coll)
        x += w + 0.0025
    box("Jetson 40-pin header", (0.051, 0.005, 0.0085), (0.012, -0.036, 0.0098), M["black"], root, coll)
    return root


def lipo(name, size, label_rgb, coll, M, connector="XT60", leads=True):
    """LiPo pack in millimetres, size = (L, W, H). Origin: centre. Leads come out of the -X end."""
    root = empty(name, coll=coll)
    box(name + " body", size, (0, 0, 0), M[label_rgb], root, coll, bevel=3)
    L = size[0]
    if not leads:
        box(name + " JST-XH balance", (14, 6, 5), (-L / 2 + 12, size[1] / 2 + 3, 8), M["white"], root, coll)
        return root
    cyl(name + " lead +", 1.8, 50, (-L / 2 - 25, 6, 6), (0, math.pi / 2, 0), M["red"], root, coll, 12)
    cyl(name + " lead -", 1.8, 50, (-L / 2 - 25, -6, 6), (0, math.pi / 2, 0), M["black"], root, coll, 12)
    if connector == "XT60":
        box(name + " XT60 (female)", (16, 16, 8), (-L / 2 - 58, 0, 6), M["xt60"], root, coll)
    else:
        box(name + " Deans (female)", (12, 16, 6), (-L / 2 - 56, 0, 6), M["red"], root, coll)
    box(name + " JST-XH balance", (14, 6, 5), (-L / 2 - 30, -16, 12), M["white"], root, coll)
    return root


def small(name, size, material, coll, parent=None):
    return box(name, size, (0, 0, 0), material, parent, coll, bevel=0.0008)


def wire(name, pts, radius, material, coll, parent=None):
    """Smooth tube through world-space points."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 3
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    for p, co in zip(sp.bezier_points, pts):
        p.co = co
        p.handle_left_type = p.handle_right_type = "AUTO"
    o = bpy.data.objects.new(name, cu)
    o.data.materials.append(material)
    coll.objects.link(o)
    if parent:
        o.parent = parent
    return o


def label(name, text, loc, coll, material, size=0.012, rot=(math.pi / 2, 0, 0)):
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    o = bpy.data.objects.new(name, cu)
    o.location = loc
    o.rotation_euler = rot
    o.data.materials.append(material)
    coll.objects.link(o)
    return o
