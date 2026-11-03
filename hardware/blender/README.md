# CogiDrone X500 V2 – Blender model

`cogidrone_x500.blend` is the whole drone as we plan to build it: frame, flight electronics, Jetson,
camera, both batteries and the wiring. Open it in Blender 4.2 or newer. Collections:

| Collection | What's in it |
|---|---|
| Frame (Holybro STEP) | Holybro's X500 V2 frame (244 parts), incl. their depth-camera mount and GPS mast, plus 10" prop discs |
| Flight electronics | Pixhawk 6C (Holybro STEP), M10 GPS (Holybro STEP), PM02 V3, SiK 433, XM+ |
| Companion computer + camera | Jetson Orin Nano Super, D435 (Intel mesh), UART-to-USB |
| Batteries + Jetson power | Flight pack, Jetson pack, buzzer, fuse holder, Deans, DC plug |
| Wiring | Power (orange), data (blue), USB 3 (dark blue), TELEM2 crossover (purple), Jetson lead (red/black) |
| Helpers | Depth-camera FOV (87° × 58°), labels, cameras for the renders in `renders/` |

The root object is `CogiDrone X500 V2`: metres, forward = +X, left = +Y, skids on the ground (Z = 0).

## Sources

| Part | Source | Accuracy |
|---|---|---|
| Frame, camera mount, GPS mast | [Holybro X500 V2 downloads](https://docs.holybro.com/drone-development-kit/px4-development-kit-x500v2/download), `x500v2-frame.step` | Official CAD |
| Pixhawk 6C | [Holybro Pixhawk 6C downloads](https://docs.holybro.com/autopilot/pixhawk-6c/download) | Official CAD |
| M10 GPS | [Holybro GPS downloads](https://docs.holybro.com/gps-and-rtk-system/m8n-m9n-m10-gps/standard-m10-m9n-m8n-gps/download) | Official CAD |
| D435i | `d435.dae` from [realsense-ros](https://github.com/realsenseai/realsense-ros) | Official mesh |
| Jetson Orin Nano dev kit | Built from published size (100 × 79 mm, ~31 mm tall). NVIDIA's STEP needs a developer login | Approximate |
| Batteries | Box with shop dimensions: 139 × 47 × 40 mm, 120 × 35 × 28 mm (elefun.no) | Approximate |
| PM02, SiK, XM+, converter, buzzer, fuse | Boxes with datasheet sizes | Approximate |

## Checks done in the model

- Jetson (upside down under the rear platform board) clears the flight pack by 10 mm, 148 mm above the ground.
- GPS sits 135 mm above the Jetson.
- No point on any prop disc is inside the D435i's depth or colour view.
- `camera_link` is 0.135 m forward and 0.044 m below the frame centre. The sim model (`sim/models/cogidrone_x500/model.sdf`) still uses 0.12 m forward and about 0 m up.
- Estimated CG is about 2 mm forward of centre, all-up weight about 2.2 kg (Jetson 176 g and frame kit 1050 g are estimates).

## Rebuilding

`scripts/` has the pipeline: `step2glb_ocp.py` converts STEP to GLB with OpenCASCADE (`pip install cadquery-ocp trimesh scipy`; for the Collada camera also `pycollada`), `build.py` + `parts.py` assemble the scene, and `render.py` renders every `Cam …` camera:

```bash
blender -b --factory-startup --python scripts/build.py -- <work dir with tools/ and cad_glb/> out.blend
blender -b out.blend --python scripts/render.py -- renders/
```

The .blend embeds Holybro's and Intel's CAD. Check that sharing it publicly is OK before pushing it to a public repo; it is also 16 MB, so consider Git LFS.

## Jetson power lead (`wiring_lead/`)

`wiring_lead/build_wiring_lead.py` builds a separate table-top scene of the Jetson power lead and
the charging setup, with the shop photos of every part we bought (`wiring_lead/shop_img/`). It
shows the Kjell 5×20 mm glass-fuse holder with a 4 A slow fuse, the DeLOCK screw-terminal DC plug
and the 470 µF capacitor in that terminal. Rebuild and render both views (overview and plug
close-up):

```bash
blender -b --factory-startup --python wiring_lead/build_wiring_lead.py
```
