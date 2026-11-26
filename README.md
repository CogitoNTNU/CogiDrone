# CogiDrone

<div align="center">

![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/CogitoNTNU/CogiDrone/ci.yml)
![GitHub top language](https://img.shields.io/github/languages/top/CogitoNTNU/CogiDrone)
![GitHub language count](https://img.shields.io/github/languages/count/CogitoNTNU/CogiDrone)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Project Version](https://img.shields.io/badge/version-0.0.1-blue)](https://img.shields.io/badge/version-0.0.1-blue)

<img src="docs/images/project-logo.webp" width="50%" alt="Cogito Project Logo" style="display: block; margin-left: auto; margin-right: auto;">
</div>

<details> 
<summary><b>Table of contents</b></summary>

- [CogiDrone](#cogidrone)
  - [Description](#description)
  - [Demo](#demo)
  - [How it works](#how-it-works)
    - [Two inference backends](#two-inference-backends)
  - [Prerequisites](#prerequisites)
  - [Getting started](#getting-started)
    - [Simulation (any OS)](#simulation-any-os)
    - [Jetson (onboard)](#jetson-onboard)
    - [Cross-compile (syntax check)](#cross-compile-syntax-check)
  - [Usage](#usage)
  - [Repository layout](#repository-layout)
  - [Testing](#testing)
    - [Generate Documentation Site](#generate-documentation-site)
  - [Team](#team)
    - [License](#license)

</details>

## Description

An autonomous drone that finds a person, follows them, and drops a payload.

CogiDrone is the onboard autonomy stack for a Pixhawk 6C + Jetson Orin airframe. It runs as a single C++23 binary on ROS 2 Jazzy, talks to PX4 over uXRCE-DDS, and does inference on the Jetson GPU with TensorRT. PX4 owns stabilisation, EKF2, and every failsafe; this stack only ever publishes position setpoints.

The mission, in order:

| # | State | What happens | Status |
|---|---|---|---|
| 1 | `SEARCHING` | Rotate in place until a person is detected | Implemented |
| 2 | `PERSON_DETECTED` | Confirm the track before committing | Implemented |
| 3 | `FOLLOWING` | Stand off behind the person, face them | Implemented |
| 4 | `DROPPING` | Release the payload | Blocked - no hardware |
| 5 | `RETURNING` | Fly back to the launch point | Blocked - no home policy |
| 6 | `LANDING` | Hand the descent to PX4 | Unreachable |

`FOLLOWING` is the terminal state today. That is the intended behaviour, not a bug - see [issue #29](https://github.com/CogitoNTNU/CogiDrone/issues/29).

## Demo

<!--
  Videos live in docs/video/. Embed them here.

  GitHub renders <video> tags as nothing, so use ONE of:
    1. Drag the .mp4 into a PR/issue comment, copy the
       https://github.com/user-attachments/assets/... URL, and paste it here.
    2. Commit a GIF to docs/images/ and use ![](docs/images/demo.gif).

  Do not use a relative path to docs/video/*.mp4 - it works on GitHub but
  breaks on the mkdocs site, because docs/index.md is a symlink to this file.
-->

[Person following in the Bymarka world](https://github.com/CogitoNTNU/CogiDrone/blob/main/docs/videos/bymarka_follow_demo_30fps.mp4)

## How it works

```
                    ┌──────────────────────────────────────────┐
                    │              cogidrone                   │
                    │         (one process, one binary)        │
                    │                                          │
  camera ──────────►│  Perception ──► Navigation ──► Flight    │
  /camera/...       │      │              │         Controller │
                    │      │              │              │     │
                    │      ▼              ▼              ▼     │
                    │  Detection2DArray  setpoints   Offboard  │
                    └──────────────────────────────────────────┘
                                                    │
                                                    ▼
                              PX4 (Pixhawk 6C) ◄────┘
                                    │
                                    ▼
                              motors / ESCs
```

**Composition, not inheritance.** `Drone` owns `Perception`, `Navigation`, and `FlightController` as `unique_ptr`s. Each subsystem has a private `M` assembly struct and a `create()` factory returning `std::expected`. A subsystem is either fully constructed or it does not exist - there is no partially-initialised state to read from.

**Perception** (`src/perception/`) - camera frame in, detections out, zero
allocation per frame.

- `TrtEngine` / `OnnxEngine` - one inference backend per build, selected at configure time. Both expose the same four members, so `Yolo<Layout>` does not know which one it holds.
- `Yolo<Layout>` - letterbox $\rightarrow$ normalise $\rightarrow$ infer $\rightarrow$ decode $\rightarrow$ NMS. Decode and NMS scratch live in members and are reused. `OutputLayout` is a template parameter, so `iouThreshold` cannot be passed to an NMS-free model.
- `Person` / `Head` - thin wrappers binding weights, thresholds, and class tables.
- Distance is **pinhole geometry from head box height** (preferred) or body box height (fallback) - not a depth map. This matches what the team validated in `python/cv_prototyping/`.

**Navigation** (`src/navigation/`) - two orthogonal state machines.

- `ControlState` - `WAITING_FOR_ARM` / `PILOT_HAS_CONTROL` / `NAVIGATION_HAS_CONTROL`, derived from two booleans FlightController publishes.
- `MissionState` - `IDLE` / `SEARCHING` / `PERSON_DETECTED` / `FOLLOWING` / `PERSON_LOST` / `LANDING`.
- Thread safety is by **executor serialisation, not a mutex**. All callbacks join the node's default `MutuallyExclusive` group, so `MultiThreadedExecutor` cannot run two concurrently. Proven from rclcpp source and documented in `navigation.h`.

**FlightController** (`src/controller/`) - the PX4 offboard handshake. Stream `OffboardControlMode` at ≥2 Hz for 1 s, then request the mode, then keep streaming. Publishes `/pixhawk/armed` and `/pixhawk/offboard_active` as `transient_local` so a late-joining Navigation learns state immediately.

**Math** (`src/math/`) - `Vec3`, `Quat`, `Transform`. `transform.h` is entirely `constexpr` and carries two `static_assert`s proving the camera $\rightarrow$ body axis mapping. If that mapping ever breaks, the build fails instead of the drone following a ghost.

### Two inference backends

| Backend | Model | Runs on | Preset |
|---|---|---|---|
| TensorRT | `models/*.engine` | Jetson GPU | `jetson-native` |
| ONNX Runtime | `models/*.onnx` | Any CPU | `sim` |

A `.engine` file is a serialised plan for **one** GPU architecture and **one** TensorRT version. It cannot be built on a workstation - `build.sh --engines` builds it on the Jetson from the committed `.onnx`. The sim uses ONNX Runtime because the container has no TensorRT and no CUDA.

## Prerequisites

CogiDrone is not a Python project. There are three ways to run it, and they need
different things.

**Simulation only** - any OS with Docker:

- **Docker** - [Docker Desktop](https://www.docker.com/products/docker-desktop) (Windows/macOS) or Docker Engine (Linux). 16 GB RAM, 30 GB free disk.
- **Git**

**Onboard** - the airframe:

- **Jetson Orin** running JetPack 7.2 (Ubuntu 24.04, L4T 38, CUDA 13.5, TensorRT 10.13)
- **Pixhawk 6C** with PX4 v1.17.0
- **RealSense D435i** (or any camera publishing `sensor_msgs/Image` + `CameraInfo`)
- **Micro XRCE-DDS Agent** on the Jetson

**Cross-compile only** - a syntax check on your laptop:

- **Arm GNU Toolchain 15.3** (`aarch64-none-linux-gnu`)
- **CMake 3.20+** and **Ninja**

**Python** is used for two things only: the CV prototypes the C++ was derived from, and the training pipeline. If you are not touching either, you do not need it.

- **Python 3.12** - [Download Python](https://www.python.org/downloads/)
- **UV** - [Install UV](https://docs.astral.sh/uv/getting-started/installation/)

## Getting started

### Simulation (any OS)

Everything runs in one Docker image, so the whole team gets the same setup.

```bash
./scripts/sim.sh build   # first time: 20-40 min (compiles PX4). Later: seconds.
./scripts/sim.sh up
./scripts/sim.sh shell   # opens a shell in the container
```

Inside the container:

```bash
cb                       # colcon build --symlink-install in ros2_ws, then source
cmake --preset sim
cmake --build build/sim
ros2 launch cogidrone_bringup sim.launch.py
```

Gazebo opens and the drone spawns. Options:

```bash
ros2 launch cogidrone_bringup sim.launch.py headless:=true    # no Gazebo window
ros2 launch cogidrone_bringup sim.launch.py autonomy:=false   # sim + bridge only
```

Full details, including macOS record-and-replay, are in `sim.md`.

### Jetson (onboard)

```bash
./build.sh              # builds build/jetson/src/cogidrone
./build.sh --engines    # also builds models/*.engine from models/*.onnx
```

`build.sh` preflights for aarch64, CMake, Ninja, `libnvinfer`, and OpenCV before compiling. It refuses to run anywhere but the Jetson, because TensorRT and CUDA only exist there.

Then follow `ON-JETSON-TODO.md` §5 for bring-up order: agent $\rightarrow$ camera $\rightarrow$ verify topics $\rightarrow$ `cogidrone`.

### Cross-compile (syntax check)

This compiles every translation unit for aarch64 but **cannot link** - the vendored OpenCV, TensorRT, and CUDA directories are headers-only by design.

```powershell
# Windows
copy toolchain.windows.config.example toolchain.windows.config
# edit ARM_GCC_ROOT to point at your Arm GNU Toolchain
.\cross-build.bat
```

```bash
# Linux / macOS
./cross-build.sh
```

Expect all translation units to compile and the link to fail on `-lnvinfer`, `-lcudart`, `-lopencv_core`. That is the intended outcome.

## Usage

On the Jetson, with the XRCE agent running and PX4 connected:

```bash
MicroXRCEAgent udp4 -p 8888 &
./build/jetson/src/cogidrone
```

The drone will not move until you arm it and switch to offboard mode from your RC transmitter. Until then `ControlState` is `WAITING_FOR_ARM` and Navigation publishes nothing.

Useful while debugging:

```bash
ros2 topic list
ros2 topic echo /fmu/out/vehicle_status_v1 --once
ros2 topic echo /perception/detections --once
ros2 run rqt_image_view rqt_image_view /camera/camera/color/image_raw
```

## Repository layout

```text
CogiDrone/
├── src/                            # The C++ autonomy stack (the actual product)
│   ├── drone.{h,cpp}               #   Orchestrator: owns the three subsystems
│   ├── perception/                 #   Camera -> detections
│   │   └── models/                 #     TrtEngine, OnnxEngine, Yolo, Person, Head
│   ├── navigation/                 #   Detections -> setpoints
│   ├── controller/                 #   Setpoints -> PX4 offboard
│   ├── representation/             #   Obstacle map (stub, issue #18)
│   ├── math/                       #   Vec3, Quat, Transform, BoundingBox
│   └── ON-JETSON-TODO.md           #   Everything to do before first flight
├── sim/                            # Gazebo world, drone model, props
│   ├── worlds/                     #   cogidrone.sdf + Bymarka variants
│   ├── models/                     #   cogidrone_x500
│   └── tools/                      #   Asset generators
├── ros2_ws/src/cogidrone_bringup/  # sim.launch.py + Gazebo->ROS bridge config
├── docker/sim/                     # The container image (ROS 2 + PX4 + Gazebo + ONNX)
├── models/                         # best.onnx, medium.onnx (+ .pt sources, export.sh)
├── python/                         # CV prototypes and training (not the product)
├── vendored/                       # Headers for IntelliSense; .so files for cross-link
├── config/                         # camera.yaml, px4.params
├── docs/                           # sim.md, manuals/, images/
├── build.sh                        # Jetson native build
├── cross-build.{bat,sh}            # Workstation cross-compile (syntax check)
├── CMakePresets.json               # linux-arm64, jetson-native, sim
└── compose.yaml                    # sim + dev services
```

## Testing

The C++ stack has no unit tests yet. What exists:

```bash
# Python prototypes and training
uv run pytest --doctest-modules --cov=src --cov-report=html
```

CI runs the Python tests and deploys the docs site. It does **not** build the C++ - there is no Jetson runner.

The real verification is `build.sh` on the Jetson followed by `ON-JETSON-TODO.md` §5.

### Generate Documentation Site

To build and preview the documentation site locally:
```bash
uv run mkdocs build
uv run mkdocs serve
```

This will build the documentation and start a local server at [http://127.0.0.1:8000/](http://127.0.0.1:8000/) where you can browse the docs and API reference. Get the documentation according to the lastes commit on main by viewing the `gh-pages` branch on GitHub: [https://cogitontnu.github.io/CogiDrone/](https://cogitontnu.github.io/CogiDrone/).

## Team

This project would not have been possible without the hard work and dedication of all of the contributors. Thank you for the time and effort you have put into making this project a reality.

<table align="center">
    <tr>
        <!--
        <td align="center">
            <a href="https://github.com/NAME_OF_MEMBER">
              <img src="https://github.com/NAME_OF_MEMBER.png?size=100" width="100px;" alt="NAME OF MEMBER"/><br />
              <sub><b>NAME OF MEMBER</b></sub>
            </a>
        </td>
        -->
    </tr>
</table>

![Group picture](docs/images/team.png)

### License

______________________________________________________________________

Distributed under the MIT License. See `LICENSE` for more information.
