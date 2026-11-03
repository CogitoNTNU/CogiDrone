# Simulation (PX4 + Gazebo + ROS 2 + YOLO)

Fly the drone in simulation while the hardware is on its way. The sim is PX4 SITL (the real
autopilot firmware built as a Linux program), Gazebo Harmonic (world, drone, camera) and ROS 2
Jazzy (our nodes), started together by one `ros2 launch`.

There are two ways to run it:

| | Where | How |
|---|---|---|
| **Native** | The sim Jetson (AGX Orin), and any Ubuntu 24.04 machine set up the same way | Install once with [jetson.md](jetson.md), then `source scripts/jetson/env.sh` in every terminal (`~/.bashrc` does it) |
| **Docker** | Laptops: Windows (WSL2), Linux, macOS | One image for everyone, sections 1-2 below |

After that the commands are the same. The common workflows, step by step with terminals, are on
the start page ([README](../README.md)). This page is the reference.

**How do the pieces connect?** See [architecture.md](architecture.md): machines, programs,
topics and frames on one page.

## 1. Install (once)

**Sim Jetson:** see [jetson.md](jetson.md). Don't use Docker there.

**Docker**, what each OS can do:

| | Windows (WSL2) | Linux | macOS |
|---|---|---|---|
| Full sim (Gazebo + PX4) | yes | yes | no |
| Write / build / run ROS 2 nodes, YOLO | yes | yes | yes |
| Replay recorded sim data (rosbag) | yes | yes | yes |

**What's inside.** ROS 2 Jazzy, PX4 v1.17.0 (SITL), Gazebo Harmonic, Micro XRCE-DDS Agent
v2.4.3, px4_msgs `release/1.17` and YOLO (ultralytics, CPU). All versions are pinned in
[`docker/sim/Dockerfile`](../docker/sim/Dockerfile) (and, for the Jetsons, in
`scripts/jetson/install-user.sh`; keep them equal).

**Recommended PC:** 16 GB RAM and 30 GB free disk. 8 GB works in headless mode. The Bymarka
worlds need much more (section 3).

**Windows**

1. Install WSL2 with Ubuntu 24.04: `wsl --install -d Ubuntu-24.04` in PowerShell.
2. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/). In
   *Settings → Resources → WSL integration*, enable Ubuntu-24.04.
3. **Clone the repo inside WSL** (e.g. `~/CogiDrone`), not under `C:\` or OneDrive.
   Builds are many times faster there.

**Linux:** install Docker Engine and add yourself to the `docker` group.

**macOS:** install Docker Desktop. Apple Silicon runs the image natively (arm64).

## 2. Build and start

**Docker**, from the repo root (inside WSL on Windows):

```bash
./scripts/sim.sh build   # first time: 20-40 min (compiles PX4). Later: seconds.
./scripts/sim.sh up
./scripts/sim.sh shell   # opens a shell in the container; open as many as you need
```

`./scripts/sim.sh down` stops the container. Prefer VS Code? Run `./scripts/sim.sh build` once,
then *Dev Containers: Reopen in Container* and pick **CogiDrone sim** (Windows/Linux) or
**CogiDrone dev** (macOS).

**Both:** build our ROS 2 packages (first time, and after every change in `ros2_ws/src` or `src/`):

```bash
cb    # = colcon build --symlink-install in ros2_ws, then sources it
```

## 3. Worlds

Pick one with `world:=NAME`. Any `.sdf` in `sim/worlds/` works, and PX4's own worlds too
(e.g. `world:=baylands`).

| World | Area | People | In git | What it is for |
|---|---|---|---|---|
| `cogidrone` (default) | Small | 2 | yes | Light test world at NTNU's coordinates. Quick start, YOLO targets in front of the drone |
| `sar` | 600 x 600 m | 24 | yes | Search-and-rescue map loosely like the outskirts of Trondheim |
| `bymarka` | 2 x 2 km | 20-24 | generated | Real Bymarka terrain with the trees the laser data resolves (sparser than reality). Long flights and navigation |
| `bymarka_dense` | 600 x 600 m | 20-24 | generated | The same forest filled in to real stand density. Detection and search tests |
| `bymarka_dense_q` | 300 x 300 m (1/4) | 13 | generated | A crop of `bymarka_dense` around the launch spot |
| `bymarka_dense_s` | 150 x 150 m (1/16) | 9 | generated | The smallest crop. Flight tests |

"Generated" worlds are not in git: build them once with section 8.

**Measured on the AGX Orin (61 GB).** Numbers from different setups, so compare within a row:

| World | Setup | Ready after | RAM in use | Real-time factor | Camera |
|---|---|---|---|---|---|
| `cogidrone` | | ~10-20 s (GPS fix) | | | |
| `sar` | first start downloads ~200 MB of Fuel models | ~1 min | | | |
| `bymarka` | PX4 + C++ core, Gazebo window, 15 Hz camera | 1.7 min to the first camera frame | 29 GB (server 10 + window 9 GB, desktop ~14 GB) | ~0.8 | 11.5 Hz |
| `bymarka_dense` | same | 2.5 min | 52 GB (server 22 + window 21 GB) | 0.9-1.0 | 13.8 of 15 Hz |
| `bymarka_dense_q` | | not measured | | | |
| `bymarka_dense_s` | PX4 + `search_test` + YOLO, headless, 720p @ 30 Hz | loads in ~40 s, flying in under 2 min | 10 GB (gz server ~6 GB RSS) | 0.98 avg (min 0.89) | 29.0 Hz avg (min 26.1) |

On the Jetson the GPU memory is system RAM, and the Gazebo server (which renders the drone
camera) and the Gazebo window each load the whole world. `headless:=true` halves the RAM.

### `sar`

A 600 x 600 m map loosely like the outskirts of Trondheim: a fjord to the north, a hilly spruce
forest to the west, fields to the south and a small town with collapsed buildings to the east.
24 people are spread over it: standing, walking, sitting, waving and lying down (some under trees
or next to rubble).

- The first start downloads about 200 MB of Gazebo Fuel models into `~/.gz/fuel` (one time).
- Where every person is: [`sim/worlds/sar_people.yaml`](../sim/worlds/sar_people.yaml).
- New random people (and terrain): `python3 sim/tools/make_sar_world.py --seed 7`.
  On a slow PC, `--trees 500` makes the forest lighter. Don't edit the generated files by hand.
- Ground textures are photo scans from [ambientCG](https://ambientcg.com) (CC0). Buildings,
  trees and people are Gazebo Fuel models.

### Bymarka (real terrain)

Worlds built from open map data of the real forest around Kobberdammen and Gråkallen in Bymarka,
Trondheim: Kartverket's 1 m laser terrain, the real lakes, paths and streams from
OpenStreetMap, trees at their real positions and heights, a photo sky and 30 km of real landscape
around it. GPS in the sim matches the real place. The world origin (the drone spawn) is the launch
clearing in every Bymarka world.

- `bymarka_dense`: the canopy is filled in to real stand density (600-1200 trees/ha), with young
  trees, bilberry, heather, rocks and logs on the ground.
- `bymarka_dense_q` and `_s` use the same trees, props and people (same seeds) as
  `bymarka_dense`, plus a few extra people inside the crop. Only the crop square is detailed (`_q`:
  bog, trails, forest, the lake shore; `_s`: open bog and the launch clearing between two trails,
  the dense spruce edge to the west, the lake shore to the east, some rock; 4 of its 9 people walk
  or wave). Outside the crop the trees are 8-triangle impostors **without collision**, so the
  horizon looks the same, but stay inside the square.
- Most of the dense world's RAM is its detailed forest (~14 GB per process: Gazebo keeps several
  copies of every vertex).
- People: standing, walking, sitting, waving and lying (face up or down), in the forest, on paths,
  by the lakes and on open rock. Clothes range from hi-vis and red to dark, green, brown and white;
  some lying people are injured. Where they are: `sim/worlds/<world>_people.yaml`.
- The data licences (Kartverket CC BY 4.0, NIBIO NLOD, OpenStreetMap ODbL) need a credit line on
  slides and screenshots, see [`sim/ATTRIBUTION.md`](../sim/ATTRIBUTION.md).

## 4. Launch arguments

Two launch files start the same simulator; they differ in who flies (section 5).
`sim_cpp.launch.py` includes `sim.launch.py` with `autonomy:=false` and adds the C++ core.

| Argument | `sim.launch.py` | `sim_cpp.launch.py` | Default | What it does |
|---|---|---|---|---|
| `world` | yes | yes | `cogidrone` | World name: `sim/worlds/<name>.sdf`, else PX4's worlds folder (section 3) |
| `headless` | yes | yes | `false` | `true`: no Gazebo window (faster, about half the RAM) |
| `altitude` | yes | yes | `3.0` / `2.5` | Takeoff height in m (Python `offboard` / C++ core) |
| `autonomy` | yes | always `false` | `true` | Start the YOLO `detector` and the Python `offboard` controller. `false` = sim only, fly it yourself |
| `yolo` | no | yes | `false` | Run the Python YOLO `detector` and feed its boxes to Navigation |
| `auto_start` | no | yes | `true` | Send `START_MISSION` to Navigation after takeoff; `false` stays in `IDLE` |
| `view` | yes | yes | `false` | The `detector` opens one OpenCV window with the camera and YOLO boxes. Needs `autonomy:=true` (Python) or `yolo:=true` (C++) |
| `cam_pitch` | yes | yes | `0` | Camera tilt, degrees down |
| `cam_x` | yes | yes | `0.12` | Camera forward of the model centre, m |
| `cam_z` | yes | yes | `0.242` | Camera height over the model origin (ground), m |
| `cam_res` | yes | yes | `720p` | Color mode like the real D435i: `720p` (1280x720, 69.8°) or `480p` (640x480, 55.2°) |
| `cam_fps` | yes | yes | `30` | Color camera rate, Hz |
| `cam_aa` | yes | yes | `4` | Color anti-aliasing, MSAA samples: 0, 2, 4, 8 |
| `model` | yes | no | `cogidrone_x500` | Drone model in `sim/models` |

With non-default `cam_*` values the launch writes a copy of the model to
`~/.cache/cogidrone/models/cogidrone_x500/` and PX4 spawns that one; the SDF in git is not touched.

What the launch starts: the XRCE-DDS agent (UDP 8888), Gazebo (server, plus the window unless
headless), PX4 SITL (`PX4_GZ_STANDALONE=1`, so `Ctrl-C` stops it all), `ros_gz_bridge` for the
camera, and the nodes above. It sets `GZ_IP=127.0.0.1` (section 10) and refuses to start while
another Gazebo or PX4 SITL is running.

## 5. Fly

**The C++ core** (`src/`, the main path):

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py
ros2 launch cogidrone_bringup sim_cpp.launch.py headless:=true yolo:=true
```

After about 20 s the C++ `Pixhawk` node arms, takes off to 2.5 m and hands over to `Navigation`,
which hovers and spins slowly to search. `yolo:=true` feeds YOLO's detections to Navigation.
Land it, or steer the mission, from another terminal:

```bash
ros2 topic pub --once /manual_override std_msgs/msg/String "{data: LAND}"
# also: START_MISSION, RETURN_HOME, EMERGENCY_STOP
```

**The Python reference controller:**

```bash
ros2 launch cogidrone_bringup sim.launch.py
```

Gazebo opens and the drone spawns. After about 20 s (while PX4 gets a GPS fix) it arms, takes off
to 3 m and turns to face the person in front of it. If nobody is in view it spins slowly to search.

Never run both, or `teleop` / `search_test` next to an autonomous controller: two programs
sending setpoints to PX4 fight each other.

**Fly it yourself.** Start the sim without a controller, then the keyboard controller in a
second terminal (keep it focused) and a camera window in a third:

```bash
ros2 launch cogidrone_bringup sim.launch.py autonomy:=false
ros2 run cogidrone_control teleop
ros2 run cogidrone_perception detector --ros-args -p show:=true
```

| Key | Action |
|---|---|
| `t` | Take off: arm, offboard, climb to 2.5 m |
| `w` / `s`, `a` / `d` | Forward / back, left / right, 0.5 m per press |
| `r` / `f` | Up / down 0.5 m |
| `q` / `e` | Turn left / right 15° |
| space | Stop and hold here |
| `l` | Land |
| `x` | Quit |

Instead of the `detector` window: `ros2 run rqt_image_view rqt_image_view /camera/camera/color/image_raw`
shows the raw camera.

**Look around** (any terminal):

```bash
ros2 topic list                          # /fmu/... (PX4), /camera/... (sim camera), /cogidrone/...
ros2 run rqt_image_view rqt_image_view   # pick /cogidrone/detections/image to see YOLO boxes
ros2 topic echo /fmu/out/vehicle_status_v1 --once
```

**QGroundControl (optional).** It connects by itself (PX4 sends to the host on UDP 14550). On the
sim Jetson start it with `qgc`. To fly with an RC transmitter such as the RadioMaster Pocket, plug
it in over USB and pick *USB Joystick (HID)*, then in QGC go to *Vehicle Setup → Joystick*, enable
and calibrate it, and fly in *Position* mode (details:
[jetson.md](jetson.md#fly-with-the-radiomaster-pocket-usb-joystick)).

**Low-spec PC (8 GB RAM).** Add `headless:=true`. The Gazebo 3D view is the heaviest part; the
camera window still works.

**Stop:** `Ctrl-C` in the launch terminal stops PX4, Gazebo, the agent and the nodes.

### macOS: record and replay

There is no Gazebo on macOS. Someone with the sim records a flight:

```bash
ros2 bag record -o /workspace/rosbags/flight1 /camera/camera/color/image_raw /camera/camera/color/camera_info /fmu/out/vehicle_status_v1 /fmu/out/vehicle_local_position_v1
```

Bags are big (about 1 GB/min) and are git-ignored, so share them on the team drive. On the Mac,
put the bag in `rosbags/` and run:

```bash
ros2 bag play /workspace/rosbags/flight1 --loop &
ros2 run cogidrone_perception detector
ros2 topic echo /cogidrone/detections
```

## 6. Flight test mode (one window)

For a flight test you don't need the Gazebo editor window: run Gazebo headless and look at the
drone camera with the YOLO boxes in one window.

```bash
# The C++ core (or the Python controller) flying, YOLO boxes in one OpenCV window:
ros2 launch cogidrone_bringup sim_cpp.launch.py headless:=true yolo:=true view:=true
ros2 launch cogidrone_bringup sim.launch.py headless:=true view:=true
```

The search test ([`scripts/search_test`](../scripts/search_test/README.md)) runs the sim without a
controller and opens its own window:

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_s headless:=true autonomy:=false cam_pitch:=20
python3 scripts/search_test/search_test.py --world bymarka_dense_s --dem --cam-pitch 20 --alt 15 --speed 3 --conf 0.4 --route="-12,-42;20,-48;22,20;-5,30;-28,28;-5,10" --area=-80,-60,70,90
```

The search test window shows the boxes with their confidence and a status line: state, height,
speed, pings, camera rate and resolution, real-time factor and YOLO fps. The same picture is
published on `/search_test/image` (and the detector's on `/cogidrone/detections/image`), so a
remote machine can watch it in `rqt_image_view`. When it lands, open the result in the
[flight viewer](../tools/flight_viewer/README.md): `python3 tools/flight_viewer/view.py`. More
routes: [`scripts/search_test/README.md`](../scripts/search_test/README.md).

Measured end-to-end on the AGX Orin running the `bymarka_dense_s` demo above, headless, one window:

| World | RAM in use | Camera | Real-time factor | Demo result (1.3 min flight) |
|---|---|---|---|---|
| `bymarka_dense_s` (150 m) | 10 GB (gz server ~6 GB RSS) | 29.0 Hz avg (min 26.1) | 0.98 avg (min 0.89) | 4/9 people found, 0.5-4.4 m error, ~30 false pings at `--conf 0.4` |

Gazebo itself (no flying, idle) was at real-time factor ~1.0 and used ~9 GB total RAM; YOLO running
in its own thread added ~1 GB and kept up at ~18 fps on top of the 29 Hz camera. `bymarka_dense_s`
had its first camera frames within a few seconds of the bridge coming up, and PX4 was armed and
climbing about 8 s after that; the whole launch-to-flying sequence took under 2 minutes.

## 7. Sim camera vs the real D435i

The camera topics use the same names as `realsense-ros`, so on the real drone you swap the bridge
for the RealSense driver and the nodes stay the same. One difference: sim depth is `32FC1` in
metres, while the real D435i gives `16UC1` in millimetres.

The sim camera copies the D435i's 1280x720 color mode (the sharpest at 30 Hz; `realsense-ros`
defaults to 640x480, `cam_res:=480p`) and its default 848x480 depth. The often quoted 69° color FOV
is for 16:9 (1280x720); 640x480 is a 4:3 crop of the same sensor, so it sees only 55°.

| | Real D435i | Sim (`sim/models/cogidrone_x500/model.sdf`) |
|---|---|---|
| Color | 1280x720 @ 30 Hz, fx ≈ fy ≈ 918 px, cx 640, cy 360 → HFOV 69.8° x VFOV 42.8° (640x480: fx 612, 55.2° x 42.8°) | Same intrinsics (`camera_info` K: fx = fy = 918.2; 480p: 612.1), 30 Hz asked, **~27 Hz delivered** (below), 4x MSAA, sees to 3 km |
| Color shutter, distortion | Rolling shutter, Brown-Conrady ≈ 0 | Global shutter, no distortion |
| Depth | 848x480, 87° x 58° (datasheet: 86 x 57 ±3°) | 848x480, 87° x 56.5° (square pixels) |
| Depth range | Min-Z 0.31 m, ideal 0.3–3 m, useful to ~10 m | Clipped to 0.31–10 m (`-inf` = too close, `inf` = too far) |
| Depth noise | RMS ≤ 2 % at 2 m, grows with distance² | None (Gazebo's depth noise broke the image, see the SDF) |
| Depth format | `16UC1`, mm | `32FC1`, m |
| Geometry | Depth origin = left IR; color imager 15 mm to its right, baseline 50 mm; 90x25x25 mm, 72 g | Same: color sensor at y = -15 mm in `camera_link`; 90x25x25 mm box, 72 g |
| IMU | BMI055: accel 62.5/250 Hz, gyro 200/400 Hz | Not simulated (PX4's IMU only) |
| Mount | Front of the X500 V2, level for now | `camera_link` 0.12 m forward, 0.242 m over the model origin (2 mm over `base_link`), pitch 0 |

Measured on the AGX Orin (`bymarka_dense_s`, headless; frames reaching a ROS subscriber):

| Color mode | Rate | Notes |
|---|---|---|
| 720p, 30 Hz, 4x MSAA (default) | 27 Hz flying with YOLO, 30 Hz from Gazebo | real-time factor ~0.9 while flying with YOLO |
| 720p, no MSAA (`cam_aa:=0`) | +5 % | jagged branches; 8x MSAA costs the same as 4x |
| 480p, 30 Hz (`cam_res:=480p`) | 28 Hz | |

- Subscribe to the image with **reliable** QoS (depth 1). With best effort ~1/3 of the 720p frames
  never arrive (21 instead of 30 Hz): a 2.7 MB image is ~40 UDP fragments and one lost fragment
  drops the frame. `detector` and `search_test` do this. The C++ perception still uses
  `SensorDataQoS` (best effort).
- The far clip went from 100 m to 3 km (the far shore and the hills are in the picture now);
  that costs ~3 % frame rate.
- Without a GPU (laptops, Docker on CPU) use `cam_res:=480p cam_fps:=15`.
- Depth is only rendered while someone subscribes, which is why `gz_bridge.yaml` leaves it
  commented out (`search_test` bridges it itself).

**Changing the mount or mode.** No need to edit the SDF; the launch files take them (section 4):

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py cam_pitch:=30
ros2 launch cogidrone_bringup sim_cpp.launch.py cam_x:=0.15 cam_z:=0.20
ros2 launch cogidrone_bringup sim_cpp.launch.py cam_res:=480p cam_fps:=15 cam_aa:=0
```

At 720p (70° wide) the tip of the front-right propeller shows in the top-right corner at pitch 0;
at 20° down it is out of view (checked in the sim). The 480p crop sees no props at either pitch.

Sources: [Intel RealSense D400 Series datasheet, rev 009](https://www.realsenseai.com/wp-content/uploads/2020/06/Intel-RealSense-D400-Series-Datasheet-June-2020.pdf)
(FOV, Min-Z, depth accuracy, dimensions, IMU) and published `realsense-ros` calibrations of
D435i units at 640x480 (fx ≈ 605–617).

## 8. Generating the Bymarka worlds

The Bymarka models are generated, not in git (several GB). A fresh clone must build them once
before `world:=bymarka*` loads. The generators need numpy, scipy and Pillow, in their own venv:

```bash
python3 -m venv ~/cogidrone_deps/simtools-venv
~/cogidrone_deps/simtools-venv/bin/pip install numpy scipy pillow
```

Then, from the repo root, in this order:

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=sar headless:=true autonomy:=false
P=~/cogidrone_deps/simtools-venv/bin/python
$P sim/tools/people/make_people.py
$P sim/tools/bymarka/make_world.py --forest --props
$P sim/tools/bymarka/make_dense.py
```

| Step | What it makes | Time / size |
|---|---|---|
| `sar` launch | Downloads the three Gazebo Fuel people (Walking person, Standing person, Casual female) to `~/.gz/fuel`. `Ctrl-C` once loaded. Skip if they are there; or `gz fuel download -u "https://fuel.gazebosim.org/1.0/OpenRobotics/models/Walking person"` (and the other two) | ~200 MB, once |
| `make_people.py` | 36 recoloured people `sim/models/person_*` + `people.json`. No options: any argument, even `--help`, runs it | |
| `make_world.py --forest --props` | Shared terrain (8 x 8 tiles of 250 m), lakes, 30 km backdrop, trees and ground clutter, `worlds/bymarka.sdf` | First run downloads ~200 MB of map data |
| `make_dense.py` | `bymarka_dense` and the crops `bymarka_dense_q`, `bymarka_dense_s` (+ their `*_people.yaml`) | |

`make_world.py` and `make_dense.py` take about 10 minutes together. On the sim Jetson the
generated models take about 5.6 GB in `sim/models/` (the largest: `bymarka_dense_forest` 1.8 GB,
`bymarka_terrain` 0.9 GB) and the download cache about 0.9 GB in
`~/cogidrone_deps/simassets-cache/` (`$COGI_ASSET_CACHE` moves it). Generation RAM was not measured.

Useful options (`--help` on both scripts lists all):

| Command | What |
|---|---|
| `make_dense.py --crops-only` | Only the crop worlds, keep `bymarka_dense` (~40 s, needs `make_world.py` once) |
| `make_dense.py --crops q` | Which crops to build (default `q,s`, `''` = none). Crop boxes and extra people: `CROPS` in `make_dense.py` |
| `make_dense.py --skip-props` | Keep the props built last time |
| `make_world.py --retex --tex-far 1024` | Only re-encode the ground textures to another size (~1 min, no rebake) |
| `make_world.py --skip-terrain --forest --props` | Keep the baked terrain, rebuild trees and clutter |
| `make_world.py --skip-terrain --forest --props --near-radius 200` | A lighter big world: detailed trees only within 200 m |
| `make_world.py --people N`, `make_dense.py --people N` | Number of people |

Ground textures are BC1 `.dds` (1/8 of the RAM a JPG takes in Gazebo): 4096 px per 250 m tile
within `--near-radius` (350 m) of the launch spot, `--tex-far` (2048) outside. The baked 4K
masters stay in `sim/models/bymarka_terrain/materials/bake/`, so other sizes take a minute
instead of a rebake.

The tree library (`sim/assets/trees`), the props (`sim/assets/props`) and the sky cubemap
(`sim/assets/sky`) are in git, so they need no step. Only to change them:
[`sim/tools/trees/README.md`](../sim/tools/trees/README.md),
[`sim/tools/sky/README.md`](../sim/tools/sky/README.md), `sim/tools/props/`. The photo sky
needs one local step, otherwise Gazebo shows its default sky (the launch uses the overlay only
if it exists):

```bash
python3 sim/tools/sky/build_media_overlay.py
```

It builds `~/cogidrone_deps/simassets-cache/sky/media_root`; re-run it after a gz-rendering upgrade.

## 9. Where things are

The full picture (machines, both controllers, every topic, ENU vs NED) is in
[architecture.md](architecture.md). The Python path in short:

```
Gazebo (cogidrone_x500 + D435i-like camera)
   │ camera ──► ros_gz_bridge ──► /camera/camera/color/image_raw ──► detector (YOLO)
   │                                                                     │ /cogidrone/detections
PX4 SITL ◄── XRCE-DDS agent (UDP 8888) ◄── /fmu/in/* ◄──────────── offboard (control)
```

With `sim_cpp.launch.py`, `offboard` is replaced by the C++ `cogidrone` process.

| Where | What |
|---|---|
| `sim/models/cogidrone_x500/` | Drone: PX4's x500 (our frame) + a camera matching the D435i (section 7) |
| `sim/worlds/` | Worlds (`cogidrone.sdf`, `sar.sdf`, generated `bymarka*.sdf`) and where the people are (`*_people.yaml`) |
| `sim/tools/make_sar_world.py` | Generator of `sar.sdf` (with the `sar_*` models) |
| `sim/tools/bymarka/` | Generators for the Bymarka worlds (`make_world.py`, `make_dense.py`, `data.py` downloads and caches the map data); trees in `sim/tools/trees/`, ground clutter in `sim/tools/props/`, sky in `sim/tools/sky/`, people in `sim/tools/people/` |
| `ros2_ws/src/cogidrone_bringup/` | `sim.launch.py`, `sim_cpp.launch.py` and the Gazebo→ROS bridge config (`gz_bridge.yaml`) |
| `ros2_ws/src/cogidrone_perception/` | `detector`: image → YOLO → `vision_msgs/Detection2DArray` |
| `ros2_ws/src/cogidrone_control/` | `offboard`: arm, take off, yaw toward target (PX4 offboard mode). `teleop`: keyboard |
| `ros2_ws/src/cogidrone_core/` | Builds the C++ core in `src/` so `sim_cpp.launch.py` can start it |
| `scripts/search_test/` | Search test: route flight, YOLO, sightings, run log, scoring |
| `tools/flight_viewer/` | 3D viewer and position solver for the run logs |

## 10. Troubleshooting

- **`gz topic -l` shows nothing (or `gz sim` is not found).** The terminal lacks the environment:
  `source scripts/jetson/env.sh`. Gazebo's transport runs on loopback (`GZ_IP=127.0.0.1`, set by
  the launch); a shell without it looks on the network interface and sees no sim. `env.sh` also
  sets `GZ_CONFIG_PATH` so `gz` finds Gazebo Harmonic and not ROS's own `gz`.
- **"A simulator is already running".** A Gazebo or PX4 SITL from an earlier run is still alive.
  `Ctrl-C` it in its terminal, or stop it by name:
  ```bash
  pkill -f "gz sim"
  pkill -f px4_sitl_default/bin/px4
  ```
  A launch started from a background job ignores `Ctrl-C`; stop that one by name too.
- **UDP port 8888 busy / no `/fmu/...` topics.** An old `MicroXRCEAgent` from an earlier run
  still holds the port (the launch does not check for it), so the new agent cannot bind and PX4
  never reaches ROS:
  ```bash
  pgrep -af MicroXRCEAgent
  pkill -f MicroXRCEAgent
  ```
  Otherwise the agent or PX4 didn't start: check the launch output. PX4 topics with versioned
  messages end in `_v1` (e.g. `vehicle_status_v1`); a wrong name receives nothing and gives no error.
- **PX4 "Accel #0 fail: TIMEOUT", refuses to arm.** Leftover Gazebo processes (above), or Gazebo
  transport not on loopback (`GZ_IP`). See [jetson.md](jetson.md#fixed-along-the-way).
- **PX4 refuses to arm after a landing.** In SITL it refuses the next arming in the same sim.
  Restart the sim between `search_test` runs.
- **World not found / `bymarka*` models missing.** They are generated: section 8.
- **Out of RAM.** The Gazebo window loads the whole world a second time. `bymarka_dense` with the
  window needs 52 GB of the AGX Orin's 61 GB; headless halves that. Use `headless:=true` with
  `view:=true`, or a crop world (`bymarka_dense_s`: 10 GB headless). Never run two sims.
- **The Gazebo window takes minutes.** Normal for the Bymarka worlds: 1.7 min (`bymarka`) and
  2.5 min (`bymarka_dense`) until the drone camera runs. `bymarka_dense_s` loads in ~40 s.
- **Camera at ~21 Hz instead of 30.** The subscriber uses best effort QoS; use reliable (section 7).
- **No Gazebo window (Docker).** Run `./scripts/sim.sh up` again. It rewrites `.env` with your
  display settings. On Linux you may need `xhost +local:`.
- **Gazebo is very slow (Docker).** It renders on the CPU in the container. Use `headless:=true`
  and `cam_res:=480p cam_fps:=15`, and view the camera in `rqt_image_view` instead.
- **Upgrading PX4.** Change `PX4_VERSION` and `PX4_MSGS_BRANCH` together in the Dockerfile (and
  `install-user.sh`). If they don't match, the topics silently stop working.
