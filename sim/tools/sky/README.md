# Photographic sky for Gazebo Harmonic (ogre2)

Replaces Gazebo's default procedural sky with a real photographic Nordic
summer sky (blue sky, scattered cumulus clouds) baked from a CC0 Poly Haven
HDRI, with a matching sun `<light>` so shadows line up with the sky.

Produces:
- `sim/assets/sky/skybox.dds` — 6-face cubemap (1024² per face, uncompressed
  RGBA8), same binary layout as Gazebo's stock
  `gz-rendering8/ogre2/media/materials/textures/skybox.dds`.
- `sim/assets/sky/sun_light.sdf.xml` — a ready-to-paste `<light
  type="directional" name="sun">` snippet whose `<direction>` and
  `<diffuse>` match the sky.
- `sim/assets/sky/ATTRIBUTION.md` — HDRI source + license.
- `sim/assets/sky/_preview_equirect.png` — tone-mapped equirect preview
  (not used at runtime, just for eyeballing the source).

## Why this is needed (Harmonic quirk)

In Gazebo Harmonic, `<scene><sky></sky></scene>` only *enables* the
built-in procedural sky; SDF's `<sky><cubemap_uri>` is parsed but **silently
ignored** by ogre2 — there is no supported way to point the sky at a custom
texture via SDF. The built-in sky always samples
`gz-rendering8/ogre2/media/materials/textures/skybox.dds` from
gz-rendering's install media dir.

gz-rendering *does* honour the environment variable
`GZ_RENDERING_RESOURCE_PATH` to override its entire ogre2 media root — so
the workaround is: build a full copy of that media tree with our own
`skybox.dds` substituted in, and point `GZ_RENDERING_RESOURCE_PATH` at the
copy before starting `gz sim`.

**Important:** `GZ_RENDERING_RESOURCE_PATH` does **not** accept multiple
colon-separated search paths (tested empirically — passing `A:B` makes
Ogre2 treat the literal string `"A:B"` as one broken path and fail to find
*anything*, including shader templates it needs for things unrelated to the
sky). So the override directory must contain the *entire* stock ogre2 media
tree (shaders, Hlms scripts, PBR data), not just the sky texture, or every
other material in the scene breaks too.

## Usage

```bash
# 1. Generate sim/assets/sky/skybox.dds + sun_light.sdf.xml (downloads the
#    HDRI once into ~/cogidrone_deps/simassets-cache/sky, ~5s to run after
#    that; needs numpy + pillow, both already in system python3).
python3 make_sky.py

# 2. Build the local (not committed) media overlay directory that
#    GZ_RENDERING_RESOURCE_PATH needs. Re-run this after any gz-rendering
#    upgrade, or whenever skybox.dds changes.
python3 build_media_overlay.py
#   -> prints: export GZ_RENDERING_RESOURCE_PATH=~/cogidrone_deps/simassets-cache/sky/media_root

# 3. In your world SDF:
#    - add <scene><sky></sky></scene>
#    - paste the contents of sim/assets/sky/sun_light.sdf.xml as your
#      directional light (or merge its <direction>/<diffuse> into an
#      existing sun light)

# 4. Run gz sim with the env var set, e.g.:
export GZ_RENDERING_RESOURCE_PATH=~/cogidrone_deps/simassets-cache/sky/media_root
gz sim -r your_world.sdf
```

If you don't set `GZ_RENDERING_RESOURCE_PATH`, Gazebo falls back to its
normal default sky — nothing breaks, you just don't get the custom one.

Camera **sensors** (and therefore anything recorded for SAR/vision work)
pick up the custom sky as long as the sensors-system process has
`GZ_RENDERING_RESOURCE_PATH` set in its environment when it starts (verified
headless, see below). The **GUI** client should behave the same way since it
uses the same gz-rendering ogre2 engine and resource-path resolution, but
this was only verified via headless camera sensors here — **GUI display is
unverified**, confirm it visually before relying on it for demos.

## How the cubemap face mapping was figured out

Gazebo's `skybox.dds` is a standard D3D/Ogre cubemap: 6 faces in the fixed
slot order +X,-X,+Y,-Y,+Z,-Z (`caps2 = DDSCAPS2_CUBEMAP |
DDSCAPS2_CUBEMAP_ALLFACES = 0xFE00`, single mip level, verified against the
stock file's own DDS header). What was **not** documented anywhere we could
find is which *world* direction (Gazebo's ENU: x=east, y=north, z=up) each
of those 6 slots actually displays — the cubemap is sampled directly by the
skybox shader using the view-rotated vertex position, so it depends on
gz-rendering/Ogre's internal axis convention, not on SDF/ENU.

We determined it empirically:
1. Built a labelled calibration cubemap (`dds_writer.py` + a throwaway
   script): each of the 6 slots filled with a distinct solid colour, plus a
   large white quadrant marking the texture's "top rows" and a black
   quadrant marking its "right columns" (big blocks, not thin text — thin
   text got mip/minification-blurred into invisibility in early attempts).
2. Loaded it via `GZ_RENDERING_RESOURCE_PATH` into a throwaway world with
   `<scene><sky></sky></scene>`, `gz-sim-sensors-system`, and 6 static
   camera sensors at the same point looking world +X (east), +Y (north),
   -X (west), -Y (south), straight up, and straight down (one pitch each,
   yaw 0/90/180/-90 for the horizontal four).
3. Ran `gz sim -s -r --headless-rendering` and pulled frames over
   `gz-transport` (see `grab.py`/`grab_indiv.py` pattern), then read off
   which coloured slot and which white/black quadrant edge appeared in
   which camera — giving 3 independent data points (2 quadrant edges + base
   colour) per face, cross-checked at two different FOVs.

Result (`nx,ny` are the texture's own column/row coords, each in `[-1,1]`,
`nx` increasing left→right, `ny` increasing top→bottom):

| DDS slot (write order) | shows world direction | direction formula |
|---|---|---|
| slot0 (+X) | **east**  (+X) | `( 1, -ny,  nx)` |
| slot1 (-X) | **west**  (-X) | `(-1, -ny, -nx)` |
| slot2 (+Y) | **north** (+Y) | `(nx,  1, -ny)` |
| slot3 (-Y) | **south** (-Y) | `(nx, -1,  ny)` |
| slot4 (+Z) | **down**  (-Z) | `(nx, -ny, -1)` |
| slot5 (-Z) | **up**    (+Z) | `(-nx, -ny, 1)` |

Notably: the horizontal four faces (east/west/north/south) carry an
in-plane rotation relative to a naive "screen x = texture x" camera
projection, and the two polar (up/down) slots are **swapped** relative to
naive `+Z slot = world up` — slot4 (nominally "+Z" in D3D cubemap-face
naming) is actually the *down* direction, and slot5 ("-Z") is *up*.
`make_sky.py`'s `face_directions()` implements exactly this table; if you
ever see a vertically- or horizontally-flipped/rotated sky, this table (not
a general cubemap-math reference) is the thing to re-derive/re-check — e.g.
after a gz-rendering/Ogre major-version bump.

The final confirmation of the whole pipeline end-to-end was rendering the
*real* baked sky with the computed sun `<light>` and checking that a camera
pointed at the HDRI's own computed sun azimuth/elevation actually shows the
bright sun/cloud-glow region, and that a vertical pole's cast shadow is
consistent with the sun's screen position — both checked out (see Testing
below).

## Tone mapping

Source is an unclipped 2k Radiance `.hdr` (own RLE decoder in
`radiance_hdr.py`, no OpenEXR/OpenCV dependency needed). Tonemap is
exposure (1.1×) → ACES filmic (Narkowicz fit) → sRGB OETF, which keeps
cloud detail without blowing the sun out to a flat white disc while still
giving a punchy saturated blue sky.

## Sun direction/colour

`make_sky.py` finds the brightest pixel in the HDR (luminance-weighted,
averaged over a small neighbourhood to denoise), converts its equirect
`(row,col)` to a unit direction using the *same* spherical convention used
to build the cubemap (row 0 = zenith, azimuth = `atan2(y,x)`), then:
- `<direction>` in the light snippet = `-sun_direction` (SDF directional
  lights specify the direction light *travels*, i.e. from sun to ground).
- `<diffuse>` = the averaged HDR colour at the sun, normalized so its max
  channel is 1.0 and clamped to `[0.5, 1.0]` per channel (plain linear
  HDR sun colour is wildly overexposed for a 0–1 SDF diffuse value).

For the current HDRI this comes out to elevation ≈ 48°, azimuth ≈ 236°
(0 = N, 90 = E) in the HDRI's own frame — i.e. a fairly high, backlit-ish
midday sun with scattered cumulus, consistent with what you see in the
cubemap.

## Resolution / size budget

1024px per face, uncompressed RGBA8 (no DXT1 compressor was written —
`skybox.dds` is ~25 MB). Stayed under the ~30 MB budget for
`sim/assets/sky/` by **not** committing the media overlay tree (it's
~99% a verbatim copy of files already installed on the machine at
`/usr/share/gz/gz-rendering8/ogre2`); `build_media_overlay.py` assembles it
locally instead. If you need to shrink `skybox.dds` further, drop
`--faces` to e.g. 768 or 512 in `make_sky.py` (quality loss is mild — it's
a diffuse sky, not a reflective surface) — a real DXT1/BC1 encoder would
roughly quarter the size but wasn't implemented here.

## Testing (headless, what was actually verified)

- Calibration cubemap → confirmed the face-direction table above via 6
  static camera sensors (east/north/west/south/up/down), 2 FOVs, 3 markers
  per face (see table derivation above).
- Real baked sky + computed sun light, same 6-camera rig: confirmed no
  visible seams between faces at the horizon (`cam0`/`cam4` screenshots),
  confirmed a camera pointed at the computed sun azimuth/elevation shows
  the bright sun region, and confirmed a reference pole's cast shadow
  direction/length is consistent with the sun light's screen position.
- All testing used `gz sim -s -r --headless-rendering` with
  `gz-sim-sensors-system` (render_engine `ogre2`) and `GZ_PARTITION=sky_agent`
  to avoid colliding with other agents' sims; **the GUI client was never
  opened**, so GUI rendering of the custom sky is unverified (should work —
  same engine/resource-path mechanism — but not confirmed).

## Files

- `make_sky.py` — the reproducible pipeline (download → decode → tonemap →
  bake cubemap → write DDS + sun light snippet). Run this first.
- `build_media_overlay.py` — assembles the local (gitignored-by-convention,
  not committed) `GZ_RENDERING_RESOURCE_PATH` directory. Run this second,
  and again after any gz-rendering package upgrade.
- `radiance_hdr.py` — standalone Radiance `.hdr` (RGBE, new-style RLE)
  reader.
- `dds_writer.py` — standalone uncompressed-RGBA8 DDS cubemap writer
  matching Gazebo's stock skybox.dds header layout exactly.
