# uXRCE-DDS agent

The companion-computer half of PX4's ROS 2 bridge. A **client** runs inside PX4 on
the Pixhawk; this **agent** runs on the Jetson and proxies that client into the DDS
network, so uORB topics appear as `/fmu/out/*` and `/fmu/in/*`.

Without it running, those topics do not exist. `cogidrone` starts normally, every
subscription stays silent, and nothing reports an error — the link just isn't there.

```sh
scripts/run-agent.sh                       # serial, /dev/ttyTHS1 @ 921600
UXRCE_TRANSPORT=udp4 scripts/run-agent.sh  # local / SITL work
```

## Version pin

`AGENT_REF`, `v2.4.3`, set in both `docker/uxrce-agent.Dockerfile` (an `ARG`) and
`get-agent.sh`.

> [!IMPORTANT]
> **This is a hard constraint, not a default to tidy up later.** PX4's bundled
> client is Micro XRCE-DDS Client `v2.x`, and the PX4 docs state it *"is not
> compatible with the latest `v3.x` Agent version"*. A v3.x agent will not talk to
> PX4. Do not bump the major version, and confirm this pin together with the
> `px4_msgs` pin in `third-party/ros2/PX4_MSGS.md` — they describe two halves of
> the same link and must agree with the firmware actually flashed.

## Why it is vendored, and why in its own directory

Same reasoning as `px4_msgs`: build once in Docker for arm64, commit the artifacts,
keep build tooling off the flight vehicle.

```sh
./third-party/uxrce-agent/get-agent.sh
```

The agent is a **standalone process with no ROS dependency**, so it gets its own
`bin/` and `lib/` instead of joining `third-party/ros2/opt/ros/jazzy/`.

> [!WARNING]
> Do not merge the two library directories, and never put the ROS tree on the
> agent's `LD_LIBRARY_PATH`. The agent's CMake superbuild builds its own **Fast DDS
> 2.14.7**; the vendored ROS tree ships **2.14.6**. Both carry the SONAME
> `libfastrtps.so.2.14`, so the loader treats them as interchangeable when they are
> not the same build — whichever directory comes first on the path wins, for both
> processes. Keeping them apart is what makes that impossible.
>
> This is also why `docker/uxrce-agent.Dockerfile` builds on plain `ubuntu:24.04`
> rather than a ROS image: on a ROS base the superbuild would find Jazzy's Fast DDS
> instead of building the version this agent expects.

`.gitignore:267` (`!third-party/**`) already un-ignores the binary and `.so`s past
the template's `*.so` and `lib/` patterns, so nothing needed changing there.

Runtime dependencies that come from the OS, not from `lib/`: `libstdc++`, `libc`,
`libm`, `libgcc_s`, `libssl.so.3`, `libcrypto.so.3`. All present on a JetPack
Ubuntu image; `libssl3`/`libcrypto3` are the only ones worth checking on a
stripped-down rootfs.

## PX4-side configuration

The agent is only half the setup — the flight controller has to be told to speak.
These are set **on the Pixhawk** via QGroundControl, not on the Jetson, and are
recorded in `config/px4.params`:

| Parameter | Value | Why |
|---|---|---|
| `UXRCE_DDS_CFG` | `TELEM2` | which serial port runs the client |
| `SER_TEL2_BAUD` | `921600` | must match `UXRCE_BAUD` in `scripts/run-agent.sh` |
| `UXRCE_DDS_DOM_ID` | `0` | must match `ROS_DOMAIN_ID` on the Jetson |
| `UXRCE_DDS_KEY` | `1` | client id; only matters with several clients |

A baud mismatch, a wrong port, or a domain-ID mismatch all fail the same silent
way: the agent runs, PX4 runs, and no topics appear.

## Serial device

`/dev/ttyTHS1` is the usual Jetson TELEM2 UART, but the name depends on the carrier
board and how it is wired. It is one environment variable:

```sh
UXRCE_DEV=/dev/ttyUSB0 scripts/run-agent.sh
```

The agent needs read/write access to the device — add the user to `dialout` rather
than running the agent as root.

## Running it on boot

```ini
# /etc/systemd/system/uxrce-agent.service
[Unit]
Description=uXRCE-DDS agent (PX4 bridge)
After=network.target

[Service]
ExecStart=/home/cogito/CogiDrone/scripts/run-agent.sh
Environment=UXRCE_DEV=/dev/ttyTHS1
Environment=UXRCE_BAUD=921600
Restart=always
RestartSec=2
User=cogito

[Install]
WantedBy=multi-user.target
```

`Restart=always` matters: the agent exits if the serial device disappears, which is
exactly what a Pixhawk power-cycle looks like.

## What is not verified

Nothing here proves a real PX4 will connect. The agent has been confirmed to build
for arm64, resolve its libraries from `lib/`, and listen. The first genuine
end-to-end check needs either the airframe or PX4 SITL
(`px4io/px4-sitl` with `PX4_SIM_MODEL=none` is headless and needs no Gazebo).
