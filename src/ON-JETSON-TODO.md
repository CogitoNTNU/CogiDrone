# ON-JETSON-TODO

Everything that must happen **on the Jetson itself**. Nothing here can be done on a
dev laptop, because TensorRT, CUDA, and the arm64 GPU are only present there.

> **Scope rule.** `vendored/tensorrt`, `vendored/cuda`, `vendored/opencv` are
> **IntelliSense-only**. They must never be used to compile or link on the Jetson.
> `src/CMakeLists.txt` enforces this with `if(CMAKE_CROSSCOMPILING)` - the Jetson
> branch points at `/usr/include/aarch64-linux-gnu`, `/usr/local/cuda/include`,
> `/usr/include/opencv4`.
>
> **Exception:** `vendored/ros2` and `vendored/px4_msgs` are used on **both**
> platforms. They are arm64 debs, so they are valid on the Jetson. See §6.

---

## 0. Target environment (verified)

| | Value |
|---|---|
| Board | Jetson Orin, JetPack 7.2 |
| OS | Ubuntu 24.04, L4T 38 |
| Arch | `aarch64` |
| CUDA | 13.x |
| TensorRT | 10.x |
| GCC | 15 |
| Flight controller | Pixhawk 6C, PX4, uXRCE-DDS over UDP 8888 |
| Camera | Intel RealSense D435i, USB 3.0 |

---

## 1. One-time setup

### 1.1 Build tools

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config
```

`build.sh` preflights for `cmake` and `ninja` and fails with a readable message if
either is missing.

### 1.2 Verify JetPack components are actually present

```bash
# TensorRT runtime
ldconfig -p | grep libnvinfer.so

# trtexec (not always on PATH)
command -v trtexec || ls -l /usr/src/tensorrt/bin/trtexec

# CUDA
nvcc --version
ls /usr/local/cuda/lib64/libcudart.so*

# OpenCV
pkg-config --modversion opencv4
ls -d /usr/include/opencv4/opencv2
```

`build.sh` checks the first four. If `libnvinfer.so` is missing, JetPack is broken -
reflash rather than trying to patch it.

### 1.3 ROS 2 Jazzy runtime

The vendored ROS 2 tree is **headers + `.so` for IntelliSense**. It is not a
substitute for a working ROS 2 install on the target.

```bash
sudo apt install -y ros-jazzy-ros-base
```

Then confirm the ABI matches what `vendored/ros2` was built against:

```bash
ls /opt/ros/jazzy/lib/librclcpp.so*
```

If the SONAME differs from the vendored one, the binary will link but fail to load
at runtime. See §6.

### 1.4 RealSense driver

Do **not** link `librealsense` into `cogidrone`. Run Intel's node as a separate
process - it publishes plain `sensor_msgs/msg/Image`, which `Perception` already
subscribes to.

```bash
sudo apt install -y ros-jazzy-realsense2-camera ros-jazzy-librealsense2 \
                    ros-jazzy-cv-bridge ros-jazzy-image-transport
```

### 1.5 Micro XRCE-DDS Agent

This is the bridge between PX4's uORB and ROS 2. Without it, `/fmu/in/*` and
`/fmu/out/*` do not exist and `FlightController` never leaves `STARTING`.

```bash
# Option A: prebuilt
sudo apt install -y ros-jazzy-micro-ros-agent   # if available for Jazzy

# Option B: build from source (v2.4.3 is what the sim image pins)
git clone --branch v2.4.3 --recurse-submodules https://github.com/eProsima/Micro-XRCE-DDS-Agent.git
cd Micro-XRCE-DDS-Agent && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j"$(nproc)" && sudo make install
sudo ldconfig /usr/local/lib/
```

---

## 2. Model pipeline (`.pt` $\rightarrow$ `.onnx` $\rightarrow$ `.engine`)

An `.engine` is a serialized plan for **one GPU architecture and one TensorRT
version**. It cannot be built on a laptop and shipped. `models/*.engine` is
gitignored for exactly this reason.

### 2.1 Export ONNX - on a workstation, NOT the Jetson

```bash
yolo export model=models/best.pt   format=onnx imgsz=640 nms=False
yolo export model=models/medium.pt format=onnx imgsz=640 nms=False
```

`nms=False` is mandatory. With `nms=True` the graph embeds the `EfficientNMS_TRT`
plugin, which needs `libnvinfer_plugin.so` at runtime and makes the engine
non-portable. NMS runs on the CPU instead (~1 ms for 8400 anchors).

**Commit the `.onnx` files.** They are portable. Do not commit the `.engine`.

### 2.2 Verify the output layout - before trusting the decode code

```bash
python -c "import onnx; m = onnx.load('models/best.onnx'); \
print([d.dim_value for d in m.graph.output[0].type.tensor_type.shape.dim])"
```

Expected: `[1, 10, 8400]` for 6 classes (`4 + 6 = 10` rows, 8400 anchors).

If it prints `[1, 8400, 10]`, the output is transposed and `Yolo::detect()` must be
adjusted - the decode assumes `[1, rows, anchors]`.

### 2.3 Build the engines - on the Jetson

```bash
./build.sh --engines
```

This runs `trtexec --fp16 --memPoolSize=workspace:2048MiB` for both models.
`--memPoolSize` replaces the deprecated `--workspace` in TensorRT 10+.

Expected output: `models/best.engine`, `models/medium.engine`.

### 2.4 Tensor names must match

`Yolo::create()` hardcodes `"images"` and `"output0"` - the names ultralytics
emits. If the export used different names, `TrtEngine::create()` logs the ones it
actually found. Check the log on first run.

---

## 3. Build

```bash
./build.sh              # configure + build
./build.sh --clean      # wipe build/jetson first
./build.sh -j4          # limit parallelism (thermal throttling)
```

Under the hood: `cmake --preset jetson-native` $\rightarrow$ Ninja $\rightarrow$ `build/jetson/src/cogidrone`.

`build.sh` fails fast on: not-aarch64, missing cmake/ninja, missing `libnvinfer.so`,
missing `.onnx` when `--engines` was requested.

---

## 4. PX4 configuration (Pixhawk 6C)

`config/px4.params` is currently **empty**. These are the parameters that must be
set, via QGroundControl $\rightarrow$ *Vehicle Setup $\rightarrow$ Parameters*:

| Parameter | Value | Why |
|---|---|---|
| `XRCE_DDS_CFG` | `2` (Ethernet/UDP) | Enables the uXRCE-DDS client |
| `UXRCE_DDS_PRT` | `8888` | Must match the agent's `-p` |
| `UXRCE_DDS_AG_IP` | Jetson's IP, encoded | Agent address |
| `UXRCE_DDS_SYNCT` | `1` | Timestamp sync - required for `TrajectorySetpoint` |
| `COM_OF_LOSS_T` | `1.0` | Offboard loss timeout |
| `COM_OBL_RC_ACT` | `0` (Position) | What PX4 does when offboard drops |
| `NAV_RCL_ACT` | `2` (Loiter) | RC-loss failsafe |
| `COM_RC_IN_MODE` | per pilot preference | Whether RC is required |

**Offboard handshake contract** (implemented in `FlightController`):
1. Stream `OffboardControlMode` at ≥ 2 Hz for 1 s before requesting the mode.
2. Then send `VehicleCommand` `MAV_CMD_DO_SET_MODE` with `PX4_CUSTOM_MAIN_MODE_OFFBOARD`.
3. Keep streaming, or PX4 drops out of offboard after `COM_OF_LOSS_T`.

PX4's `commander` arbitrates the request. The RC switch is an **event source**, not
a continuous veto - but a switch in a non-offboard position will win the next
arbitration.

---

## 5. Bring-up order (every session)

Order matters. Each step depends on the previous one.

```bash
# 1. Agent - must be up before cogidrone, or /fmu/* never appears
MicroXRCEAgent udp4 -p 8888

# 2. Camera - must be up before cogidrone, or CameraInfo never arrives
#    and Perception cannot compute distance
ros2 launch realsense2_camera rs_launch.py \
    enable_color:=true enable_depth:=true enable_sync:=true \
    align_depth.enable:=true \
    depth_module.profile:=640x480x30 rgb_camera.profile:=640x480x30 \
    color_qos:=SENSOR_DATA depth_qos:=SENSOR_DATA

# 3. Verify topics exist BEFORE starting cogidrone
ros2 topic list | grep -E 'fmu|camera'
ros2 topic hz /camera/camera/color/image_raw
ros2 topic echo /fmu/out/vehicle_status --once

# 4. cogidrone - MUST run from the repo root (model paths are relative)
cd ~/CogiDrone
./build/jetson/src/cogidrone
```

**Why the repo root matters:** `Person::create()` and `Head::create()` use
`"models/best.engine"` and `"models/medium.engine"` - relative paths. Running from
`build/jetson/` fails with `EngineLoadFailed`.

**USB 3.0 is mandatory.** `lsusb -t` must show `5000M` for the D435i. On USB 2 it
silently drops resolution or fails to enumerate. Use the Jetson's own ports, not a
hub.

---

## 6. Known gaps to resolve on the Jetson

### 6.1 ROS 2 library resolution at runtime

`src/CMakeLists.txt` takes `_ros_lib_dirs` and `_px4_lib_dir` from `vendored/`
**unconditionally** - there is no `CMAKE_CROSSCOMPILING` branch for them. So the
Jetson binary links against `vendored/ros2/opt/ros/jazzy/lib/*.so`.

The `-Wl,-rpath-link` block is cross-only, so on the Jetson the linker finds them
via `-L` from `target_link_directories`. At runtime there is no rpath, so:

```bash
# Either source the system ROS 2 (if SONAMEs match)
source /opt/ros/jazzy/setup.bash

# Or point at the vendored libs
export LD_LIBRARY_PATH="$PWD/vendored/ros2/opt/ros/jazzy/lib:$PWD/vendored/ros2/usr/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH"
```

**Verify which one actually works** and record the answer here. If the SONAMEs
differ, the binary will fail with `error while loading shared libraries`.

### 6.2 OpenCV version skew - the dangerous direction

Vendored headers are **OpenCV 5.0.0** (from MSYS2). JetPack ships **OpenCV 4.x**.

Vendored headers *newer* than the target is the unsafe direction: code that
compiles on the laptop can fail on the Jetson, and IntelliSense will suggest APIs
that do not exist there.

```bash
pkg-config --modversion opencv4
```

If this is 4.x, either re-vendor OpenCV 4.x headers, or accept that the cross-build
is only a syntax check for OpenCV-touching code.

### 6.3 CUDA header version match

Vendored headers are `CUDART_VERSION 13050` (13.5).

```bash
grep CUDART_VERSION /usr/local/cuda/include/cuda_runtime_api.h
```

If it prints below `13050`, re-vendor from
`https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/sbsa/`
using the matching `cuda-cudart-dev-13-X` **and** `cuda-crt-13-X` packages.
Both are needed - CUDA 12.x/13.x moved implementations into `crt/`.

### 6.4 TensorRT header version match

Vendored headers are **10.13**.

```bash
dpkg -l | grep -i nvinfer
```

### 6.5 Collect `.so` files for the dev laptops (optional)

Only needed if the team wants the cross-build to produce a **linked binary** rather
than stopping at compile. Run on the Jetson:

```bash
mkdir -p /tmp/vendored-libs/{tensorrt,cuda,opencv}
cp -a /usr/lib/aarch64-linux-gnu/libnvinfer.so*        /tmp/vendored-libs/tensorrt/
cp -a /usr/local/cuda/lib64/libcudart.so*              /tmp/vendored-libs/cuda/
cp -a /usr/lib/aarch64-linux-gnu/libopencv_core.so*    /tmp/vendored-libs/opencv/
cp -a /usr/lib/aarch64-linux-gnu/libopencv_imgproc.so* /tmp/vendored-libs/opencv/
tar -czf ~/vendored-libs.tar.gz -C /tmp/vendored-libs .
```

Then on Windows: `scp jetson:~/vendored-libs.tar.gz .` and
`tar -xzf vendored-libs.tar.gz -C vendored`.

TensorRT for arm64 is **not** in NVIDIA's CUDA repo - JetPack only. JetPack's
OpenCV is a custom CUDA-enabled build, not Ubuntu's `libopencv-core406t64`.

---

## 7. Pending code fixes (do these before the first flight)

- [ ] `-Wmissing-field-initializers` - 5 warnings, all in `M` struct initializers.
      Designated initializers must be in **declaration order**:
      - `trt_engine.cpp:121` - `.hostInput = {}`, `.hostOutput = {}` go **between**
        `.deviceOutput` and `.inputName`
      - `yolo.cpp:166` - `.candidates = {}`, `.suppressedScratch = {}` go at the end
      - `navigation.cpp:57` - `.localPositionSubscription = nullptr` goes **between**
        `.armedSubscription` and `.setpointPublisher`
- [ ] `build.sh` - the OpenCV check is stale. Replace
      `warn "opencv4 pkg-config not found..."` with
      `[[ -d /usr/include/opencv4/opencv2 ]] || fail "OpenCV headers not found at /usr/include/opencv4"`
- [ ] `config/px4.params` - empty. Fill in from §4.
- [ ] `config/camera.yaml`, `config/detector.yaml` - empty.
- [ ] `src/.notes/C++ STANDARD.md` - says Humble/GCC 11. Actual is Jazzy/GCC 15.
- [ ] `vendored/cuda/bin/`, `vendored/cuda/lib/` - Windows x64 leftovers from a
      failed pip attempt. Delete.

---

## 8. First-flight checklist

- [ ] `MicroXRCEAgent udp4 -p 8888` running
- [ ] `ros2 topic list` shows `/fmu/out/vehicle_status`
- [ ] `ros2 topic hz /camera/camera/color/image_raw` ≈ 30 Hz
- [ ] `ros2 topic echo /camera/camera/color/camera_info --once` shows non-zero `K`
- [ ] `models/best.engine` and `models/medium.engine` exist
- [ ] `./build.sh` completes
- [ ] `./build/jetson/src/cogidrone` starts from the repo root
- [ ] Log shows `Perception`, `Navigation`, `FlightController` all created
- [ ] `/pixhawk/armed` and `/pixhawk/offboard_active` publish `false` (transient_local)
- [ ] **Props off.** Verify the offboard handshake reaches `CONTROL_GRANTED` before
      mounting props.