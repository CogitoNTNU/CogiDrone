You should treat this as **three separate systems**:

```text
Ground station / RC controller
             |
       telemetry link
             |
Pixhawk 6C running PX4
  - stabilization
  - motor mixing
  - GPS, failsafes, battery
             |
       MAVLink over USB/UART
             |
Jetson Orin Nano running Linux
  - YOLO
  - RealSense
  - SLAM/VIO
  - high-level navigation
```

The Jetson should not directly control the four motors. The Pixhawk must remain capable of keeping the drone stable and executing failsafes if the Jetson crashes.

## Recommended software stack

I would choose:

- **PX4** on the Pixhawk 6C
- **QGroundControl** for configuration, calibration, missions, and flight monitoring
- **ROS 2** on the Jetson for camera, SLAM, transforms, and perception
- **MAVSDK C++** initially for simple Jetson-to-Pixhawk commands
- **MAVLink** as the communication protocol
- **YOLO with TensorRT** for Jetson inference
- **RealSense ROS 2 wrapper** for D435i color, depth, and IMU data

PX4 officially supports Linux companion computers communicating through MAVLink or micro-ROS/uXRCE-DDS. MAVSDK provides a cross-platform C++ API for telemetry, arming, takeoff, landing, missions, and offboard control.

Useful documentation:

- [PX4 companion computers](https://docs.px4.io/main/en/companion_computer/)
- [PX4 ROS 2 integration](https://docs.px4.io/main/en/ros/ros2_comm.html)
- [MAVSDK C++](https://mavsdk.mavlink.io/main/en/cpp/)
- [MAVLink](https://mavlink.io/en/)
- [RealSense ROS 2 wrapper](https://github.com/realsenseai/realsense-ros)

## What is the airframe API?

The **Holybro X500 V2 frame has no software API**. It is mechanical hardware.

The software interfaces are:

| Hardware | Interface |
|---|---|
| Motors | Electrical connection to ESCs |
| ESCs | PWM, DShot, or another actuator protocol from Pixhawk |
| Pixhawk | PX4 or ArduPilot firmware |
| Jetson to Pixhawk | MAVLink over UART or USB |
| GPS | Connected to Pixhawk |
| Telemetry radio | Usually connected to Pixhawk |
| RealSense | USB to Jetson |
| YOLO | C++/TensorRT application on Jetson |

In other words, the “API for the drone” is primarily the **PX4 MAVLink interface**. MAVSDK wraps much of that interface in a friendlier C++ API.

Example high-level operations are:

```cpp
system->action().arm();
system->action().takeoff();
system->offboard().set_velocity_ned(...);
system->action().land();
```

You should not initially send raw motor commands. Use PX4 actions, missions, position setpoints, or velocity setpoints.

## How to build from Windows and macOS

You do not need to develop directly on the drone at first.

### Development machines

Use one of these:

- **Windows 11:** WSL2 with Ubuntu, or Docker Desktop
- **macOS:** Docker or a Linux virtual machine
- **Jetson:** native Ubuntu/JetPack environment for final deployment

For normal C++ code, your Windows/macOS build is useful. For Jetson-specific CUDA and TensorRT code, compile and test on the Jetson or inside an ARM64-compatible container.

Do not assume an x86 Docker image will run on the Jetson. The Jetson uses ARM64.

### The current repository

At the moment this is still a template:

- `main.cpp` is empty
- `build.sh` is empty
- `build.bat` is empty
- `README.md` describes training code
- There is no CMake project yet

The C++ portion should become a normal CMake project. The Python directory should contain training, dataset preparation, and model-export tooling. The deployed drone application should be C++, probably under `src`.

A reasonable eventual structure is:

```text
src/
  flight/
    mavsdk_client.cpp
  perception/
    camera_node.cpp
    yolo_detector.cpp
  navigation/
    mission_controller.cpp
  representation/
    coordinates.cpp
  main.cpp

python/
  training/
  export/
  evaluation/

config/
  px4.params
  camera.yaml
  detector.yaml

simulation/
  ros2/
  px4/
tests/
```

## How to drive the drone

Use a staged control model:

1. **Manual flight**
   - RC transmitter controls the drone.
   - Pixhawk handles stabilization.
   - Jetson is disconnected.

2. **Telemetry only**
   - Jetson reads GPS, attitude, battery, mode, and health.
   - No control commands are sent.

3. **Commanded actions**
   - Jetson requests arm, takeoff, land, and return-to-home.
   - A human still controls the flight mode and emergency override.

4. **Offboard velocity or position control**
   - Jetson sends setpoints at a reliable rate.
   - PX4 has a timeout and exits offboard mode if setpoints stop.

5. **Autonomous perception**
   - YOLO detects objects.
   - SLAM/VIO estimates local position.
   - A navigation component decides where to move.
   - PX4 executes the resulting setpoints.

The RC transmitter, physical kill switch, GPS failsafe, low-battery action, geofence, and return-to-home behavior should exist before autonomous flight.

## ROS 2 versus MAVSDK

Use both, but give them different jobs:

- **ROS 2:** camera streams, IMU, TF frames, SLAM, YOLO, mapping
- **MAVSDK:** connection, telemetry, arming, flight actions, missions, initial offboard experiments
- **PX4 ROS 2 bridge:** use later when you need direct PX4 topics and tighter integration

Avoid having multiple independent components simultaneously command the vehicle. Create one `flight_manager` component that owns flight commands and enforces states such as:

```text
DISCONNECTED
CONNECTED
NOT_READY
READY
ARMED
TAKEOFF
AUTONOMOUS
LANDING
FAILSAFE
```

## SLAM and YOLO are different

YOLO answers:

> “What objects are visible?”

SLAM or visual-inertial odometry answers:

> “Where is the drone relative to its environment?”

The RealSense D435i can provide color, depth, and IMU data. A ROS 2 RealSense driver publishes these as topics and provides camera coordinate transforms. You will still need to carefully calibrate:

- camera-to-body transform
- camera orientation
- timestamp synchronization
- coordinate conventions
- vibration isolation
- depth reliability outdoors

Do not begin by writing SLAM yourselves. Use an established ROS 2 package or NVIDIA Isaac ROS component first, then replace components only if necessary.

## Hardware issues to resolve early

Before software flight tests, verify:

- The X500 kit includes suitable motors, ESCs, and propellers.
- Motor/propeller combination is compatible with 4S LiPo.
- The battery has enough discharge current.
- The Jetson has a dedicated regulated power supply, not an improvised connection to the battery.
- The RealSense receives stable USB power.
- Pixhawk and Jetson share a correctly wired UART or USB connection.
- GPS and telemetry antennas are physically separated from noisy electronics.
- Total mass and center of gravity remain acceptable.
- The Jetson does not brown out during throttle changes.

A 4S 5200 mAh battery contains approximately:

$$
14.8\ \text{V} \times 5.2\ \text{Ah} \approx 77\ \text{Wh}
$$

Actual flight time depends heavily on total mass, motor/propeller efficiency, and current draw. Measure it rather than estimating from battery capacity alone.

## Recommended first milestone

Do not start with YOLO. Start with this:

1. Install PX4 on the Pixhawk.
2. Configure the X500 frame in QGroundControl.
3. Calibrate sensors and verify motor order with propellers removed.
4. Fly manually in a controlled area.
5. Run PX4 SITL simulation on a developer computer.
6. Write a tiny C++ MAVSDK program that connects and prints telemetry.
7. Test arm/takeoff/land in simulation.
8. Deploy the same program to the Jetson.
9. Add RealSense streaming.
10. Add YOLO inference.
11. Add SLAM/VIO.
12. Only then enable autonomous movement.

The most important architectural rule is:

> PX4 is responsible for flight safety and stabilization. The Jetson proposes high-level movement based on perception.

That division lets you develop most of the system safely on Windows/macOS and postpone Linux-specific hardware work until the interfaces are understood.