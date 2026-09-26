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

```
Gazebo (cogidrone_x500 + D435i-like camera)
   │ camera ──► ros_gz_bridge ──► /camera/camera/color/image_raw ──► detector (YOLO)
   │                                                                     │ /cogidrone/detections
PX4 SITL ◄── XRCE-DDS agent (UDP 8888) ◄── /fmu/in/* ◄──────────── offboard (control)
```

| Where | What |
|---|---|
| `sim/models/cogidrone_x500/` | Drone: PX4's x500 (our frame) + a camera matching the D435i |
| `sim/worlds/cogidrone.sdf` | World: NTNU Trondheim coordinates, two people as YOLO targets |
| `ros2_ws/src/cogidrone_bringup/` | `sim.launch.py` and the Gazebo→ROS bridge config |
| `ros2_ws/src/cogidrone_perception/` | `detector`: image → YOLO → `vision_msgs/Detection2DArray` |
| `ros2_ws/src/cogidrone_control/` | `offboard`: arm, take off, yaw toward target (PX4 offboard mode) |

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
