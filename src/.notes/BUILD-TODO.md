# BUILD-TODO - Option 1: native Jetson build with ament + px4_msgs

> Goal: get `px4_msgs` message types compiling into `cogidrone` so we can
> command the Pixhawk 6C via uXRCE-DDS (offboard hover).
> Windows cross-build stays as an error-checker only (guarded by `if(NOT MSVC)`).

## 1. Install build tooling on the Jetson

```bash
sudo apt update
sudo apt install ros-jazzy-ament-cmake python3-colcon-common-extensions
```

## 2. Build `px4_msgs` in a separate workspace

```bash
mkdir -p ~/px4_ws/src && cd ~/px4_ws/src
git clone https://github.com/PX4/px4_msgs.git -b release/1.17   # ! branch MUST match PX4 firmware version on the Pixhawk
cd ~/px4_ws
colcon build --packages-select px4_msgs                          # runs rosidl codegen (takes a few min)
```

Verify: `ls ~/px4_ws/install/px4_msgs/include/px4_msgs/px4_msgs/msg/` shows generated headers (`trajectory_setpoint.hpp` etc.).

## 3. Environment sourcing (every build shell, or add to ~/.bashrc)

```bash
source ~/px4_ws/install/setup.bash
# (only needed if base ROS is installed on the Jetson:)
# source /opt/ros/jazzy/setup.bash
```

## 4. Build natively on the Jetson

```bash
cd ~/CogiDrone
source ~/px4_ws/install/setup.bash
cmake -B build && cmake --build build -j$(nproc)
```

## 5. uXRCE-DDS agent on the Jetson

```bash
sudo apt install ros-jazzy-uxrce-dds-agent
MicroXRCEAgent udp4 -p 8888        # keep running; or make a systemd unit later
```

## 6. Pixhawk 6C config (QGroundControl → Parameters, then reboot FC)

| Parameter        | Value                          | Note                          |
|------------------|--------------------------------|-------------------------------|
| `UXRCE_DDS_CFG`  | `TELEM2` (flight) / USB (bench)| enables DDS client            |
| `COM_RCL_EXCEPT` | include `Offboard`             | bench testing ONLY            |
| `COM_OBL_RC_ACT` | `Hold`                         | failsafe if setpoint stream dies |

Verify bridge: with agent running + Pixhawk connected → `ros2 topic list` shows `/fmu/out/vehicle_odometry`, `/fmu/in/trajectory_setpoint`, ...

## 7. First flight test sequence (do not skip)

1. ☐ Props OFF: run `cogidrone`, watch mode/arm commands in QGC MAVLink console
2. ☐ (Optional) PX4 SITL on any PC: `make px4_sitl gz_x500` + agent on `udp4 -p 8888`
3. ☐ Outdoors, GPS lock, hover at 0.5 m
4. ☐ Then 0.1 m (expect ground-effect sloppiness)

## Notes / gotchas

- `px4_msgs` timestamps are microseconds on PX4's clock; if setpoints are
  rejected as stale, check agent timesync logs first.
- NED frame: `z = -0.1` means 10 cm ABOVE ground.
- Offboard requires streaming `OffboardControlMode` + `TrajectorySetpoint`
  at ≥2 Hz BEFORE switching mode / arming.
