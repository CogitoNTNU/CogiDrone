# Simulation (PX4 + Gazebo + ROS 2 + YOLO)

Fly the drone in simulation while the hardware is on its way. Everything runs in one
Docker image, so all 8 of us get the same setup no matter which computer we use.

| | Windows (WSL2) | Linux | macOS |
|---|---|---|---|
| Full sim (Gazebo + PX4) | yes | yes | no |
| Write / build / run ROS 2 nodes, YOLO | yes | yes | yes |
| Replay recorded sim data (rosbag) | yes | yes | yes |

**What's inside.** ROS 2 Jazzy, PX4 v1.17.0 (SITL), Gazebo Harmonic, Micro XRCE-DDS Agent
v2.4.3, px4_msgs `release/1.17` and YOLO (ultralytics, CPU). All versions are pinned in
[`docker/sim/Dockerfile`](../docker/sim/Dockerfile) and nowhere else.

**Recommended PC:** 16 GB RAM and 30 GB free disk. 8 GB works in headless mode.

**On a Jetson?** Don't use Docker there. See [jetson.md](jetson.md) for the native install.

**How do the pieces connect?** See [architecture.md](architecture.md): machines, programs,
topics and frames on one page.

## 1. Install (once)

**Windows**

1. Install WSL2 with Ubuntu 24.04: `wsl --install -d Ubuntu-24.04` in PowerShell.
2. Install [Docker Desktop](https://www.docker.com/products/docker-desktop/). In
   *Settings → Resources → WSL integration*, enable Ubuntu-24.04.
3. **Clone the repo inside WSL** (e.g. `~/CogiDrone`), not under `C:\` or OneDrive.
   Builds are many times faster there.

**Linux:** install Docker Engine and add yourself to the `docker` group.

**macOS:** install Docker Desktop. Apple Silicon runs the image natively (arm64).

## 2. Build and start

From the repo root (inside WSL on Windows):

```bash
./scripts/sim.sh build   # first time: 20-40 min (compiles PX4). Later: seconds.
./scripts/sim.sh up
./scripts/sim.sh shell   # opens a shell in the container; open as many as you need
```

Inside the container, build our ROS 2 packages:

```bash
cb    # = colcon build --symlink-install in /workspace/ros2_ws, then sources it
```

Prefer VS Code? Run `./scripts/sim.sh build` once, then *Dev Containers: Reopen in
Container* and pick **CogiDrone sim** (Windows/Linux) or **CogiDrone dev** (macOS).

## 3. Fly (Windows / Linux)

There are two launch files. Both start the same simulator; they differ in who flies the drone.

**The C++ core** (`src/`, the main path):

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py
ros2 launch cogidrone_bringup sim_cpp.launch.py headless:=true yolo:=true
```

After about 20 s the C++ `Pixhawk` node arms, takes off to 2.5 m and hands over to `Navigation`,
which hovers and spins slowly to search. `yolo:=true` feeds YOLO's detections to Navigation.

**The Python reference controller:**

```bash
ros2 launch cogidrone_bringup sim.launch.py
```

Gazebo opens and the drone spawns. After about 20 s (while PX4 gets a GPS fix) it arms, takes off
to 3 m and turns to face the person in front of it. If nobody is in view it spins
slowly to search. Options:

```bash
ros2 launch cogidrone_bringup sim.launch.py headless:=true    # no Gazebo window (faster)
ros2 launch cogidrone_bringup sim.launch.py autonomy:=false   # only sim + bridge, run your own nodes
```

In a second shell:

```bash
ros2 topic list                          # /fmu/... (PX4), /camera/... (sim camera), /cogidrone/...
ros2 run rqt_image_view rqt_image_view   # pick /cogidrone/detections/image to see YOLO boxes
ros2 topic echo /fmu/out/vehicle_status_v1 --once
```

**Fly it yourself.** Start the sim without the autonomous controller, then in a
second shell run the keyboard controller (`t` take off, `w/a/s/d` move, `r/f` up/down,
`q/e` turn, `l` land):

```bash
ros2 launch cogidrone_bringup sim.launch.py autonomy:=false
ros2 run cogidrone_control teleop                                   # second shell
ros2 run rqt_image_view rqt_image_view /camera/camera/color/image_raw   # drone camera
```

**Low-spec PC (8 GB RAM).** Add `headless:=true`. The Gazebo 3D view is the heaviest
part; the drone camera window above still works.

**QGroundControl (optional).** Install it on your own machine and open it while the
sim runs. It connects by itself (PX4 sends to the host on UDP 14550). To fly with an RC
transmitter such as the RadioMaster Pocket, plug it in over USB and pick *USB Joystick (HID)*,
then in QGC go to *Vehicle Setup → Joystick*, enable and calibrate it, and fly in
*Position* mode.

**Search-and-rescue map.** `world:=sar` loads a 600 x 600 m map loosely like the outskirts
of Trondheim: a fjord to the north, a hilly spruce forest to the west, fields to the south and
a small town with collapsed buildings to the east. 24 people are spread over it: standing,
walking, sitting, waving and lying down (some under trees or next to rubble).

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py world:=sar
```

- The first start downloads about 200 MB of Gazebo Fuel models into `~/.gz/fuel` (one time).
  After that it takes about a minute to load before PX4 is ready.
- Where every person is: [`sim/worlds/sar_people.yaml`](../sim/worlds/sar_people.yaml).
  Use it to check what the drone should have found.
- New random people (and terrain): `python3 sim/tools/make_sar_world.py --seed 7`.
  On a slow PC, `--trees 500` makes the forest lighter. Don't edit the generated files by hand.
- Any world in `sim/worlds/` (or PX4's own, e.g. `world:=baylands`) can be picked with `world:=`.
- Ground textures are photo scans from [ambientCG](https://ambientcg.com) (CC0). Buildings,
  trees and people are Gazebo Fuel models.

**Bymarka (real terrain).** Two worlds built from open map data of the real forest around
Kobberdammen and Gråkallen in Bymarka, Trondheim: Kartverket's 1 m laser terrain, the real
lakes, paths and streams from OpenStreetMap, trees at their real positions and heights, a photo
sky and 30 km of real landscape around it. GPS in the sim matches the real place.

| World | Area | What it is for |
|---|---|---|
| `world:=bymarka_dense` | 600 x 600 m search area | As close to the real forest as we can make it: the canopy is filled in to real stand density (600-1200 trees/ha), young trees, bilberry, heather, rocks and logs on the ground. Use this one for detection and search tests. |
| `world:=bymarka` | 2 x 2 km | Bigger area with the trees the laser data resolves (sparser than reality). For long flights and navigation. |

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py world:=bymarka_dense
```

- The models are generated, not in git (several GB). Build them once (downloads ~200 MB of map
  data, then ~10 min):
  ```bash
  P=~/cogidrone_deps/simtools-venv/bin/python   # numpy, scipy, Pillow
  $P sim/tools/people/make_people.py                    # 36 differently dressed people (needs the Fuel people once)
  $P sim/tools/bymarka/make_world.py --forest --props   # shared terrain + the big world
  $P sim/tools/bymarka/make_dense.py                    # the dense world
  ```
- Measured on the AGX Orin (61 GB) with PX4 + the C++ core flying and the Gazebo window open:

  | | Drone camera after | Real-time factor | Camera | Gazebo server + window | RAM in use (desktop ~14 GB) |
  |---|---|---|---|---|---|
  | `bymarka_dense` | 2.5 min | 0.9-1.0 | 13.8 of 15 Hz | 22 + 21 GB | 52 GB |
  | `bymarka` | 1.7 min | ~0.8 | 11.5 Hz | 10 + 9 GB | 29 GB |

  On the Jetson the GPU memory is system RAM, and the server (drone camera) and the window each
  load everything. Most of the dense world's RAM is its detailed forest (~14 GB per process:
  Gazebo keeps several copies of every vertex). `headless:=true` halves it. Not tested on laptops
  yet; make the big world lighter with `make_world.py --skip-terrain --forest --props --near-radius 200`.
- Ground textures are BC1 `.dds` (1/8 of the RAM a JPG takes in Gazebo): 4096 px per 250 m tile
  within `--near-radius` (350 m) of the launch spot, `--tex-far` (2048) outside. The baked 4K
  masters stay in `sim/models/bymarka_terrain/materials/bake/`, so other sizes take a minute
  instead of a rebake: `make_world.py --retex --tex-far 1024`.
- People: 20-24 per world, standing, walking, sitting, waving and lying (face up or down), in
  the forest, on paths, by the lakes and on open rock. Clothes range from hi-vis and red to dark,
  green, brown and white; some lying people are injured. Where they are:
  `sim/worlds/bymarka_dense_people.yaml` / `bymarka_people.yaml`.
- The data licences (Kartverket CC BY 4.0, NIBIO NLOD, OpenStreetMap ODbL) need a credit line on
  slides and screenshots, see [`sim/ATTRIBUTION.md`](../sim/ATTRIBUTION.md).

## 4. Mac users: record and replay

Someone with the sim records a flight:

```bash
ros2 bag record -o /workspace/rosbags/flight1 \
  /camera/camera/color/image_raw /camera/camera/color/camera_info \
  /fmu/out/vehicle_status_v1 /fmu/out/vehicle_local_position_v1
```

Bags are big (about 1 GB/min) and are git-ignored, so share them on the team drive. On the
Mac, put the bag in `rosbags/` and run:

```bash
ros2 bag play /workspace/rosbags/flight1 --loop &
ros2 run cogidrone_perception detector
ros2 topic echo /cogidrone/detections
```

## How it fits together

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
| `sim/models/cogidrone_x500/` | Drone: PX4's x500 (our frame) + a camera matching the D435i |
| `sim/worlds/cogidrone.sdf` | World: NTNU Trondheim coordinates, two people as YOLO targets |
| `sim/worlds/sar.sdf` | Search-and-rescue map, generated by `sim/tools/make_sar_world.py` (with `sar_*` models) |
| `sim/tools/bymarka/` | Generators for the Bymarka worlds (`make_world.py`, `make_dense.py`); trees in `sim/tools/trees/`, ground clutter in `sim/tools/props/`, sky in `sim/tools/sky/` |
| `ros2_ws/src/cogidrone_bringup/` | `sim.launch.py` and the Gazebo→ROS bridge config |
| `ros2_ws/src/cogidrone_perception/` | `detector`: image → YOLO → `vision_msgs/Detection2DArray` |
| `ros2_ws/src/cogidrone_control/` | `offboard`: arm, take off, yaw toward target (PX4 offboard mode). `teleop`: keyboard |
| `ros2_ws/src/cogidrone_core/` | Builds the C++ core in `src/` so `sim_cpp.launch.py` can start it |

The camera topics use the same names as `realsense-ros`, so on the real drone you
swap the bridge for the RealSense driver and the nodes stay the same. One difference:
sim depth is `32FC1` in metres, while the real D435i gives `16UC1` in millimetres.

## Troubleshooting

- **No Gazebo window.** Run `./scripts/sim.sh up` again. It rewrites `.env` with your
  display settings. On Linux you may need `xhost +local:`.
- **Gazebo is very slow.** It renders on the CPU in the container. Use `headless:=true` and view
  the camera in `rqt_image_view` instead.
- **No `/fmu/...` topics.** The XRCE agent or PX4 didn't start. Check the launch output.
  PX4 topics with versioned messages end in `_v1` (e.g. `vehicle_status_v1`).
- **Upgrading PX4.** Change `PX4_VERSION` and `PX4_MSGS_BRANCH` together in the
  Dockerfile. If they don't match, the topics silently stop working.
