# Sky asset attribution

`skybox.dds` is derived from the HDRI **"Kloofendal 48d Partly Cloudy (Pure Sky)"**
by Greg Zaal and Jarod Guest, published on [Poly Haven](https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky).

- Source: https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky
- File used: `kloofendal_48d_partly_cloudy_puresky_2k.hdr` (2k equirectangular, Radiance RGBE)
- License: **CC0 1.0** (public domain) — https://polyhaven.com/license
- No attribution is legally required, but it's credited here anyway.

## Processing

`skybox.dds` is generated from the source HDRI by `sim/tools/sky/make_sky.py`:
tone-mapped (ACES filmic + sRGB), reprojected from equirectangular to a
6-face cubemap, and written out as an uncompressed RGBA8 DDS cubemap in the
layout Gazebo Harmonic's ogre2 skybox expects. The derived file is itself
CC0 (same license as the source, no new copyrightable material introduced
by a mechanical transform).

The accompanying `sun_light.sdf.xml` direction/colour values were computed
from the same HDRI (brightest-pixel detection) by the same script, see
`sim/tools/sky/README.md`.
