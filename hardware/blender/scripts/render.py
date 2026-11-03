"""Render every 'Cam ...' camera in the built .blend to PNGs."""
import bpy, sys, os

out = sys.argv[sys.argv.index("--") + 1]
os.makedirs(out, exist_ok=True)
sc = bpy.context.scene
try:
    sc.render.engine = "BLENDER_EEVEE"
except TypeError:
    sc.render.engine = "BLENDER_EEVEE_NEXT"
sc.render.resolution_x, sc.render.resolution_y = 1600, 1100
sc.render.film_transparent = False
sc.view_settings.view_transform = "AgX"
try:
    sc.eevee.taa_render_samples = 32
except AttributeError:
    pass
labels = [o for o in bpy.data.objects if o.name.startswith("Label ")]
for t in labels:
    t.hide_render = True
for cam in [o for o in bpy.data.objects if o.type == "CAMERA"]:
    sc.camera = cam
    for t in labels:
        for c in t.constraints:
            c.target = cam
    sc.render.filepath = os.path.join(out, cam.name.replace("Cam ", "").replace(" ", "_").replace("(", "").replace(")", "") + ".png")
    bpy.ops.render.render(write_still=True)
    print("RENDERED", sc.render.filepath)
