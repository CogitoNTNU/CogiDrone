"""Jetson power lead and charging setup, built from shaped parts, with the shop photos.

blender -b --factory-startup --python build_wiring_lead.py

Writes jetson_power_lead.blend, jetson_power_lead.png (overview) and
jetson_plug_closeup.png (capacitor + Kjell plug) next to this script.
Real sizes in metres. +X runs battery -> Jetson, +Y is away from the viewer, Z = 0 is the table.
Shapes are simplified: right size and recognisable, not exact CAD.
"""
import math
import os

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "shop_img")

for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
coll = bpy.context.scene.collection


# ---------- helpers ----------
def mat(name, color, metallic=0.0, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = rough
    if emit:
        bsdf.inputs["Emission Color"].default_value = (*color, 1)
        bsdf.inputs["Emission Strength"].default_value = emit
    return m


def img_mat(path):
    m = bpy.data.materials.new(os.path.basename(path))
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(path)
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tex.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    return m, tex.image.size


def link(ob, m=None):
    coll.objects.link(ob)
    if m is not None:
        ob.data.materials.append(m)
    return ob


def smooth(ob):
    bpy.ops.object.shade_smooth()
    if hasattr(ob.data, "set_sharp_from_angle"):
        ob.data.set_sharp_from_angle(angle=math.radians(40))


def box(name, size, loc, m, bevel=0.0, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    ob = bpy.context.object
    ob.name = name
    ob.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 3
        smooth(ob)
    ob.data.materials.append(m)
    return ob


def cyl(name, r, depth, loc, m, axis="Z", verts=32):
    rot = {"X": (0, math.pi / 2, 0), "Y": (math.pi / 2, 0, 0), "Z": (0, 0, 0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, location=loc, rotation=rot, vertices=verts)
    ob = bpy.context.object
    ob.name = name
    ob.data.materials.append(m)
    smooth(ob)
    return ob


def wire(name, pts, r, m):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = r
    cu.bevel_resolution = 4
    cu.use_fill_caps = True
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    return link(bpy.data.objects.new(name, cu), m)


def text(body, loc, size, m, align="CENTER", z=0.0003):
    cu = bpy.data.curves.new("txt", "FONT")
    cu.body = body
    cu.size = size
    cu.align_x = align
    cu.align_y = "CENTER"
    ob = link(bpy.data.objects.new("T " + body[:24], cu), m)
    ob.location = (loc[0], loc[1], z)
    return ob


def card(fname, x, y, lines, target, w=0.056):
    """Shop photo on a white card, caption under it, leader line to the part."""
    m, (iw, ih) = img_mat(os.path.join(IMG, fname))
    h = w * ih / iw
    bpy.ops.mesh.primitive_plane_add(size=1, location=(x, y, 0.0006))
    ph = bpy.context.object
    ph.name = "Photo " + fname
    ph.scale = (w, h, 1)
    ph.data.materials.append(m)
    box("Card " + fname, (w + 0.006, h + 0.006 + 0.022, 0.0004), (x, y - 0.011, 0.0002), M_CARD)
    ty = y - h / 2 - 0.007
    for i, ln in enumerate(lines):
        text(ln, (x, ty - i * 0.0085), 0.0062 if i else 0.0068, M_INK if i else M_INKB, z=0.0009)
    if target:
        tx, tyy, tz = target
        # leave from the caption when the part is below the card, from the top edge when it's above
        sy = ty - len(lines) * 0.0085 + 0.002 if tyy < y else y + h / 2 + 0.003
        wire("Leader " + fname, [(x, sy, 0.001), (tx, tyy, tz)], 0.0004, M_LEAD)
        cyl("Dot " + fname, 0.0016, 0.0016, (tx, tyy, tz), M_LEAD)


# ---------- materials ----------
M_TABLE = mat("Table", (0.80, 0.80, 0.79), rough=0.9)
M_CARD = mat("Card", (1, 1, 1), rough=0.8)
M_INK = mat("Ink", (0.12, 0.12, 0.14))
M_INKB = mat("InkBold", (0.02, 0.02, 0.03))
M_WARN = mat("Warn", (0.75, 0.08, 0.05))
M_LEAD = mat("Leader", (0.45, 0.45, 0.5))
M_RED = mat("Red wire", (0.8, 0.04, 0.03), rough=0.35)
M_BLACK = mat("Black wire", (0.03, 0.03, 0.03), rough=0.35)
M_BAT = mat("Battery shrink", (0.10, 0.20, 0.45), rough=0.3)
M_STICKER = mat("Sticker", (0.92, 0.92, 0.9), rough=0.6)
M_DEANS = mat("Deans red", (0.72, 0.10, 0.07), rough=0.45)
M_XT60 = mat("XT60 yellow", (0.95, 0.72, 0.05), rough=0.45)
M_PLASTIC = mat("Black plastic", (0.04, 0.04, 0.045), rough=0.55)
M_HOLE = mat("Hole", (0.0, 0.0, 0.0), rough=1)
M_FUSE = mat("Fuse 3 A violet", (0.55, 0.25, 0.80), rough=0.15)
M_SHRINK = mat("Heat-shrink", (0.02, 0.02, 0.02), rough=0.3)
M_METAL = mat("Metal", (0.82, 0.82, 0.82), metallic=1, rough=0.25)
M_BRASS = mat("Brass", (0.85, 0.65, 0.3), metallic=1, rough=0.3)
M_ALU = mat("Aluminium", (0.75, 0.76, 0.78), metallic=1, rough=0.35)
M_PCB = mat("PCB green", (0.05, 0.30, 0.15), rough=0.5)
M_PCB_BUZ = mat("PCB blue", (0.05, 0.12, 0.40), rough=0.5)
M_HEATSINK = mat("Heatsink", (0.12, 0.12, 0.13), metallic=0.7, rough=0.35)
M_STRIPE = mat("Cap stripe", (0.78, 0.78, 0.8))
M_USB = mat("USB blue", (0.05, 0.25, 0.85), rough=0.4)
M_CHARGER = mat("Charger", (0.10, 0.10, 0.11), rough=0.45)
M_LCD = mat("LCD", (0.15, 0.45, 0.85), rough=0.2, emit=0.6)
M_7SEG = mat("7-seg red", (1.0, 0.08, 0.05), emit=4.0)
M_JST = mat("JST white", (0.93, 0.92, 0.88), rough=0.5)
M_TERM = mat("Terminal green", (0.20, 0.65, 0.25), rough=0.5)
M_HOLDER = mat("Fuse holder white", (0.94, 0.93, 0.90), rough=0.35)
M_GLASS = mat("Glass", (0.85, 0.92, 0.98), rough=0.05)
BAL_COLS = [(0.03, 0.03, 0.03), (0.9, 0.9, 0.9), (0.9, 0.9, 0.9), (0.9, 0.9, 0.9), (0.8, 0.04, 0.03)]
M_BALS = [mat(f"Balance {i}", c, rough=0.4) for i, c in enumerate(BAL_COLS)]

box("Table", (1.3, 1.0, 0.002), (0.22, -0.06, -0.001), M_TABLE)


# ---------- parts ----------
def lipo(name, x, y, leads="+X"):
    """4S 2200 pack, 120 x 35 x 28 mm, power leads out of one end."""
    s = 1 if leads == "+X" else -1
    box(name, (0.120, 0.035, 0.028), (x, y, 0.014), M_BAT, bevel=0.003)
    box(name + " sticker", (0.080, 0.026, 0.0004), (x - s * 0.008, y, 0.0281), M_STICKER)
    text("4S  2200 mAh  14,8 V  35C", (x - s * 0.008, y + 0.004), 0.0042, M_INKB, z=0.0285)
    text("LiPo", (x - s * 0.008, y - 0.005), 0.0042, M_INK, z=0.0285)
    box(name + " end cap", (0.004, 0.030, 0.022), (x + s * 0.0605, y, 0.014), M_PLASTIC, bevel=0.0015)


def balance_lead(name, pts):
    """5 thin wires side by side (4S balance lead)."""
    for i, m in enumerate(M_BALS):
        dy = (i - 2) * 0.0013
        wire(f"{name} {i}", [(p[0], p[1] + dy, p[2]) for p in pts], 0.0006, m)


def deans_face(name, face, y, z, s, male):
    """The T on a Deans mating face: brass blades (male) or slots (female)."""
    if male:
        box(name + " blade -", (0.007, 0.009, 0.0007), (face + s * 0.0035, y, z + 0.0022), M_BRASS)
        box(name + " blade +", (0.007, 0.0007, 0.006), (face + s * 0.0035, y, z - 0.0008), M_BRASS)
    else:
        box(name + " slot -", (0.0006, 0.010, 0.0015), (face, y, z + 0.0022), M_HOLE)
        box(name + " slot +", (0.0006, 0.0015, 0.0065), (face, y, z - 0.0008), M_HOLE)


def deans(name, x, y, z, male, facing):
    s = 1 if facing == "+X" else -1
    box(name, (0.010, 0.017, 0.009), (x, y, z), M_DEANS, bevel=0.001)
    deans_face(name, x + s * 0.005, y, z, s, male)


def xt60(name, x, y, z, male, facing):
    """XT60: yellow body, two 3.5 mm contacts."""
    s = 1 if facing == "+X" else -1
    box(name, (0.0155, 0.016, 0.008), (x, y, z), M_XT60, bevel=0.0012)
    face = x + s * 0.00775
    for dy in (-0.0035, 0.0035):
        if male:
            cyl(f"{name} pin {dy}", 0.00175, 0.006, (face + s * 0.003, y + dy, z), M_BRASS, axis="X")
        else:
            cyl(f"{name} socket {dy}", 0.0018, 0.0008, (face, y + dy, z), M_HOLE, axis="X")


def fuse_holder(x, y, z):
    """Kjell 41107 inline 5x20 mm holder: white two-part tube, unscrewed a little to show the 4 A glass fuse."""
    gap = 0.008
    cyl("Fuse holder half A", 0.0045, 0.022, (x - 0.011 - gap / 2, y, z), M_HOLDER, axis="X")
    cyl("Fuse holder grip A", 0.0050, 0.006, (x - 0.004 - gap / 2, y, z), M_HOLDER, axis="X")
    cyl("Fuse holder half B", 0.0045, 0.022, (x + 0.011 + gap / 2, y, z), M_HOLDER, axis="X")
    cyl("Glass fuse 4 A T", 0.0025, 0.012, (x, y, z), M_GLASS, axis="X")
    for dx in (-0.0068, 0.0068):
        cyl(f"Glass fuse cap {dx}", 0.0027, 0.0032, (x + dx, y, z), M_METAL, axis="X")
    wire("Glass fuse element", [(x - 0.005, y, z), (x, y + 0.0008, z), (x + 0.005, y, z)], 0.00015, M_PLASTIC)


def delock_plug(x, y, z):
    """DeLOCK 5.5 x 2.5 mm male plug: green 2-pin screw terminal (x = its centre), ribbed black body, metal barrel."""
    box("DeLOCK terminal (green)", (0.010, 0.014, 0.011), (x, y, z), M_TERM, bevel=0.0006)
    for dy, sign in ((0.0035, "+"), (-0.0035, "-")):
        cyl(f"Screw {sign}", 0.0021, 0.0012, (x + 0.001, y + dy, z + 0.0058), M_METAL)
        box(f"Screw slot {sign}", (0.0035, 0.0005, 0.0004), (x + 0.001, y + dy, z + 0.0065), M_HOLE)
        box(f"Wire hole {sign}", (0.0005, 0.0028, 0.0028), (x - 0.005, y + dy, z), M_HOLE)
        text(sign, (x - 0.003, y + dy), 0.0045 if sign == "+" else 0.006, M_WARN if sign == "+" else M_STRIPE, z=z + 0.0057)
    box("DeLOCK body", (0.016, 0.012, 0.012), (x + 0.013, y, z), M_PLASTIC, bevel=0.0012)
    for i in range(4):
        box(f"DeLOCK rib {i}", (0.0012, 0.0128, 0.0128), (x + 0.008 + i * 0.0035, y, z), M_PLASTIC, bevel=0.0004)
    cyl("Barrel sleeve (-)", 0.00275, 0.010, (x + 0.026, y, z), M_METAL, axis="X")
    cyl("Barrel hole (+ pin inside)", 0.00125, 0.0004, (x + 0.0311, y, z), M_HOLE, axis="X")


def capacitor(x, y, legs_to):
    """Radial 470 uF 35 V (10 x 20 mm), standing; stripe (-) faces -Y; legs run to legs_to['+'], legs_to['-']."""
    zc = 0.013
    cyl("Cap 470uF", 0.005, 0.020, (x, y, zc), M_PLASTIC)
    cyl("Cap top", 0.0047, 0.0004, (x, y, zc + 0.0101), M_ALU)
    box("Cap vent a", (0.006, 0.0004, 0.0002), (x, y, zc + 0.0104), M_HOLE)
    box("Cap vent b", (0.0004, 0.006, 0.0002), (x, y, zc + 0.0104), M_HOLE)
    for i, a in enumerate((-22, 0, 22)):
        r = math.radians(a - 90)
        box(f"Cap stripe {i}", (0.0022, 0.0006, 0.019), (x + 0.0049 * math.cos(r), y + 0.0049 * math.sin(r), zc),
            M_STRIPE, rot=(0, 0, r + math.pi / 2))
    wire("Cap leg + (long)", [(x, y + 0.0025, 0.003), (x + 0.004, y + 0.004, 0.0015), legs_to["+"]], 0.00035, M_METAL)
    wire("Cap leg - (stripe)", [(x, y - 0.0025, 0.003), (x + 0.004, y - 0.004, 0.0015), legs_to["-"]], 0.00035, M_METAL)


def buzzer(x, y):
    """2-8S LiPo alarm: blue PCB, 3-digit red display, buzzer can, 9-pin header on the -X edge."""
    box("Buzzer PCB", (0.038, 0.022, 0.0016), (x, y, 0.004), M_PCB_BUZ)
    for i in range(4):
        cyl(f"Buzzer standoff {i}", 0.0012, 0.0032, (x + (-0.016 if i % 2 else 0.016), y + (-0.009 if i < 2 else 0.009), 0.0016), M_PLASTIC)
    box("Buzzer display", (0.018, 0.012, 0.005), (x + 0.002, y, 0.0073), M_PLASTIC)
    text("3.85", (x + 0.002, y), 0.0075, M_7SEG, z=0.0099)
    cyl("Buzzer can", 0.0055, 0.006, (x + 0.0145, y, 0.0078), M_PLASTIC)
    cyl("Buzzer hole", 0.0009, 0.0002, (x + 0.0145, y, 0.0109), M_HOLE)
    for i in range(9):
        box(f"Buzzer pin {i}", (0.006, 0.0006, 0.0006), (x - 0.021, y - 0.0080 + i * 0.002, 0.0048), M_BRASS)
    box("JST-XH on pins (- end)", (0.006, 0.012, 0.005), (x - 0.0225, y - 0.0045, 0.0055), M_JST, bevel=0.0005)


def jetson(x, y):
    """Orin Nano dev kit, 100 x 79 mm, ports along the -X edge. (x, y) = DC jack opening on that edge."""
    cx, cy = x + 0.0395, y + 0.035
    for i, (dx, dy) in enumerate(((-0.035, -0.045), (0.035, -0.045), (-0.035, 0.045), (0.035, 0.045))):
        cyl(f"Jetson foot {i}", 0.003, 0.004, (cx + dx, cy + dy, 0.002), M_PLASTIC)
    box("Jetson carrier PCB", (0.079, 0.100, 0.0016), (cx, cy, 0.0048), M_PCB)
    # rear-edge ports from the plug side: DC jack, RJ45, USB, USB, DisplayPort
    box("DC jack", (0.013, 0.009, 0.011), (x + 0.0065, y, 0.0111), M_PLASTIC, bevel=0.0006)
    cyl("DC jack opening", 0.0029, 0.0004, (x - 0.0001, y, 0.0111), M_HOLE, axis="X")
    cyl("DC jack centre pin", 0.00125, 0.0006, (x - 0.0003, y, 0.0111), M_METAL, axis="X")
    box("RJ45", (0.021, 0.016, 0.0135), (x + 0.0105, y + 0.021, 0.0124), M_METAL, bevel=0.0004)
    box("RJ45 opening", (0.0004, 0.012, 0.009), (x - 0.0001, y + 0.021, 0.0118), M_HOLE)
    for j, dy in enumerate((0.0405, 0.0585)):
        box(f"USB stack {j}", (0.017, 0.0145, 0.0155), (x + 0.0085, y + dy, 0.0134), M_METAL, bevel=0.0004)
        for k, dz in enumerate((-0.0035, 0.0040)):
            box(f"USB {j}{k} opening", (0.0004, 0.0125, 0.005), (x - 0.0001, y + dy, 0.0134 + dz), M_HOLE)
            box(f"USB {j}{k} tongue", (0.008, 0.0105, 0.0017), (x + 0.003, y + dy, 0.0134 + dz - 0.0010), M_USB)
    box("DisplayPort", (0.010, 0.017, 0.006), (x + 0.005, y + 0.077, 0.0088), M_METAL, bevel=0.0004)
    # module heatsink + fan
    hx, hy = cx + 0.006, cy + 0.004
    box("Heatsink base", (0.060, 0.070, 0.004), (hx, hy, 0.0080), M_HEATSINK)
    for i in range(16):
        box(f"Fin {i}", (0.058, 0.0012, 0.016), (hx, hy - 0.033 + i * 0.0044, 0.0180), M_HEATSINK)
    box("Fan frame", (0.042, 0.042, 0.007), (hx, hy, 0.0295), M_PLASTIC, bevel=0.002)
    cyl("Fan opening", 0.0195, 0.0002, (hx, hy, 0.0331), M_HOLE, verts=48)
    cyl("Fan hub", 0.0075, 0.0012, (hx, hy, 0.0334), M_PLASTIC)
    for i in range(7):
        a = i * 2 * math.pi / 7
        box(f"Fan blade {i}", (0.012, 0.0045, 0.0006), (hx + 0.013 * math.cos(a), hy + 0.013 * math.sin(a), 0.0333),
            M_PLASTIC, rot=(math.radians(20), 0, a))
    text("NVIDIA", (hx, hy - 0.0255), 0.0040, M_STICKER, z=0.0335)


def charger(x, y):
    """SkyRC e450-style charger: dark box, blue screen, buttons, side vents, balance port on the +X face."""
    box("Charger body", (0.100, 0.090, 0.040), (x, y, 0.020), M_CHARGER, bevel=0.004)
    box("Charger screen", (0.050, 0.028, 0.0006), (x - 0.010, y + 0.014, 0.0402), M_LCD)
    text("LiPo 4S  2.2A", (x - 0.010, y + 0.018), 0.0050, M_STICKER, z=0.041)
    text("BAL  16.80V", (x - 0.010, y + 0.009), 0.0045, M_STICKER, z=0.041)
    for i in range(4):
        cyl(f"Charger button {i}", 0.0035, 0.002, (x - 0.030 + i * 0.013, y - 0.022, 0.0405), M_PLASTIC)
    for i in range(6):
        box(f"Vent {i}", (0.004, 0.0006, 0.022), (x - 0.025 + i * 0.010, y - 0.0452, 0.020), M_HOLE)
    box("Balance port", (0.0006, 0.016, 0.006), (x + 0.0502, y + 0.025, 0.012), M_JST)
    text("e450", (x + 0.030, y + 0.014), 0.006, M_STICKER, z=0.0405)


# ================= ROW 1: Jetson power lead (y = 0) =================
Z = 0.006  # wire height
text("JETSON-STRØMLEDNING  (ingen omformer: batteriet går rett inn i Jetsonen)", (-0.11, 0.255), 0.011, M_INKB, align="LEFT")

lipo("Jetson battery", 0.0, 0.0)
text("Jetson-batteri 4S 2200 mAh (har)", (0.0, -0.032), 0.0062, M_INKB)
wire("Bat +", [(0.0625, 0.006, 0.014), (0.080, 0.006, Z + 0.002), (0.095, 0.004, 0.0065)], 0.0012, M_RED)
wire("Bat -", [(0.0625, -0.006, 0.014), (0.080, -0.006, Z + 0.002), (0.095, -0.004, 0.0065)], 0.0012, M_BLACK)
deans("Deans female (battery)", 0.100, 0.0, 0.0065, male=False, facing="+X")
deans("Deans male (lead)", 0.1145, 0.0, 0.0065, male=True, facing="-X")
text("Deans", (0.107, -0.016), 0.0055, M_INK)
text("+", (0.088, 0.016), 0.010, M_WARN)
text("-", (0.088, -0.016), 0.012, M_INKB)

balance_lead("Balance", [(-0.045, 0.0175, 0.022), (-0.045, 0.040, 0.006), (-0.0255, 0.0595, 0.0055)])
buzzer(0.020, 0.064)
text("buzzer, alarm 3,5 V/celle", (0.072, 0.066), 0.0055, M_INK, align="LEFT")
text("ta av etter bruk!", (0.072, 0.058), 0.0055, M_WARN, align="LEFT")

# Red: Deans -> 18 AWG -> splice -> 12 AWG holder -> fuse -> 12 AWG -> splice -> 18 AWG -> + screw
YR, YB = 0.030, -0.030
KX = 0.372  # DeLOCK terminal block centre
KZ = 0.0065
wire("Red 18AWG a", [(0.1195, 0.002, KZ), (0.130, 0.020, Z), (0.145, YR, Z), (0.150, YR, Z)], 0.0011, M_RED)
cyl("Splice 1 (5 mm shrink)", 0.0028, 0.014, (0.156, YR, Z), M_SHRINK, axis="X")
wire("Holder lead in (1.25 mm2)", [(0.162, YR, Z), (0.175, YR, Z)], 0.0011, M_RED)
fuse_holder(0.200, YR, Z)
wire("Holder lead out (1.25 mm2)", [(0.225, YR, Z), (0.238, YR, Z)], 0.0011, M_RED)
cyl("Splice 2 (5 mm shrink)", 0.0028, 0.014, (0.244, YR, Z), M_SHRINK, axis="X")
wire("Red 18AWG b", [(0.251, YR, Z), (0.325, YR, Z), (0.352, 0.008, KZ), (KX - 0.0055, 0.0035, KZ)], 0.0011, M_RED)
text("4 A treg glassikring, nær batteriet", (0.200, 0.052), 0.0055, M_INK)
text("skjøt", (0.156, 0.019), 0.005, M_INK)
text("skjøt", (0.244, 0.019), 0.005, M_INK)

wire("Black 18AWG", [(0.1195, -0.002, KZ), (0.130, -0.020, Z), (0.145, YB, Z), (0.325, YB, Z), (0.352, -0.008, KZ),
                     (KX - 0.0055, -0.0035, KZ)], 0.0011, M_BLACK)
text("sort 18 AWG, ingen sikring", (0.220, -0.042), 0.0055, M_INK)

delock_plug(KX, 0.0, KZ)
capacitor(0.338, 0.0, {"+": (KX - 0.0055, 0.0028, KZ - 0.0006), "-": (KX - 0.0055, -0.0028, KZ - 0.0006)})
text("DeLOCK DC-plugg 5,5 x 2,5", (0.385, 0.040), 0.0055, M_INK)
text("kondensatorbeina skrus inn", (0.360, -0.050), 0.0055, M_WARN)
text("sammen med ledningene", (0.360, -0.058), 0.0055, M_WARN)

jetson(0.4045, 0.0)
text("Jetson Orin Nano (har)", (0.452, -0.050), 0.0062, M_INKB)
text("DC-inngang 9-20 V, maks 3,5 A", (0.452, -0.059), 0.0055, M_INK)
text("Mål før tilkobling: +12 til +16,8 V", (0.300, -0.080), 0.0062, M_WARN)
text("på midtpinnen, Jetsonen frakoblet", (0.300, -0.089), 0.0062, M_WARN)

CY = 0.190
card("buzzer.jpg", -0.075, CY, ["Buzzer 2-8S", "elefun 95 kr"], (0.022, 0.064, 0.010))
card("deans.jpg", -0.005, CY, ["Deans hann 3 stk", "elefun 29 kr"], (0.1145, 0.0, 0.011))
card("wire.jpg", 0.065, CY, ["Silikon 18 AWG", "elefun 39 kr"], (0.135, YR - 0.004, Z + 0.001))
card("shrink5.jpg", 0.135, CY, ["Krympestrømpe 5 mm", "elefun 45 kr"], (0.156, YR, Z + 0.003))
card("glass_holder.jpg", 0.205, CY, ["Sikringsholder 5x20", "Kjell 9 kr"], (0.211, YR, Z + 0.005))
card("glass_fuse_4a.jpg", 0.275, CY, ["Glassikring 4 A treg", "Kjell 39 kr"], (0.200, YR, Z + 0.003))
card("cap.jpg", 0.345, CY, ["Kondens. 470 uF 35 V", "elefun 12 kr"], (0.338, 0.0, 0.0235))
card("delock_plug.png", 0.415, CY, ["DeLOCK DC-plugg", "Proshop 70 kr"], (KX + 0.013, 0.0, KZ + 0.006))
card("ssd.png", 0.485, CY, ["SSD 250 GB NVMe", "Proshop 676 kr"], (0.450, 0.040, 0.034))
text("SSD-en går i M.2-sporet under Jetsonen", (0.485, 0.122), 0.0048, M_INK)

# ================= ROW 2: charging (y = -0.21) =================
Y2 = -0.21
text("LADING  (på benken, under oppsyn)", (-0.11, -0.115), 0.013, M_INKB, align="LEFT")
charger(-0.045, Y2)
text("SkyRC e450 (har)", (-0.045, Y2 - 0.058), 0.0062, M_INKB)
text("LiPo, 4S, 2,2 A", (-0.045, Y2 - 0.067), 0.0055, M_INK)
wire("Charge +", [(0.005, Y2 + 0.004, 0.010), (0.035, Y2 + 0.004, Z), (0.064, Y2 + 0.0035, 0.006)], 0.0013, M_RED)
wire("Charge -", [(0.005, Y2 - 0.004, 0.010), (0.035, Y2 - 0.004, Z), (0.064, Y2 - 0.0035, 0.006)], 0.0013, M_BLACK)
xt60("XT60 male (charger lead)", 0.0720, Y2, 0.006, male=True, facing="+X")
# Bronto adapter: female XT60 moulded onto a black body that ends in a male Deans
xt60("XT60 female (adapter)", 0.0895, Y2, 0.006, male=False, facing="-X")
box("Adapter body", (0.012, 0.016, 0.009), (0.1032, Y2, 0.0065), M_PLASTIC, bevel=0.001)
deans_face("Adapter Deans", 0.1092, Y2, 0.0065, 1, male=True)
deans("Deans female (battery) 2", 0.1215, Y2, 0.0065, male=False, facing="-X")
text("XT60", (0.081, Y2 - 0.016), 0.0055, M_INK)
text("Deans", (0.117, Y2 - 0.016), 0.0055, M_INK)
wire("Bat2 +", [(0.1265, Y2 + 0.004, 0.0065), (0.150, Y2 + 0.006, Z + 0.002), (0.1775, Y2 + 0.006, 0.014)], 0.0012, M_RED)
wire("Bat2 -", [(0.1265, Y2 - 0.004, 0.0065), (0.150, Y2 - 0.006, Z + 0.002), (0.1775, Y2 - 0.006, 0.014)], 0.0012, M_BLACK)
lipo("Jetson battery (charging)", 0.240, Y2, leads="-X")
text("Jetson-batteriet", (0.240, Y2 - 0.032), 0.0062, M_INKB)
balance_lead("Balance charge", [(0.285, Y2 + 0.0175, 0.022), (0.27, Y2 + 0.055, 0.006), (0.08, Y2 + 0.062, 0.006),
                                (0.0055, Y2 + 0.025, 0.012)])
text("balanseledning til laderen", (0.150, Y2 + 0.072), 0.0055, M_INK)
text("Ladeplass: metallboks / betonggulv, aldri ubevoktet", (0.240, Y2 - 0.050), 0.0055, M_WARN)

card("deans_xt60.jpg", 0.095, -0.300, ["Overgang Deans->XT60", "elefun 59 kr"], (0.097, Y2 - 0.005, 0.011))
card("shrink2.jpg", 0.440, -0.215, ["Krympestrømpe 2 mm", "elefun 36 kr"], None)
text("(til TELEM2-krysskabelen,", (0.440, -0.282), 0.0048, M_INK)
text("ikke en del av denne)", (0.440, -0.290), 0.0048, M_INK)

# ---------- cameras, light, render ----------
scene = bpy.context.scene


def add_cam(name, loc, look_at, ortho=None, lens=50):
    cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
    coll.objects.link(cam)
    cam.location = loc
    if ortho:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = ortho
    else:
        cam.data.lens = lens
    cam.data.clip_start = 0.005
    tgt = bpy.data.objects.new(name + " target", None)
    coll.objects.link(tgt)
    tgt.location = look_at
    c = cam.constraints.new("TRACK_TO")
    c.target = tgt
    c.track_axis = "TRACK_NEGATIVE_Z"
    c.up_axis = "UP_Y"
    return cam


tilt = math.radians(42)
target = (0.20, -0.04, 0.0)
cam_all = add_cam("Cam overview", (target[0], target[1] - math.sin(tilt), math.cos(tilt)), target, ortho=0.72)
cam_plug = add_cam("Cam plug closeup", (0.315, -0.095, 0.085), (0.365, 0.0, 0.008), lens=55)

sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
coll.objects.link(sun)
sun.data.energy = 2.4
sun.data.angle = math.radians(15)
sun.rotation_euler = (math.radians(40), math.radians(-20), math.radians(25))
world = scene.world or bpy.data.worlds.new("World")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (1, 1, 1, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.45

for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng
        break
    except TypeError:
        pass
scene.view_settings.view_transform = "Standard"
scene.camera = cam_all

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "jetson_power_lead.blend"))
bpy.ops.file.make_paths_relative()
bpy.ops.wm.save_mainfile()

for cam, fname, res in ((cam_all, "jetson_power_lead.png", (2000, 1560)), (cam_plug, "jetson_plug_closeup.png", (1600, 1100))):
    scene.camera = cam
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = os.path.join(HERE, fname)
    bpy.ops.render.render(write_still=True)
