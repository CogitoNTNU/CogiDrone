# `px4_msgs`

The uORB message types PX4 exposes over uXRCE-DDS — `/fmu/out/*` for telemetry,
`/fmu/in/*` for setpoints and commands. Everything else in this tree comes from
`get.sh`; `px4_msgs` cannot, so it has its own script.

## Why it is not in `get.sh`

`get.sh` downloads prebuilt arm64 `.deb`s from `packages.ros.org`. **There is no
`ros-jazzy-px4-msgs` deb** — `px4_msgs` is never bloom-released into a ROS
distro. It ships as `.msg` source that must go through rosidl codegen, which
needs `ament_cmake` and a colcon build that this project deliberately does not
have.

So the codegen happens once, in a throwaway container, and only the resulting
artifacts are vendored:

```sh
./third-party/ros2/get-px4-msgs.sh
```

It builds `docker/px4-msgs.Dockerfile` for `linux/arm64` (native on Apple
Silicon, emulated elsewhere), then copies the install tree into
`opt/ros/jazzy/{include,lib,share}/` so it sits exactly where the deb-extracted
packages do. `src/CMakeLists.txt` picks the headers up with no change at all —
its `file(GLOB _ros_include_dirs ... include/*)` treats each package dir as its
own include root.

`libpx4_msgs__rosidl_generator_py.so` is dropped: Python bindings are dead
weight for a C++-only target, and `px4_msgs` is ~450 messages.

## Version pin

`PX4_MSGS_REF`, defaulting to `release/1.15` in both
`docker/px4-msgs.Dockerfile` (an `ARG`) and `get-px4-msgs.sh`. Override without
editing either:

```sh
PX4_MSGS_REF=release/1.16 ./third-party/ros2/get-px4-msgs.sh
```

> [!IMPORTANT]
> **`release/1.15` is a placeholder and has not been confirmed against the
> airframe.** The message set must match the uORB definitions in the PX4 firmware
> actually flashed. A mismatch does not produce an error — topics silently never
> connect, or fields shift. Confirm the flashed PX4 version and re-run before the
> first flight test.

## Deployment to the Jetson

`src/CMakeLists.txt` passes `-Wl,-rpath-link,...`, which is link-time only. But
CMake *also* bakes a build-tree `RUNPATH` of its own from `target_link_directories`,
and it points at the build container's path:

```
RUNPATH   /workspace/third-party/ros2/opt/ros/jazzy/lib
```

That directory does not exist on the Jetson, so in practice it resolves to nothing
and the loader falls through — but do not rely on it, and do not read it as the
libraries being found. `DT_RUNPATH` is searched *after* `LD_LIBRARY_PATH` (unlike the
legacy `DT_RPATH`), so an explicit `LD_LIBRARY_PATH` still wins.

The Jetson therefore resolves ROS libraries from its own `/opt/ros/jazzy`, and
`px4_msgs` must be installed there too (or on `LD_LIBRARY_PATH` as a sourced
overlay).

> [!NOTE]
> Leaking an absolute build-container path into a flight binary is untidy and
> worth fixing with `CMAKE_SKIP_BUILD_RPATH` (or an explicit install RPATH). Left
> alone for now because it changes link behaviour for every target, not just
> `px4_msgs`.

That install must include the `fastrtps` and `introspection` typesupport `.so`s,
even though `src/CMakeLists.txt` does not link them: `rmw_fastrtps_cpp`
`dlopen`s them **by name** at runtime. Linking only
`px4_msgs__rosidl_typesupport_{c,cpp}` is correct at build time and insufficient
at run time.

The artifacts this script produces are genuine arm64 binaries, so the exported
tree can be copied to the Jetson directly — no rebuild on target.

## Smoke test

`src/test.cpp` carries a compiled-but-never-instantiated `TestNode` that
subscribes to `/fmu/out/vehicle_status` and publishes
`/fmu/in/offboard_control_mode`. It exists to keep both directions of the PX4
link inside the build, so a broken vendored tree fails at compile time rather
than in the air.

Its constructor is defined **out of line** on purpose. A ctor defined in the
class body is implicitly `inline`; with nothing instantiating `TestNode` it is
never odr-used, the compiler emits no code, and `-Wl,--as-needed` then drops
every message `.so` — the file would prove only that the headers parse. Out of
line the ctor is emitted unconditionally, so the link line is actually
exercised. Verify with:

```sh
aarch64-linux-gnu-objdump -p build/linux-arm64/src/cogidrone | grep NEEDED
```

`libpx4_msgs__rosidl_typesupport_cpp.so` must appear. Making this change
immediately exposed a pre-existing gap: `statistics_msgs__rosidl_typesupport_cpp`
was missing from `src/CMakeLists.txt` (reached via `libstatistics_collector` →
`MetricsMessage` from any real `rclcpp::Node`) and had gone unnoticed precisely
because no emitted code had ever referenced it.

What this does **not** cover: the binary's `Drone::start()` is still empty and
`TestNode` is never constructed, so running it exercises dynamic loading only —
not DDS discovery or the QoS match. Confirming the topics actually connect needs
a live PX4 (or SITL) with a uXRCE-DDS agent.

Note the QoS there: PX4 publishes `/fmu/out/*` as **best-effort, `keep_last(5)`,
volatile**. `rclcpp`'s default is *reliable*, which does not match, and an
unmatched subscription connects to nothing without warning. Every `/fmu/out/*`
subscription needs the explicit profile.
