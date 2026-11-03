# How everything is connected

One page that shows which machine runs what, which program talks to which, and over which
topic. Details live in the linked pages:

| Page | What it covers |
|---|---|
| [hardware.md](hardware.md) | Parts on the drone, wiring, power, PX4 parameters |
| [jetson.md](jetson.md) | Installing and running everything natively on the Jetsons |
| [sim.md](sim.md) | The simulator: worlds, launch arguments, camera, generating worlds, troubleshooting |
| [README](../README.md) | Start here: the common workflows step by step |
| [`scripts/search_test/README.md`](../scripts/search_test/README.md), [`tools/flight_viewer/README.md`](../tools/flight_viewer/README.md) | The search test and the flight viewer (section 6) |

## 1. Machines

```mermaid
flowchart LR
    PC["Dev PC (Windows)<br/>edit code, git"]
    GH["GitHub<br/>CogitoNTNU/CogiDrone"]
    SIM["Sim Jetson<br/>AGX Orin 64 GB"]

    subgraph Drone["Drone (real flights)"]
        NANO["Jetson Orin Nano Super"]
        FC["Pixhawk 6C<br/>(PX4)"]
        CAM["RealSense D435i"]
    end

    PC -- "git push / pull" --> GH
    PC -- "SSH: run, test,<br/>git push over SSH" --> SIM
    NANO -- "USB serial<br/>uXRCE-DDS" --> FC
    CAM -- USB 3 --> NANO
```

- **The dev PC** is where code is written and the only machine that pushes to GitHub.
- **The sim Jetson** has no GitHub login. Code reaches it over SSH from the PC
  (`git push ssh://<user>@<sim-jetson>/~/Cogito/CogiDrone <branch>`), and you run the sim there.
- **The drone** runs the same code. The only difference is that PX4 runs on the Pixhawk and the
  images come from the real camera instead of Gazebo (see [section 5](#5-sim-vs-real-drone)).

## 2. The two ways to fly the sim

Both start the same simulator. They differ in **who flies the drone**. Never run both at once:
two programs sending setpoints to PX4 fight each other.

| | C++ core (main path) | Python reference |
|---|---|---|
| Launch | `ros2 launch cogidrone_bringup sim_cpp.launch.py` | `ros2 launch cogidrone_bringup sim.launch.py` |
| Who flies | `cogidrone` (C++, `src/`) | `offboard` (Python, `ros2_ws/src/cogidrone_control`) |
| Behaviour | Take off to 2.5 m, Navigation state machine (search / follow), land on request | Take off to 3 m, yaw toward the person YOLO sees |
| YOLO boxes | Only with `yolo:=true` | Always |
| Fly by keyboard | No | Add `autonomy:=false`, then `ros2 run cogidrone_control teleop` |

## 3. What runs in the sim

```mermaid
flowchart TB
    subgraph SIMU["Simulator (started by both launch files)"]
        direction LR
        GZ["Gazebo Harmonic<br/>world + x500 + camera"]
        PX4["PX4 SITL<br/>autopilot + EKF2"]
        AG["XRCE-DDS agent<br/>UDP 8888"]
        BR["ros_gz_bridge"]
        GZ <-- "sensors, motors" --> PX4
        PX4 <-- uORB --> AG
        GZ -- camera --> BR
    end

    QGC["QGroundControl<br/>(optional)"]
    YOLO["detector<br/>(Python YOLO, optional)"]

    subgraph CORE["cogidrone process (C++)"]
        direction LR
        PER["Perception"]
        NAV["Navigation"]
        PIX["Pixhawk"]
        PER -. "/perception/detections<br/>(nothing yet)" .-> NAV
        NAV -- "velocity_setpoint<br/>mission_status" --> PIX
        PIX -- "vio_odometry<br/>START_MISSION" --> NAV
    end

    PX4 -- "MAVLink UDP 14550" --> QGC
    AG <-- "/fmu/in/*<br/>/fmu/out/*" --> PIX
    BR -- "color/image_raw" --> PER
    BR -- "color/image_raw" --> YOLO
    YOLO -. "/cogidrone/detections<br/>(yolo:=true)" .-> NAV
```

Labels are shortened; the full topic names are in [section 4](#4-topics). Dashed arrows are
planned or optional. Today the C++ Perception loads stub models and publishes
nothing, so without `yolo:=true` Navigation stays in `SEARCHING` (hover and spin slowly).

### Inside the `cogidrone` process

`main.cpp` creates one `Drone`, which owns three subsystems. Each one is a ROS 2 node, and all
three spin in one `MultiThreadedExecutor` (intra-process, so messages between them are not copied).

| Subsystem | File | Job |
|---|---|---|
| `Perception` | `src/perception/` | Camera image → detections. **Models are stubs for now** |
| `Navigation` | `src/navigation/` | Mission state machine: `IDLE → SEARCHING → PERSON_DETECTED → FOLLOWING`, plus `LANDING` / `EMERGENCY`. Outputs a velocity |
| `Pixhawk` | `src/controller/` | The only link to PX4. Converts frames, arms, takes off, follows Navigation's velocity, lands |
| Estimation | `src/estimation/` | Not written yet. Until then `Pixhawk` publishes PX4's EKF2 estimate as the odometry |
| Representation | `src/representation/` | Not written yet (map / obstacles) |

### Pixhawk state machine

```mermaid
stateDiagram-v2
    [*] --> WAITING_EKF
    WAITING_EKF --> STREAMING: PX4 has a position fix
    STREAMING --> TAKEOFF: armed + offboard<br/>(requested every 2 s until accepted)
    TAKEOFF --> FLYING: within 0.3 m of target_height<br/>sends START_MISSION
    FLYING --> LANDING: Navigation says LANDING
    TAKEOFF --> DISARMED: PX4 disarmed
    FLYING --> DISARMED: PX4 disarmed
    LANDING --> DISARMED: PX4 disarmed after touchdown
```

In `FLYING` it follows Navigation's velocity while Navigation keeps sending one (at least every
0.5 s). If Navigation goes quiet it holds the current position instead of drifting.

## 4. Topics

| Topic | Type | From | To | Frame |
|---|---|---|---|---|
| `/fmu/out/vehicle_local_position_v1` | `px4_msgs/VehicleLocalPosition` | PX4 | Pixhawk, Python nodes | NED |
| `/fmu/out/vehicle_status_v1` | `px4_msgs/VehicleStatus` | PX4 | Pixhawk, Python nodes | — |
| `/fmu/in/offboard_control_mode` | `px4_msgs/OffboardControlMode` | Pixhawk | PX4 | — |
| `/fmu/in/trajectory_setpoint` | `px4_msgs/TrajectorySetpoint` | Pixhawk | PX4 | NED |
| `/fmu/in/vehicle_command` | `px4_msgs/VehicleCommand` | Pixhawk | PX4 | — |
| `/estimation/vio_odometry` | `nav_msgs/Odometry` | Pixhawk (stand-in for Estimation) | Navigation | ENU |
| `/navigation/velocity_setpoint` | `geometry_msgs/TwistStamped` | Navigation | Pixhawk | ENU, world frame |
| `/navigation/mission_status` | `std_msgs/String` | Navigation | Pixhawk | — |
| `/manual_override` | `std_msgs/String` | Pixhawk, or you | Navigation | — |
| `/camera/camera/color/image_raw` | `sensor_msgs/Image` | ros_gz_bridge (sim) / realsense-ros (drone) | Perception, detector | camera |
| `/perception/detections` | `vision_msgs/Detection2DArray` | Perception (nothing yet) | Navigation | image |
| `/cogidrone/detections` | `vision_msgs/Detection2DArray` | detector (YOLO) | Navigation (with `yolo:=true`), `offboard` | image |
| `/cogidrone/detections/image` | `sensor_msgs/Image` | detector | `rqt_image_view` (camera with boxes) | image |
| `/camera/camera/color/camera_info` | `sensor_msgs/CameraInfo` | ros_gz_bridge / realsense-ros | `search_test` | camera |
| `/camera/camera/depth/image_rect_raw` | `sensor_msgs/Image` | bridge started by `search_test` (sim) / realsense-ros | `search_test` | camera |
| `/fmu/out/vehicle_attitude` | `px4_msgs/VehicleAttitude` | PX4 | `search_test` | NED |
| `/search_test/image` | `sensor_msgs/Image` | `search_test` | `rqt_image_view` (camera with boxes + status) | image |
| `/search_test/person_pings` | `geometry_msgs/PointStamped` | `search_test` (one per new person) | anyone | local |

PX4 topics with versioned messages end in `_v1`. A wrong name receives nothing and gives no error.

`/manual_override` takes plain words, so you can steer the mission from a terminal:

```bash
ros2 topic pub --once /manual_override std_msgs/msg/String "{data: LAND}"
# also: START_MISSION, RETURN_HOME, EMERGENCY_STOP
```

### Frames: ENU vs NED

PX4 uses **NED** (x north, y east, z down, so up is negative). ROS uses **ENU** (x east,
y north, z up). The `Pixhawk` class is the only code that converts between them. Everything
else in `src/` is ENU:

| | ENU (ROS, our code) | NED (PX4) |
|---|---|---|
| x | east | north |
| y | north | east |
| z | up | down |
| yaw | from east, counter-clockwise | heading from north, clockwise |

## 5. Sim vs real drone

Only the bottom layer changes. The topic names are the same, so the code above it does not change.

| | Sim | Real drone |
|---|---|---|
| Autopilot | PX4 SITL on the sim Jetson | PX4 on the Pixhawk 6C |
| PX4 ↔ ROS | `xrce-agent udp4 -p 8888` | `xrce-agent serial --dev /dev/ttyUSB0 -b 921600` (TELEM2, see [hardware.md](hardware.md#pixhawk--jetson-link)) |
| Camera | Gazebo camera → `ros_gz_bridge` | RealSense D435i → `realsense2_camera` |
| Depth image | `32FC1`, metres | `16UC1`, millimetres |
| Position | Gazebo GPS → PX4 EKF2 | GPS outdoors, cuVSLAM later (see [jetson.md](jetson.md#localization-and-mapping-plan)) |
| Started by | `sim_cpp.launch.py` | Not written yet |

## 6. Search test and flight viewer

How a person sighting becomes a position on the map. `scripts/search_test` flies and detects
(it is a third flyer: run it with `sim.launch.py autonomy:=false`, never next to the C++ core or
`offboard`). The run log is the hand-over: anything that writes it (sim or real flight) can be
opened in the viewer.

```mermaid
flowchart LR
    CAM["Camera<br/>color + depth"]
    PX4["PX4<br/>local position + attitude"]

    subgraph ST["search_test.py (in flight)"]
        direction TB
        YOLO["YOLO<br/>person boxes"]
        HEAD["Head detector<br/>(own thread, optional)"]
        RAY["Sighting<br/>camera pose + rays<br/>feet / centre / head"]
        EST["Position now<br/>depth ≤ 10 m, else<br/>feet ray hits ground / DEM"]
        CLU["Cluster into persons<br/>PING after 3 sightings"]
        YOLO --> RAY
        YOLO -- crop --> HEAD
        HEAD -- head box --> RAY
        RAY --> EST --> CLU
    end

    LOG[("Run log<br/>~/flight_runs/latest<br/>track, frames, sightings,<br/>persons, truth")]
    OUT["~/search_test_runs/...<br/>summary, score vs<br/>*_people.yaml"]

    subgraph VW["tools/flight_viewer (after the flight)"]
        direction TB
        TER["terrain.py<br/>map background"]
        SOL["solver.js<br/>weighted least squares<br/>per person"]
        UI["Browser: 3D map,<br/>path, frames, spheres,<br/>solved persons"]
        TER --> UI
        SOL --> UI
    end

    CAM -- image --> YOLO
    CAM -- depth --> EST
    PX4 -- pose --> RAY
    CLU --> OUT
    RAY -- every observation --> LOG
    CLU -- persons --> LOG
    LOG --> TER
    LOG --> SOL
```

| Step | Where | What |
|---|---|---|
| Detect | `search_test.py` | Stock YOLO (person class) on the newest frame, in its own thread |
| Head | `search_test.py`, `tools/flight_viewer/heads.py` | Head box on the person crop: distance = head height × fy / head px (Muhammad's `depth_head.py`). In flight, or after it on the saved crops |
| Sighting | `search_test.py` → `sightings.csv` | Camera pose from PX4, rays through the feet, box centre and head, and every distance estimate: depth camera, terrain hit, head size, body height |
| Position in flight | `search_test.py` | Depth within 10 m, else the feet ray meets flat ground or the real terrain (`--dem`). Sightings within 4 m are one person; a person with 3 sightings is pinged |
| Run log | `tools/flight_viewer/runlog.py` | Format in the [viewer README](../tools/flight_viewer/README.md#run-log-format-version-2): local ENU around the EKF origin |
| Position after the flight | `tools/flight_viewer/solver.js` | Re-solves every person from the raw observations with weights you set (ray triangulation, terrain, head, body, depth), 2σ ellipse, error vs the sim truth |

## 7. Where things live in the repo

| Path | What |
|---|---|
| `src/` | The C++ drone core (`cogidrone`). Built by CMake on its own, or by colcon through `ros2_ws/src/cogidrone_core` |
| `ros2_ws/src/cogidrone_core/` | A thin ROS package that builds `src/` so `ros2 launch` can start it |
| `ros2_ws/src/cogidrone_bringup/` | Launch files (`sim.launch.py`, `sim_cpp.launch.py`) and the Gazebo → ROS bridge config |
| `ros2_ws/src/cogidrone_perception/` | Python YOLO `detector` |
| `ros2_ws/src/cogidrone_control/` | Python `offboard` controller and keyboard `teleop` (reference / manual flying) |
| `sim/` | Gazebo worlds (`worlds/cogidrone.sdf`: two people; `worlds/sar.sdf`: 600 m search-and-rescue map from `tools/make_sar_world.py`; the generated Bymarka worlds from `tools/bymarka/`, see [sim.md](sim.md#3-worlds)) and drone model (`models/cogidrone_x500`) |
| `scripts/search_test/` | Search test: route flight, YOLO + head detector, sightings, pings, run log, scoring (throwaway, section 6) |
| `tools/flight_viewer/` | Run-log writer, 3D viewer, position solver, map backgrounds, post-flight head detection (section 6) |
| `python/cv_prototyping/` | Stand-alone perception prototypes (webcam/image scripts, not ROS nodes): person/head detection, pose, segmentation, distance estimation (`inference/`) and target choosing (`targeter/`) |
| `python/cv_training/` | YOLO training on NTNU's IDUN cluster (`training.py`, run with `run_yolo.sh`, logs to wandb) and dataset filtering. Dataset links: `python/dataset_lenker.txt`, IDUN notes: `idun.txt` |
| `scripts/jetson/` | Native install scripts, `env.sh` (sourced by `~/.bashrc`), smoke test, `qgc` launcher |
| `docker/sim/`, `compose.yaml`, `scripts/sim.sh` | The Docker sim for laptops (see [sim.md](sim.md)) |
| `docker/Dockerfile`, `build.sh`, `build.bat` | Cross-compiling `src/` for the Jetson from another machine |
| `vendored/` | Pre-built aarch64 ROS 2 + `px4_msgs` for cross-compiling `src/` from Windows |
