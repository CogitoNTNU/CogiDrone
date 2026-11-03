# Flight viewer

A 3D viewer for search flights, used after a run (sim or real, anywhere): the drone path with
time marks, camera frames (pyramids showing where the camera looked, with the photo), every
person sighting as a sphere and the clustered persons, on a terrain map, plus a **position
solver** that re-solves the person positions from the raw observations with weights you tune
live, and the drone's own photos draped on the ground.

```bash
python3 tools/flight_viewer/view.py                 # ~/flight_runs/latest
python3 tools/flight_viewer/view.py RUN_DIR --port 8765 --no-browser --rebuild-terrain --host 0.0.0.0
python3 tools/flight_viewer/view.py RUN_DIR --background global          # map source, see Maps
python3 tools/flight_viewer/view.py RUN_DIR --anchor 63.4261,10.2806     # log without lat/lon (no GPS)
~/cogidrone_deps/venv/bin/python tools/flight_viewer/heads.py RUN_DIR    # head detector after the flight
```

`view.py` builds the terrain background if the run does not have it yet (`terrain.py`),
serves this folder and the run on `http://127.0.0.1:8765/` and opens Chromium/Firefox.
Everything is offline except the map tiles (Kartverket / AWS / Esri / OSM, cached afterwards);
without network the background is a flat metric grid.

**Controls:** left drag pans, right drag (or Ctrl+drag) orbits and tilts, the wheel zooms to
the cursor. Hover a marker for details, click to pin, Esc clears it. Space plays. The time
slider has two handles (from, to) and everything follows it. *Fade after N s* fades out
what is older than N s before the end handle (it is gone after another N/2 s). *Marker size*
scales spheres, rings, path width and pins; *sphere opacity* boosts the faint low-confidence
spheres; *frustum size* is the pyramid depth in metres. Frames saved only because of a
sighting are hidden unless *+ frames of sightings* is on (hovering a sphere always shows its
camera). URL options: `?view=tilt`, `?t1=SECONDS`, `?select=s:12` (sighting 12; also
`f:` frame, `p:` person, `t:` truth, `q:` solved person), `?run=OTHER_URL/`, `?photos` (photo layer on),
`?nosolver` (solver panel folded). A scale bar, north arrow and the lat/lon of the view centre
sit bottom left (or "local frame only" without GPS / anchor).

| Marker | Meaning |
|---|---|
| path ribbon + line | drone position, coloured by time (viridis: dark = start, yellow = end); dots + `m:ss` labels every 5 s to 20 min (≤ 40 marks) |
| white pyramid | a sampled camera frame: apex = camera, base = image rectangle at *frustum size* m, small tick = image top |
| sphere | one sighting (one YOLO box) at its estimated position; colour = mean RGB of the inner 50 % of the box, opacity = `min(0.5, 0.5·conf)` × *sphere opacity* |
| magenta ring + `P3 · 12/40` | person (cluster of sightings), sightings within the time range / total; grey = not pinged yet |
| green cone | sim truth person (sim runs only) |
| cyan ellipse + `S3 · 12 · 1.4 m` | solved person (solver panel): 2σ horizontal ellipse, sightings, error to the nearest truth (sim) |
| photos on the ground | *Drone photos on ground* layer (experimental, below) |
| yellow | selection: camera frustum of the sighting (reaching to the sphere), camera → sphere line |

## Files

| File | |
|---|---|
| `runlog.py` | `RunLog`: writes the run-log below (numpy + Pillow, thread safe). Use it from any flight code. |
| `terrain.py` | builds `terrain/` for a run (also `python3 tools/flight_viewer/terrain.py RUN_DIR [--force]`) |
| `view.py` | launcher: terrain + local http server + browser; `GET /api/terrain?source=` builds another map |
| `heads.py` | post-flight head detection on the saved crops; also upgrades a v1 log to the v2 columns |
| `index.html`, `app.js`, `style.css` | the viewer (static, ES modules) |
| `solver.js` | the position solver (plain JS, no three.js) |
| `vendor/` | three.js r160.1 `three.module.min.js` + `OrbitControls.js`, MIT (`vendor/LICENSE.three`) |

## Position solver (viewer, left panel)

Getting the person position right is a post step. Every sighting carries raw observations
(format v2 below); the solver combines them per person with weights you set and re-solves on
every slider move (~40 ms for 800 sightings). Sources, all along the **feet ray** (camera →
bottom centre of the box):

| weight | estimate | 1σ used (then × 1/weight) |
|---|---|---|
| Rays (triang.) | a line constraint: the person is on the ray; many rays from different places intersect (multi-view triangulation) | across the ray: range × √((3 px/fy)² + 0.5°²) |
| Terrain hit | where the feet ray meets the terrain: the flight's ground model (logged `gnd_*`), or this map (*re-intersect rays with this map*, default on for real flights: the flight itself only knows flat ground) | along: *terrain σ* / sin(depression) + 2 % |
| Head size | Muhammad's `depth_head.py`: *head height* × fy / head px (head box from the head detector) | along: 20 % |
| Body height | same file, whole body: *body height* × fy / box px | along: 15 % |
| Depth camera | depth image median in the middle of the box (D435i, valid ≤ 10 m) | along: 3 % + 5 cm |

Ranges to the head / body centre are moved to the feet for a standing person (head 1.6 m,
body 0.85 m over the feet). Each estimate is an information matrix (along / across the ray),
the weight multiplies it, a person is the weighted least-squares point of all its constraints
(+ a weak 20 m prior), 4 IRLS rounds with a Huber weight (outliers > 2.5σ are down-weighted).
Clustering: every sighting is first fused on its own, clustered in time order (nearest cluster
within *cluster radius*, running mean, like the flight), each cluster is solved, every sighting
is re-assigned to the nearest solved point and the clusters are solved again. *Min sightings*
= pinged person (else candidate), *min conf* drops weak YOLO boxes, *head px min* ignores tiny
heads. *Only sightings in the time range* solves with the time slider's range. The ellipse is
2σ of the covariance, scaled by the reduced χ² and by n/4 (sightings of one pass share their
errors, so ~4 independent looks).

Click a solved person for its photo, the rays that went into it (yellow), and per source the
mean residual along / across the ray: a source with a large *along* mean is biased (e.g. the
head height constant). With sim truth the panel shows live **found N/9 within 5 m, error
mean / median / max, false persons** — tune with it. *Export solved persons* downloads
`persons_solved.csv` (person, pinged, n_sight, conf_max, e, n, u, lat, lon, sigma_major_m,
sigma_minor_m, ellipse_deg_from_east, t_first, t_last, sources, truth, err_m) and
`persons_solved.json` (the same + all parameters + the score). *Defaults* = roughly the flight's
own result (terrain + rays, 4 m clusters, 3 sightings, conf 0.4).

## Maps (terrain.py, `--background`, viewer *Background* menu)

`auto` tries, first that works: **sim** (Bymarka worlds: the baked sim terrain) →
**kartverket** (inside Norway: Kartverket DTM + Norge i bilder / topo) → **global** (anywhere:
[AWS Terrain Tiles](https://registry.opendata.aws/terrain-tiles/) terrarium heights, ~30 m
SRTM-class, open, no key + Esri World Imagery, ~0.3 m in cities, attribution shown) →
**flat** (no network / no position: a plane at the takeoff ground height with a 10 m / 50 m
metric grid, 50 m lines labelled). `osm` = global heights + OpenStreetMap tiles (zoom ≤ 18).
The viewer's menu asks `view.py` to build another source into `RUN/terrain_<source>/` (not with
a plain static server). Tiles are cached in `~/.cache/cogidrone_flight_viewer/tiles/`.

**No GPS / local position only.** A log without lat/lon (`meta.origin` null, empty lat/lon
columns; `meta.position.global = false`) is shown in its local metric frame on the flat grid.
To put it on a map, give local (0, 0) a place: `view.py --anchor LAT,LON[,ALT]` or
`"anchor": {"lat": .., "lon": ..}` in meta.json (also overrides a wrong origin).

**Drone photos on ground (experimental)** — the "map from the drone's own photos": up to
*max photos* sampled frames (taken ≥ 5 m over the ground), a 16 × 9 grid of image points →
rays → terrain (or the flat plane) → a textured mesh per photo. Rays less than 8° below the
horizon or longer than *photo range* are cut off. Follows the time slider; *photo opacity*.
Anything not on the ground (trees, people, buildings) smears outward, and pose errors show as
seams — on the flat grid without any map it still gives a usable picture of the area.

## Position source / odometry

The log takes its pose from whatever the flight code passes to `RunLog.track/frame/sighting`
(local NED + attitude). `search_test.py` uses PX4's `VehicleLocalPosition` (EKF2: GPS + IMU +
baro in the sim and outdoors): `src = px4_ekf_gps` when `xy_global` (then `meta.origin` =
the EKF reference `ref_lat/ref_lon/ref_alt`), else `px4_ekf_local` (no GPS: local frame only,
no lat/lon). `eph/epv` = PX4's 1σ horizontal / vertical accuracy. The planned VIO
(cuVSLAM, see the localization plan) can feed the same format: call `set_position_source("vio",
global_ok=False)` and pass its pose as local NED (`track(..., src="vio", eph=..)`); the viewer
then works in the local frame (+ `--anchor`). Ground truth poses from a sim would be `sim_truth`.

## Run-log format (version 2)

One directory, default `~/flight_runs/latest/`, **replaced by every run** (writers may keep a
timestamped copy next to it, e.g. `search_test.py --keep` → `~/flight_runs/20261103_101500_<world>/`).

Frames: **local ENU** in metres (`e` east, `n` north, `u` up) around `meta.origin` (PX4: the
EKF origin, `ref_lat/ref_lon/ref_alt`; lat/lon ↔ e/n with PX4's azimuthal equidistant
projection). `t` = seconds since the first sample of the run (`meta.t0` = that moment on the
logging clock, `meta.t0_wall` = wall clock). Camera orientation is a quaternion `(w, x, y, z)`
rotating the camera **optical frame** (x right, y down, z forward = OpenCV) into local ENU.
CSV: comma separated, header row, `\n`, no quoting; empty cell = unknown.

```
meta.json        what/where/how (below)
track.csv        drone path
frames.csv       saved camera frames  + frames/f00012.jpg
sightings.csv    every detection      + crops/s00031.jpg
persons.csv      clusters (written at the end)
truth.csv        sim only: where the people really are
terrain/         added by terrain.py (not written by the flight code)
```

**meta.json**

| key | |
|---|---|
| `format`, `version` | `"cogidrone-flight-run"`, `2` (v1 logs still load; `heads.py` adds the v2 columns) |
| `source`, `world` | `"sim"` / `"real"`; sim world name (`bymarka_dense_s`, `sar`, …) or `""` |
| `start_time`, `end_time`, `duration_s`, `t0`, `t0_wall` | |
| `origin` | `{lat, lon, alt}` of local (0, 0, 0); `null` = no global position (local frame only) |
| `position` | `{source, global, topic, note}`: `px4_ekf_gps`, `px4_ekf_local`, `vio`, `sim_truth` |
| `estimators` | `{head_height_m, body_height_m, head_model, head_conf, head_every, head_min_px, depth_max}` |
| `anchor` | optional, set by hand: `{lat, lon[, alt]}` for local (0, 0) (see Maps) |
| `takeoff` | `{e, n, u}` of the body at takeoff |
| `ground_u` | `u` of the ground under the takeoff spot (terrain heights are anchored to it) |
| `world_origin` | sim: `{lat, lon, alt}` of the Gazebo world origin |
| `camera` | full-resolution intrinsics `{width, height, fx, fy, cx, cy}` (+ `pitch_down_deg`, `offset_frd`); sighting boxes are in these pixels |
| `sampling` | `{track_hz, frame_every_s, frame_min_move_m, frame_min_turn_deg, max_frames, frame_width_px, jpeg_quality}` |
| `counts` | `{track, frames, sightings}` |
| anything else | writer specific (`tool`, `args`, `stats`, `search_test_out`) |

**track.csv** `t, lat, lon, alt, e, n, u, roll, pitch, yaw, src, eph, epv` at `track_hz` (default 5).
Angles in degrees, body FRD → NED (yaw clockwise from north); `alt` = `origin.alt + u`;
`lat, lon` empty without a global position; `src` = position source, `eph/epv` = its 1σ (m).

**frames.csv** `id, t, file, e, n, u, roll, pitch, yaw, cam_e, cam_n, cam_u, cam_qw, cam_qx, cam_qy, cam_qz, kind`.
`kind = sample`: tried every `frame_every_s` (3 s) and kept only if the drone moved
≥ `frame_min_move_m` (2 m) **or** the camera turned ≥ `frame_min_turn_deg` (15°) since the last
kept sample (hovering produces no new frames), at most `max_frames` (2000).
`kind = sighting`: always saved for a camera frame with a sighting (one image per frame,
shared by its sightings; cap 2 × `max_frames`). JPEG, `frame_width_px` (640) wide, quality 80.

**sightings.csv** `t, person, method, conf, u0, v0, u1, v1, e, n, u, lat, lon, range_m, r, g, b, frame, crop, cam_e, cam_n, cam_u, cam_qw, cam_qx, cam_qy, cam_qz`.
One row per detection box: `person` = cluster id (persons.csv), `method` = how it was located
(`depth`, `ground`), box in full-resolution pixels, `e n u` = estimated position, `r g b` =
mean colour of the inner 50 % of the box, `frame` = frames.csv id, `crop` = small crop of the
box (≤ 160 px, upscaled to ≥ 96 px), camera pose at that moment. Version 2 appends the raw
observations (everything needed to re-solve the position later; empty = not available):

| columns | |
|---|---|
| `cam_lat, cam_lon, cam_alt` | camera position, global |
| `ray_f_e, ray_f_n, ray_f_u` | unit ray (local ENU) through the box bottom centre (feet) |
| `ray_c_*`, `ray_h_*` | through the box centre / the head box centre |
| `head_u0, head_v0, head_u1, head_v1, head_conf, head_px` | head box (full-res px), its detector conf, height px |
| `d_head` | `head_height_m · fy / head_px` (Muhammad's formula, 0.23 m) |
| `d_body` | `body_height_m · fy / (v1 − v0)` (1.70 m) |
| `d_depth` | depth camera range to the body (median of the middle half of the box), if valid |
| `gnd_e, gnd_n, gnd_u, d_gnd` | where the feet ray meets the flight's ground model, range |

Rows may be out of time order (sightings that wait for the head detector are written later).

**persons.csv** `person, n_sight, conf_max, e, n, u, lat, lon, t_first, t_last, pinged, truth, err_m`
(`truth`, `err_m`: nearest sim truth person and distance, sim only).

**truth.csv** `id, state, zone, e, n, u` (u = feet; walking people: start point).

### terrain/ (terrain.py)

`terrain.json` `{kind, source, tried, errors, extent: [e0, n0, e1, n1], nx, ny, res_m, u_min, u_max, tex_size, origin, anchored, heights, texture, attribution}`
(`kind` = what was built: `bymarka`, `kartverket`, `global`, `osm`, `flat`; other sources from the viewer menu go to `terrain_<source>/`),
`height.bin` (float32 LE, `ny × nx`, row 0 = north edge, corners included, local `u`),
`color.jpg` (north up, covers `extent`, ≤ 4096 px). Area = everything in the run + 150 m.
Heights are DTM − DTM(takeoff) + `ground_u`.

- **sim, Bymarka worlds**: the cached Kartverket 1 m DTM the world was built from + the baked
  terrain masters (`sim/models/bymarka_terrain/materials/bake`), lakes painted from the cached
  topo map. Local ENU → world is a shift through `world_origin` (the world is built on the
  UTM grid; Gazebo treats it as true east/north).
- **kartverket** (real flights in Norway): Kartverket WCS DTM + Norge i bilder ortophoto WMS (only from authorised networks;
  `COGI_ORTHO_WMS='url|layer'` for another WMS in EPSG:25832), else the Kartverket topo map.
  Local ENU → UTM 32N is fitted from true lat/lon.
- **global / osm** (anywhere): web-mercator tiles; local ENU → mercator pixel is an affine fit
  over the area (conformal, fine at km scale). Heights ≤ zoom 15 (≤ 36 tiles), imagery up to
  zoom 19 (≤ 144 tiles; OSM ≤ 18).
- **flat** (other sims / no network / no position): a plane at `ground_u` (or 0.25 m under the
  first track point) with a 10 m / 50 m grid, 50 m crossings labelled `+50 E / -100 N`.

Cached in `~/.cache/cogidrone_flight_viewer/<hash of area + source>/`; `--rebuild-terrain` redoes it.

## Writing the log

```python
sys.path.insert(0, "tools/flight_viewer"); from runlog import RunLog
rl = RunLog("~/flight_runs/latest", meta={"source": "real"}, frame_every=3.0)
rl.set_origin(lat, lon, alt); rl.write_meta(takeoff={...}, ground_u=...)
rl.set_camera(w, h, fx, fy, cx, cy)
rl.track(t, p_ned, R_frd_to_ned)                         # every pose, rate-limited
fid = rl.frame(t, rgb, p_ned, R, cam_ned, R_cam_frd_to_ned)        # sampled
fid = rl.frame(..., force=True); rl.sighting(t, pid, "ground", conf, box, est_ned, rng, rgb=rgb, frame=fid, cam_ned=..., R_cam_ned=...)
rl.close(persons=[{...}])
```

`rl.set_origin(None, None, 0)` for no GPS; `rl.set_position_source(...)`,
`rl.set_estimators(head_height_m=.., body_height_m=..)`; `rl.track(..., src=, eph=, epv=)`;
`rl.sighting(..., head=(u0, v0, u1, v1, conf), d_depth=.., ground=(p_ned, range))`.

`scripts/search_test/search_test.py` writes it (`--log-dir`, `--keep`, `--track-hz`,
`--frame-every`, `--frame-min-move`, `--frame-min-turn`, `--max-frames`, `--frame-width`,
`--no-runlog`) and runs the head detector in flight: `--head-model`
(default `~/cogidrone_deps/models/head_medium.pt`, Muhammad's `medium.pt`; off if missing or
`''`), `--head-conf 0.25`, `--head-every 1` (every Nth sighting), `--head-min-px 30` (person
boxes at least this tall), `--head-imgsz 320`, `--head-height-m 0.23`, `--body-height-m 1.70`.
It runs on the person crop in its own thread with an 8-deep queue (full = no head for that
sighting), so YOLO never waits; ~22 ms per crop on the Orin.

**Head distance, measured** (bymarka_dense_s run, 15 m altitude, 20° pitch, 720p, persons at
25–45 m, `heads.py` on the saved crops): a head is found in 21 % of all sightings (34 % of the
ones on a true person); heads are 6–13 px tall. The head box is bigger than the head: px ×
range / fy = 0.36 m median, so with 0.23 m the distance is 37 % short (11 m); with
*head height* 0.36 m the error is 2.3 m median (≈8 %), p90 7 m — worse than the terrain hit
(1.1 m) and like the body height (2.4 m). ±1 px is ±10 % at 10 px: the head needs ≳ 20 px
(≈ 2× closer, ~15 m range, or a 1080p/zoom camera) to beat the terrain hit. Calibrate the
head height on real people before trusting it (the sim actors' heads may differ).
