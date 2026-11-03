# CogiDrone

CogiDrone is Cogito NTNU's search-and-rescue drone. It flies over an area, looks for people with
a RealSense D435i camera and YOLO, and reports where they are. The drone is an X500 frame with a
Pixhawk 6C (PX4) and a Jetson Orin Nano Super. Until it flies, everything is tested in a
simulator (PX4 SITL + Gazebo Harmonic + ROS 2 Jazzy) on the sim Jetson (AGX Orin 64 GB), with
Gazebo worlds built from real map data of Bymarka in Trondheim.

This page is the starting point: a map of the docs and the four common workflows with the
commands in the order you type them.

## Map of the docs

| Page | Read it for |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Which machine runs what, which program talks to which, every topic, ENU vs NED, the search / viewer data flow |
| [docs/sim.md](docs/sim.md) | The simulator: install, build, worlds, launch arguments, flying, camera vs the real D435i, generating worlds, troubleshooting |
| [docs/jetson.md](docs/jetson.md) | Native install on the Jetsons (no Docker), versions, smoke test, RC joystick, gotchas |
| [docs/hardware.md](docs/hardware.md) | Drone parts, wiring, power, PX4 parameters, bench checklist |
| [scripts/search_test/README.md](scripts/search_test/README.md) | The search test: fly a route, find people, log every sighting, score against the sim truth |
| [tools/flight_viewer/README.md](tools/flight_viewer/README.md) | The 3D flight viewer, the position solver and the run-log format |
| [sim/tools/trees/README.md](sim/tools/trees/README.md), [sim/tools/sky/README.md](sim/tools/sky/README.md) | How the tree library and the photo sky are made (only needed to change them) |
| [python/README.md](python/README.md) | Muhammad's perception prototypes (`python/cv_prototyping`) and YOLO training (`python/cv_training`) |
| [sim/ATTRIBUTION.md](sim/ATTRIBUTION.md) | Map data and texture licences (credit line for slides and screenshots) |

## Before any workflow

**Sim Jetson (native).** Every terminal needs the environment. `~/.bashrc` normally sources it;
if `ros2`, `gz topic` or `$PX4_DIR` are missing in a terminal, source it by hand:

```bash
source ~/Cogito/CogiDrone/scripts/jetson/env.sh
```

It sources ROS 2, `px4_msgs` and our workspace, sets `PX4_DIR`, `GZ_IP=127.0.0.1` and the YOLO
model, and defines `cb`. Build our ROS 2 packages (incl. the C++ core) the first time and after
every change in `ros2_ws/src` or `src/`:

```bash
cb
```

**Laptop (Docker).** Run `./scripts/sim.sh up` once, then `./scripts/sim.sh shell` for every
terminal below, and `cb` in the first one. See [docs/sim.md](docs/sim.md#2-build-and-start).

Only one sim can run at a time. The launch refuses to start while a Gazebo or PX4 SITL is still
running and prints the PIDs to stop.

All commands below are run from the repo root (`~/Cogito/CogiDrone` on the sim Jetson).

## Workflow A: fly by keyboard with the camera window

`autonomy:=false` starts Gazebo, PX4, the XRCE agent and the camera bridge, and nothing that
flies.

**Terminal 1** (the sim):

```bash
ros2 launch cogidrone_bringup sim.launch.py autonomy:=false
```

The Gazebo window opens. PX4 needs about 10-20 s for a GPS fix in the default `cogidrone`
world.

**Terminal 2** (camera window with the YOLO boxes):

```bash
ros2 run cogidrone_perception detector --ros-args -p show:=true
```

Or the plain camera without YOLO:

```bash
ros2 run rqt_image_view rqt_image_view /camera/camera/color/image_raw
```

**Terminal 3** (the keyboard; keep this terminal focused while flying):

```bash
ros2 run cogidrone_control teleop
```

It prints `waiting for PX4...` until PX4 is ready.

| Key | Action |
|---|---|
| `t` | Take off: arm, offboard, climb to 2.5 m |
| `w` / `s` | Forward / back 0.5 m |
| `a` / `d` | Left / right 0.5 m |
| `r` / `f` | Up / down 0.5 m |
| `q` / `e` | Turn left / right 15° |
| space | Stop and hold here |
| `l` | Land |
| `x` | Quit `teleop` |

Each key moves the target one step; PX4 flies there and holds. For a lighter run add
`headless:=true` in terminal 1 (no Gazebo window; the camera window still works).

**Stop:** `l` and wait for touchdown, `x` in terminal 3, `Ctrl-C` in terminal 2, then `Ctrl-C`
in terminal 1 (stops Gazebo, PX4 and the agent).

## Workflow B: autonomous flight with the C++ core

**Terminal 1:**

```bash
ros2 launch cogidrone_bringup sim_cpp.launch.py yolo:=true view:=true
```

After about 20 s the C++ `Pixhawk` node arms, takes off to 2.5 m (`altitude:=`) and hands over to
`Navigation`, which hovers and spins slowly to search. `yolo:=true` runs the Python YOLO detector
and feeds its boxes to Navigation; `view:=true` opens one window with the camera and the boxes.
Add `headless:=true` for no Gazebo window. Do not run `teleop` or `offboard` at the same time:
two programs sending setpoints to PX4 fight each other.

**Terminal 2** (optional, steer the mission):

```bash
ros2 topic echo /navigation/mission_status
ros2 topic pub --once /manual_override std_msgs/msg/String "{data: LAND}"
```

Other words for `/manual_override`: `START_MISSION`, `RETURN_HOME`, `EMERGENCY_STOP`.

**Stop:** send `LAND`, wait for touchdown, then `Ctrl-C` in terminal 1.

## Workflow C: search test, then the flight viewer

[`scripts/search_test`](scripts/search_test/README.md) flies a route with PX4 offboard, runs YOLO
(and the head detector, if its weights are installed) on the camera, turns every person sighting
into a map position, pings each new person, returns home and lands, scores the result against
the world's `*_people.yaml` and writes a run log for the flight viewer. This demo needs the
Bymarka worlds (Workflow D).

**Terminal 1** (sim without any controller, camera 20° down):

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_s headless:=true autonomy:=false cam_pitch:=20
```

`bymarka_dense_s` is ready in about 1 minute and uses ~10 GB RAM.

**Terminal 2** (the test; start it any time, it waits for PX4):

```bash
python3 scripts/search_test/search_test.py --world bymarka_dense_s --dem --cam-pitch 20 --alt 15 --speed 3 --conf 0.4 --route="-12,-42;20,-48;22,20;-5,30;-28,28;-5,10" --area=-80,-60,70,90
```

A window shows the camera with the YOLO boxes and a status line. Each new person prints a green
`PING person N: lat, lon` line. The flight takes about 1.5 min; then the script lands, prints the
summary and exits. `Ctrl-C` lands early and still writes the summary.

| Output | What |
|---|---|
| `~/flight_runs/latest/` | Run log for the viewer (replaced every run; `--keep` also keeps a dated copy) |
| `~/search_test_runs/<time>_<world>/` | `summary.txt`, `sightings.csv`, `persons.csv`, one picture per person |

**Terminal 2 again** (the viewer):

```bash
python3 tools/flight_viewer/view.py
```

It builds the terrain background the first time (cached afterwards), serves
`http://127.0.0.1:8765/` and opens the browser. From another machine, start it with
`--host 0.0.0.0 --no-browser` and open `http://<sim-jetson>:8765/`. Controls and the position
solver: [tools/flight_viewer/README.md](tools/flight_viewer/README.md).

**Stop:** `Ctrl-C` in terminal 2 (the viewer server) and in terminal 1 (the sim). Restart the
sim before the next search test: after one landing PX4 refuses to arm again in the same sim.

## Workflow D: (re)generate the Bymarka worlds

The Bymarka models are generated, not in git. A fresh clone has the `cogidrone` and `sar` worlds
only; the `bymarka*` worlds do not load until these steps have run. Options and details:
[docs/sim.md, section 8](docs/sim.md#8-generating-the-bymarka-worlds).

Once, the Python venv for the generators (numpy, scipy, Pillow):

```bash
python3 -m venv ~/cogidrone_deps/simtools-venv
~/cogidrone_deps/simtools-venv/bin/pip install numpy scipy pillow
```

Then in this order, no sim running:

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=sar headless:=true autonomy:=false
P=~/cogidrone_deps/simtools-venv/bin/python
$P sim/tools/people/make_people.py
$P sim/tools/bymarka/make_world.py --forest --props
$P sim/tools/bymarka/make_dense.py
```

1. The `sar` launch only downloads the three Gazebo Fuel people (~200 MB with the other Fuel
   models, once) that `make_people.py` recolours. Stop it with `Ctrl-C` once it has loaded
   (about a minute). Skip it if `~/.gz/fuel` already has them.
2. `make_people.py`: 36 people in `sim/models/person_*`. It has no options: any argument,
   even `--help`, starts it.
3. `make_world.py --forest --props`: downloads ~200 MB of map data on the first run, then
   builds the shared terrain and the 2 x 2 km `bymarka` world.
4. `make_dense.py`: `bymarka_dense` and the two crops `bymarka_dense_q` and `bymarka_dense_s`.

Steps 3 and 4 take about 10 minutes together. Only the crops again (needs `make_world.py` once):
`$P sim/tools/bymarka/make_dense.py --crops-only` (~40 s). On the sim Jetson the generated models
take about 5.6 GB in `sim/models/` and the download cache about 0.9 GB in
`~/cogidrone_deps/simassets-cache/`.

## Stop everything

`Ctrl-C` in the launch terminal stops Gazebo, PX4, the XRCE agent and the nodes it started.
If a launch ran in the background or its terminal was closed, stop the leftovers by name.
Otherwise the next launch refuses to start, or the new agent cannot bind UDP 8888:

```bash
pkill -f "gz sim"
pkill -f px4_sitl_default/bin/px4
pkill -f MicroXRCEAgent
pkill -x cogidrone
```

## Documentation site

The pages above are also built as a site with mkdocs:

```bash
uv run mkdocs serve
```

It serves [http://127.0.0.1:8000/](http://127.0.0.1:8000/). `docs/index.md` is a link to this
README.

## License

Distributed under the MIT License. See `LICENSE` for more information.
