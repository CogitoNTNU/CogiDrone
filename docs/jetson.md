# Jetson (native, no Docker)

The Docker setup in [sim.md](sim.md) is for laptops (Windows, Linux, macOS). On the Jetsons we
install everything **directly on the OS**, because the flight Jetson must run the real code without
Docker, and the sim Jetson should match it.

| Jetson | Board | Role | What runs on it |
|---|---|---|---|
| Sim / dev box | AGX Orin 64 GB | `sim` | Everything: Gazebo + PX4 SITL + ROS 2 + YOLO + cuVSLAM + QGroundControl |
| Flight computer | Orin Nano Super 8 GB | `drone` | Everything except the simulator (no Gazebo, no PX4 SITL) |

Both run **JetPack 7.2.1** (L4T r39.2, Ubuntu 24.04), so the same scripts and versions work on both.

## Quick start: run the sim (sim Jetson, already installed)

1. **Open a new terminal.** Your `~/.bashrc` loads `scripts/jetson/env.sh`, which sets up ROS, PX4,
   CUDA and the Python packages. Nothing else to source.
2. **Build our ROS 2 packages** (first time, and after you change code in `ros2_ws/src`):
   ```bash
   cb
   ```
3. **Start the sim:**
   ```bash
   ros2 launch cogidrone_bringup sim.launch.py
   ```
   Gazebo opens with the drone. After ~10-20 s (GPS fix) it arms, takes off to 3 m and turns toward
   the person in view. Add `headless:=true` for no Gazebo window, or `autonomy:=false` to start only
   the sim (PX4 + Gazebo + bridge) and run your own nodes.
4. **Watch / fly it** (each in its own terminal, optional):
   ```bash
   qgc                                                                # QGroundControl, connects by itself
   ros2 run rqt_image_view rqt_image_view /cogidrone/detections/image  # camera with YOLO boxes
   ros2 run cogidrone_control teleop                                   # keyboard flying (use autonomy:=false)
   ```
5. **Stop:** `Ctrl-C` in the launch terminal stops PX4, Gazebo, the agent and the nodes.
   Only one sim can run at a time: a second `ros2 launch` refuses to start and tells you which
   process to stop first.

Something wrong? Run `scripts/jetson/smoke-test.sh sim`. It checks every piece and tells you
which one fails.

## Versions

| Component | Version | Notes |
|---|---|---|
| JetPack / L4T | 7.2.1 / r39.2.1 | First JetPack 7 that supports all Orin boards (incl. Orin Nano Super) |
| CUDA / cuDNN / TensorRT | 13.2 / 9.20 / 10.16 | From the JetPack apt repo, but **not** the `nvidia-jetpack` meta package (see below) |
| ROS 2 | Jazzy | Same as Docker |
| PX4-Autopilot (SITL) | v1.17.0 | Same as Docker. Sim role only |
| px4_msgs | `release/1.17` | **Must match the PX4 version** on the Pixhawk and in SITL |
| Micro XRCE-DDS Agent | v2.4.3 | **Must stay v2.x**: PX4's built-in client is v2 and does not talk to a v3 agent |
| Gazebo | Harmonic | Sim role only |
| PyTorch / torchvision | 2.14.1 / 0.29.1 (`cu132`) | Official aarch64 CUDA wheels from download.pytorch.org. Not pinned yet, so you get the newest |
| YOLO | ultralytics 8.4, `yolo11n.pt` | Runs on the GPU (`cuda:0`) |
| cuVSLAM | v17.0.0 | Built from source for Orin (`sm_87`); NVIDIA has no prebuilt for Orin + JetPack 7 |
| QGroundControl | v5.1.4 (aarch64 AppImage) | Ground station. Sim role. Start with `qgc` |
| RTAB-Map | 0.23.7 (apt) | CPU VIO + 3D mapping, fallback for cuVSLAM |

The versions are pinned at the top of `scripts/jetson/install-user.sh` and must agree with
`docker/sim/Dockerfile`. If you bump one, bump the other.

## 1. Install (once per Jetson)

```bash
cd ~/Cogito/CogiDrone
sudo scripts/jetson/install-system.sh sim     # or: drone   (apt packages, ~20 min)
scripts/jetson/install-user.sh sim            # or: drone   (no sudo, ~30-60 min, builds PX4 + cuVSLAM)
```

Then log out and in once (new groups), or open a new shell. `install-user.sh` adds
`source .../scripts/jetson/env.sh` to your `~/.bashrc`.

Both scripts can be re-run safely. `install-user.sh` skips anything already built. Delete a folder in
`~/cogidrone_deps` to rebuild it, or set `SKIP_CUVSLAM=1` to skip the long cuVSLAM build.

### What the scripts do

**`install-system.sh` (sudo)**

1. CUDA 13.2, cuDNN, TensorRT, Jetson multimedia API.
2. Build tools (cmake, ninja, git-lfs, ...).
3. ROS 2 Jazzy packages: vision_msgs, cv_bridge, tf2, realsense2_camera, foxglove_bridge, rtabmap_ros, ...
4. `sim` only: Gazebo Harmonic (OSRF apt repo), ros_gz bridge, PX4 build dependencies.
5. Adds you to `dialout video render plugdev`, disables `nvgetty` (frees the UART for the Pixhawk),
   and takes back ownership of `ros2_ws/` (old Docker runs leave root-owned folders there).

**`install-user.sh` (no sudo)**, everything goes in `~/cogidrone_deps`, outside the repo:

| Folder | What |
|---|---|
| `venv/` | Python venv (`--system-site-packages`): torch, ultralytics, PX4 python deps, PyCuVSLAM |
| `xrce/` | Micro XRCE-DDS Agent, built **without** ROS in the environment (it ships its own Fast DDS) |
| `px4_ws/` | colcon workspace with `px4_msgs` |
| `PX4-Autopilot/` | PX4 SITL build. Our model/world are symlinked in from `sim/` |
| `cuVSLAM/` | cuVSLAM source + `build/bin/libcuvslam.so` |
| `models/` | YOLO weights |
| `bin/` | `xrce-agent` wrapper, `QGroundControl.AppImage`, `qgc` launcher (always use `qgc`) |

**`env.sh`**: the native version of `docker/sim/env.sh`. Sources ROS + px4_msgs + `ros2_ws`, puts
CUDA, the venv and `bin/` on the path, sets `PX4_DIR`, `COGIDRONE_YOLO_MODEL`,
`COGIDRONE_YOLO_DEVICE=cuda:0`, and defines `cb` (build `ros2_ws`).

## 2. Check that it works

```bash
scripts/jetson/smoke-test.sh sim      # or: drone
```

Checks CUDA, torch on the GPU, TensorRT, YOLO on the GPU, cuVSLAM, ROS + px4_msgs, the `ros2_ws`
build, the XRCE agent and the C++ build. With `sim` it then starts the full sim headless and
waits for PX4 topics, the camera, YOLO detections and a takeoff to ~3 m. Logs go to
`/tmp/cogidrone-smoke.*`.

> The **C++ core build (`src/`) fails at link time on purpose for now**: `main.cpp` and
> `perception/perception.cpp` are not listed in `src/CMakeLists.txt`. Everything else should pass.

## 3. Fly in the sim (sim Jetson)

Same commands as in [sim.md](sim.md), just without `./scripts/sim.sh shell`. Pick the takeoff
height with `altitude:=` (default 3 m):

```bash
cb                                                    # build our ROS 2 packages
ros2 launch cogidrone_bringup sim.launch.py           # Gazebo window + PX4 + YOLO + offboard
ros2 launch cogidrone_bringup sim.launch.py headless:=true autonomy:=false
ros2 launch cogidrone_bringup sim.launch.py altitude:=1.0       # take off to 1 m instead of 3 m
ros2 run cogidrone_control teleop                     # keyboard flying (second shell)
qgc                                                   # QGroundControl, connects by itself
```

**Who is the flight controller?** In the sim it is **PX4 SITL**: the same PX4 firmware as on the
Pixhawk, compiled as a Linux program and running on the Jetson against Gazebo. `sim.launch.py`
starts it. On the real drone PX4 runs on the Pixhawk instead, and the Jetson talks to it through
the XRCE agent.

### Fly with the RadioMaster Pocket (USB joystick)

1. Plug the radio in with a **data** USB-C cable (many cables only charge) and pick **Joystick**
   on the radio. `ls /dev/input/js*` must show `/dev/input/js0`.
2. Use a **fresh model** on the radio (default template, plain AETR mixes). Our old model only sent
   throttle over USB, and switches on axes made channels look off-centre in QGC.
3. Start the sim with `autonomy:=false`, then QGC with **`qgc`**, not the AppImage directly.
   `qgc` gives SDL (inside QGC) a joystick mapping: without it SDL treats the radio as a gamepad
   with the throttle on a half-range "trigger", and QGC's calibration never gets past throttle
   up/down. See `scripts/jetson/qgc`.
4. QGC: *Vehicle Setup → Joystick*, **Mode 2**, throttle mid + right stick centred, **Calibrate**,
   hold each position still for 2-3 s, then tick **Enable**.
5. Flight mode **Position**, throttle down, arm, throttle up past the middle to take off.

## 4. Real drone (Orin Nano Super)

The Pixhawk talks to the Jetson over serial (TELEM2). Start the agent on the UART instead of UDP:

```bash
xrce-agent serial --dev /dev/ttyTHS1 -b 921600
```

The Pixhawk must be configured to match (set in QGroundControl, see `config/px4.params` on the
`px4MessageSetup` branch): `UXRCE_DDS_CFG` = TELEM2, `SER_TEL2_BAUD` = 921600,
`UXRCE_DDS_DOM_ID` = `ROS_DOMAIN_ID` (0). A wrong port, baud or domain all fail the same silent
way: no `/fmu/...` topics, no error anywhere.

> **px4_msgs must match the flashed PX4 firmware.** We use `release/1.17` (PX4 v1.17). The
> `px4MessageSetup` branch still has a `release/1.15` placeholder. Decide one version and flash
> the Pixhawk with it.

## Localization and mapping plan

Goal: hold the drone stable without GPS, and build a 3D map onboard.

- **cuVSLAM (primary)**, stereo + IMU from the D435i. On EuRoC (drone dataset) it gets 0.29 %
  error stereo-inertial vs 5.70 % for ORB-SLAM3. It takes 3.8 ms/frame on AGX Orin and stays
  under 40 % GPU at 30 FPS on an Orin Nano Super.
  Feed its **odometry** (not the loop-closed SLAM pose, which can jump) into PX4 EKF2 via
  `/fmu/in/vehicle_visual_odometry`.
- **nvblox (3D map)**: GPU voxel mapping from D435i depth + cuVSLAM pose. It gives a live mesh
  (Foxglove/RViz) and a distance field for obstacle avoidance. Not installed yet: source build.
- **RTAB-Map (fallback)**: `ros-jazzy-rtabmap-ros`, installed by `install-system.sh` (on Jetsons set up
  before it was added: `sudo apt install ros-jazzy-rtabmap-ros`). CPU VIO + loop closure +
  point cloud / OctoMap. Slower, but works today.

To use cuVSLAM in the sim, the sim drone needs the D435i's two IR cameras and an IMU added to
`sim/models/cogidrone_x500/model.sdf`. Right now it only has the color and depth cameras.

Sources: [cuVSLAM paper](https://arxiv.org/html/2506.04359v2),
[nvblox paper](https://arxiv.org/pdf/2311.00626).

## Gotchas (why the scripts look the way they do)

- **Never `apt install nvidia-jetpack`.** It pulls NVIDIA's OpenCV 4.8, which replaces Ubuntu's
  `libopencv-dev` 4.6 that all of ROS 2 Jazzy is built against. Even `ros-jazzy-cv-bridge` would
  pull it, because the L4T repo has the higher version. `install-system.sh` writes
  `/etc/apt/preferences.d/cogidrone-no-nvidia-opencv` to block it. As a result, NVIDIA's VPI
  (which needs that OpenCV) is not installed either.
- **`nvidia-cuda-dev` is Ubuntu's CUDA 12.0**, not JetPack's. Install `cuda-toolkit-13-2`.
- **numpy stays < 2** (`~/cogidrone_deps/constraints.txt`), because the apt ROS packages are
  built against numpy 1.x.
- **XRCE agent and ROS have different Fast DDS builds** with the same soname. The `xrce-agent`
  wrapper sets the agent's own library path. Never put the ROS libs on it.
- **Isaac ROS**: 4.6 is the last release for Jazzy (5.0 moved to ROS 2 Lyrical). There are no
  Jazzy apt packages, so we use the standalone cuVSLAM library instead of `isaac_ros_visual_slam`.
- **JetPack 6 is not an option** without dropping to Ubuntu 22.04 / ROS Humble and reflashing
  both Jetsons. Its only advantage would be a prebuilt cuVSLAM.

## Open issues

- **Versions are pinned twice** (Dockerfile + `install-user.sh`). A shared versions file would
  stop them drifting apart.
- **torch and ultralytics are not version-pinned** yet (torch comes from `--pre` wheels).
- **The `drone` role is untested** on the Orin Nano Super. The full CUDA toolkit (with Nsight) is
  heavy for its disk, so the package list may need trimming.
- **`nvgetty` is disabled on the sim Jetson** too, where it isn't needed.
- **The C++ core (`src/`) doesn't link**: `main.cpp` and `perception/perception.cpp` are missing
  from `src/CMakeLists.txt`.

## Fixed along the way

- **Gazebo kept running after Ctrl-C.** PX4's start script launches Gazebo in the background and
  orphans it (Docker hid this: stopping the container kills everything). Leftover Gazebo
  processes then broke the next run: PX4 got "Accel #0 fail: TIMEOUT", refused to arm, and the
  altitude estimate jumped around. `sim.launch.py` now starts Gazebo itself and runs PX4 with
  `PX4_GZ_STANDALONE=1`, so Ctrl-C stops everything. Verified on the AGX Orin, with and without
  the Gazebo window.
- **Two sims at once** fed PX4 IMU data from both ("timestamp error"). The launch now refuses to
  start while a Gazebo or PX4 SITL is already running.
- **"Accel #0 fail: TIMEOUT" with a single sim.** Gazebo's transport (gz <-> PX4 <-> ros_gz_bridge)
  binds to the first network interface by default, here the campus Wi-Fi, and IMU messages got
  delayed there: 3-12 timeouts per minute, worse with the Gazebo window, and PX4 refused to arm.
  `sim.launch.py` and `env.sh` now set `GZ_IP=127.0.0.1` (loopback): 0 timeouts per minute, with
  and without the window. `sudo jetson_clocks` (GPU locked at max) did not fix it on its own.
