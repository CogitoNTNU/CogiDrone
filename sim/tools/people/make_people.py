#!/usr/bin/env python3
"""Make many differently dressed people from the three MakeHuman-based Gazebo Fuel people.

Walking person, Standing person and Casual female (OpenRobotics on Gazebo Fuel) keep every
garment, the skin and the hair in separate textures. Each variant gets its own top colour
(easy to spot: hi-vis, red, orange; hard to spot: dark, green, brown, white, grey), trousers,
skin tone and hair colour. Injured variants get dark blood stains on the clothes. Search and
rescue detection should not be tested on six identical people.

    python3 sim/tools/people/make_people.py        # -> sim/models/person_<base>_<nn>/ (git-ignored)

Needs the three Fuel models in ~/.gz/fuel (Gazebo downloads them the first time a world uses
them, or: gz fuel download -u "https://fuel.gazebosim.org/1.0/OpenRobotics/models/Walking person").
Writes sim/models/people.json: name -> base, colours, injured, posture it suits.
"""

import glob
import json
import os
import shutil
import sys

import numpy as np
from PIL import Image

SIM = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MODELS = os.path.join(SIM, "models")
FUEL = os.path.expanduser("~/.gz/fuel/fuel.gazebosim.org/openrobotics/models")

# base -> (Fuel folder, mesh, {texture file: role})
BASES = {
    "walking": ("walking person", "walking.dae", {
        "tshirt02_texture.png": "top", "jeans_basic_diffuse.png": "bottom",
        "young_lightskinned_male_diffuse.png": "skin", "male02_diffuse_black.png": "hair"}),
    "standing": ("standing person", "standing.dae", {
        "tshirt02_texture.png": "top", "jeans_basic_diffuse.png": "bottom",
        "young_lightskinned_male_diffuse.png": "skin", "male02_diffuse_black.png": "hair"}),
    "female": ("casual female", "casual_female.dae", {
        "female_casualsuit01_diffuse.png": "top", "female_casualsuit02_diffuse.png": "top",
        "young_lightskinned_female_diffuse.png": "skin", "ponytail01_diffuse.png": "hair"}),
}

TOPS = {  # outdoor clothing seen in Bymarka, from easy to hard to spot from the air
    "hivis": (0.85, 0.92, 0.10), "red": (0.72, 0.07, 0.06), "orange": (0.92, 0.40, 0.05),
    "lightblue": (0.32, 0.55, 0.82), "purple": (0.40, 0.16, 0.45), "white": (0.86, 0.86, 0.84),
    "grey": (0.42, 0.43, 0.44), "navy": (0.07, 0.10, 0.24), "black": (0.05, 0.05, 0.06),
    "forest": (0.17, 0.27, 0.13), "olive": (0.30, 0.31, 0.17), "brown": (0.38, 0.27, 0.17),
}
BOTTOMS = {"jeans": None, "black": (0.06, 0.06, 0.07), "khaki": (0.46, 0.40, 0.27),
           "green": (0.16, 0.22, 0.13), "grey": (0.33, 0.34, 0.35), "navy": (0.08, 0.10, 0.20)}
SKIN = [(1.0, 1.0, 1.0), (0.93, 0.86, 0.78), (0.78, 0.63, 0.52), (0.58, 0.44, 0.34), (0.40, 0.29, 0.22)]
HAIR = [None, (0.35, 0.22, 0.12), (0.85, 0.70, 0.45), (0.55, 0.55, 0.55), (0.55, 0.25, 0.10)]

# 10 variants per base: (top, bottom, skin, hair, injured)
PLAN = [("hivis", "black", 0, 1, False), ("red", "jeans", 2, 0, False), ("navy", "khaki", 1, 2, True),
        ("forest", "green", 3, 0, False), ("black", "black", 0, 3, True), ("white", "grey", 4, 0, False),
        ("orange", "navy", 1, 4, False), ("brown", "khaki", 2, 1, True), ("olive", "jeans", 0, 2, False),
        ("lightblue", "grey", 3, 0, False), ("grey", "black", 1, 3, False), ("purple", "jeans", 2, 1, True)]


def tint(img, colour, strength=1.0):
    """Recolour a garment texture: keep its shading (luminance), replace the colour."""
    a = np.asarray(img.convert("RGBA"), np.float32) / 255
    rgb, alpha = a[..., :3], a[..., 3:]
    lum = rgb @ np.array([0.299, 0.587, 0.114], np.float32)
    m = np.median(lum[alpha[..., 0] > 0.5]) if (alpha > 0.5).any() else lum.mean()
    shade = np.clip(lum / max(m, 1e-3), 0, 2.2)[..., None]
    new = np.clip(shade * np.array(colour, np.float32) * 0.95, 0, 1)
    out = rgb * (1 - strength) + new * strength
    return Image.fromarray((np.concatenate([out, alpha], 2) * 255).astype(np.uint8), "RGBA")


def multiply(img, mul):
    a = np.asarray(img.convert("RGBA"), np.float32) / 255
    a[..., :3] *= np.array(mul, np.float32)
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), "RGBA")


def blood(img, rng, n=None):
    """A few soaked, dark-red stains with soft edges (dried blood, not gore)."""
    a = np.asarray(img.convert("RGBA"), np.float32) / 255
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    mask = np.zeros((h, w), np.float32)
    for _ in range(n or rng.integers(2, 5)):
        cx, cy = rng.uniform(0.15, 0.85) * w, rng.uniform(0.15, 0.85) * h
        r = rng.uniform(0.04, 0.12) * w
        ang = rng.uniform(0, np.pi)
        dx, dy = xx - cx, yy - cy
        u = (dx * np.cos(ang) + dy * np.sin(ang)) / r
        v = (-dx * np.sin(ang) + dy * np.cos(ang)) / (r * rng.uniform(0.4, 0.9))
        d = np.sqrt(u * u + v * v) + 0.25 * np.sin(5 * np.arctan2(v, u) + rng.uniform(0, 6))
        mask = np.maximum(mask, np.clip((1.1 - d) * 3, 0, 1))
    stain = np.array((0.30, 0.03, 0.03), np.float32)
    lum = a[..., :3].mean(2, keepdims=True)
    a[..., :3] = a[..., :3] * (1 - 0.85 * mask[..., None]) + 0.85 * mask[..., None] * stain * (0.6 + 0.6 * lum)
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), "RGBA")


def build(base, idx, plan, rng):
    folder, mesh, roles = BASES[base]
    src = sorted(glob.glob(os.path.join(FUEL, folder, "*")), key=lambda p: int(os.path.basename(p)))
    if not src:
        sys.exit(f"Fuel model '{folder}' is not in {FUEL}; start a world that uses it once, or gz fuel download it")
    src = src[-1]
    top, bottom, skin, hair, injured = plan
    name = f"person_{base}_{idx:02d}"
    out = os.path.join(MODELS, name)
    shutil.rmtree(out, ignore_errors=True)
    shutil.copytree(os.path.join(src, "meshes"), os.path.join(out, "meshes"))
    shutil.copytree(os.path.join(src, "materials"), os.path.join(out, "materials"))
    tex = os.path.join(out, "materials", "textures")
    for f, role in roles.items():
        p = os.path.join(tex, f)
        if not os.path.exists(p):
            continue
        img = Image.open(p)
        if role == "top":
            # the female suit has two garment textures: jacket in the top colour, the other darker
            col = TOPS[top] if f != "female_casualsuit02_diffuse.png" else (BOTTOMS[bottom] or (0.12, 0.16, 0.3))
            img = tint(img, col)
        elif role == "bottom" and BOTTOMS[bottom] is not None:
            img = tint(img, BOTTOMS[bottom])
        elif role == "skin":
            img = multiply(img, SKIN[skin])
        elif role == "hair" and HAIR[hair] is not None:
            img = tint(img, HAIR[hair], 0.8)
        if injured and role in ("top", "bottom"):
            img = blood(img, rng)
        img.save(p)
    # Gazebo's renderer caches textures by file name, so every variant needs its own names.
    for f in os.listdir(tex):
        os.rename(os.path.join(tex, f), os.path.join(tex, f"{name}_{f}"))
    dae = os.path.join(out, "meshes", mesh)
    text = open(dae, encoding="utf-8").read()
    for f in os.listdir(tex):
        orig = f[len(name) + 1:]
        text = text.replace(f"textures/{orig}<", f"textures/{f}<")
    open(dae, "w", encoding="utf-8").write(text)
    uri = f"model://{name}/meshes/{mesh}"
    with open(os.path.join(out, "model.config"), "w") as f:
        f.write(f"""<?xml version="1.0"?>
<model><name>{name}</name><version>1.0</version><sdf version="1.9">model.sdf</sdf>
<description>Recoloured copy of the Gazebo Fuel model "{folder}" (OpenRobotics), made by
sim/tools/people/make_people.py. Top {top}, bottom {bottom}, skin tone {skin}, injured {injured}.</description>
</model>
""")
    with open(os.path.join(out, "model.sdf"), "w") as f:
        f.write(f"""<?xml version="1.0"?>
<sdf version="1.9">
  <model name="{name}">
    <static>true</static>
    <link name="link">
      <collision name="body"><pose>0 0 0.9 0 0 0</pose><geometry><box><size>0.45 0.3 1.8</size></box></geometry></collision>
      <visual name="visual"><pose>0 0 -0.02 0 0 0</pose><geometry><mesh><uri>{uri}</uri></mesh></geometry></visual>
    </link>
  </model>
</sdf>
""")
    return name, {"base": base, "top": top, "bottom": bottom, "skin": skin, "hair": hair, "injured": injured}


def main():
    rng = np.random.default_rng(7)
    index = {}
    for base in BASES:
        for i, plan in enumerate(PLAN if base != "female" else PLAN[::-1]):
            name, info = build(base, i, plan, rng)
            index[name] = info
            print(name, info)
    with open(os.path.join(MODELS, "people.json"), "w") as f:
        json.dump(index, f, indent=1)
    print(f"{len(index)} people in sim/models/person_*")


if __name__ == "__main__":
    main()
