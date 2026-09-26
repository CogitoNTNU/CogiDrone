### Link

The channel to the flight controller. Telemetry in, setpoints out.

## Why this is not a pipeline stage

Every other directory under `src/` is a stage of the pipeline in `src/DRONE.md`:
`Perception → Estimation → Representation → Navigation → Control → Act`. This one
is not. It is an **adapter**: infrastructure the pipeline talks *through*, at both
ends — telemetry enters here on its way to Estimation, and setpoints leave here as
Act's actual output.

Putting it in `act/` was the alternative, and it would have hidden the telemetry
half. If the team prefers that anyway, moving it is three lines in
`src/CMakeLists.txt` and an include path.

## The three files

| File | Role |
|---|---|
| `vehicle_link.h` | the port: `IVehicleLink` + plain structs. **No ROS.** |
| `px4_dds_link.{h,cpp}` | the real adapter: `rclcpp` + `px4_msgs` over uXRCE-DDS |
| `fake_link.{h,cpp}` | an in-memory implementation that talks to nothing |

`vehicle_link.h` must never include `rclcpp` or `px4_msgs`, and neither must
anything it pulls in. That constraint is the whole design:

- `third-party/ros2` holds **arm64-only** `.so` files. Anything touching `rclcpp`
  can only run on the Jetson or in an arm64 container — never on a dev machine or
  an x86 CI runner. Code written against this port can.
- It confines the `px4_msgs` version pin (see `third-party/ros2/PX4_MSGS.md`) to one
  translation layer instead of spreading PX4 message types through the flight logic.
- `src/C++ STANDARD.md` argues for exactly this split: *"Keep C++23 only for code
  that never touches ROS 2."*

So `px4_dds_link.cpp` is the only file in the project that may mention `px4_msgs`.

## Two things that fail silently

**Frames.** PX4 is NED/FRD; ROS 2 is ENU/FLU. Every field in `vehicle_link.h` names
its frame, there is deliberately no generic `Vector3`, and conversion happens only
inside the adapter. An untyped triple is how a frame gets lost.

**The offboard heartbeat.** PX4 needs `OffboardControlMode` at **>2 Hz**, both before
entering offboard mode and continuously while in it, or it leaves the mode —
mid-flight, with no error. `IVehicleLink::tick()` is that heartbeat, exposed rather
than hidden in a timer inside the adapter, because the obligation belongs to whoever
owns the link and a fake can then record whether it was honoured.

> [!NOTE]
> Nothing calls `tick()` yet. That belongs to the `flight_manager` component
> described in `src/NOTES.md` — the one thing that owns flight commands. Until it
> exists the link can read telemetry but cannot fly the vehicle.

## The fake

`FakeLink` is the seam for tests, but **no tests use it yet** — the test framework is
still an open decision. It is deliberately dumb: no threads, no timers, no DDS. Push
telemetry in with `push_odometry()` / `set_state()`, inspect what the code under test
sent with `sent_setpoints()`, `commands()` and `tick_count()`.

Inject it through the second `Drone::initiate` overload:

```cpp
auto drone = Drone::initiate(std::make_unique<FakeLink>());
```

`Drone::M` is private, so that overload is the only available seam. It does not call
`rclcpp::init()` — a fake-injected `Drone` has to work in a process with no ROS at
all, or the fake buys nothing.

> [!WARNING]
> `CMakePresets.json` currently has only the `linux-arm64` cross preset, and
> `src/CMakeLists.txt` links ROS unconditionally. So although `FakeLink` needs no
> ROS, there is as yet no way to *compile* it without the cross toolchain. A host
> preset is the missing half, and belongs with the test-framework decision.

Also note `tick_count()` counts calls, not rate. Asserting the >2 Hz contract needs
an injectable clock, which does not exist yet — a nonzero count is not compliance.
