# search_test (throwaway)

Flies a lawnmower path (or a fixed `--route`) with PX4 offboard, runs stock YOLO (person class) on
the drone camera, places every person sighting on the map (depth, else ray onto the ground),
clusters the sightings into persons, pings each new person, returns home and lands. In the sim it
compares the found persons with `sim/worlds/<world>_people.yaml`. It also writes the run log for
the [flight viewer](../../tools/flight_viewer/README.md).

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=sar headless:=true autonomy:=false
python3 scripts/search_test/search_test.py --world sar --area 20,0,140,160 --alt 15
```

Run the sim with `autonomy:=false` and without the C++ core: any other controller fights this
script over PX4. The script can start before PX4 is ready; it waits. **To remove it: delete this
folder** (`git rm -r scripts/search_test`); nothing else uses it.

## What it does while flying

- **Window** (with a display; `--no-show` turns it off): the drone camera with the YOLO boxes
  (confidence) and a status line (state, height, speed, pings, camera Hz and resolution,
  real-time factor, YOLO fps). The same picture is on `/search_test/image` either way (only drawn
  if someone listens).
- **YOLO** runs on the newest frame in its own thread and drops the rest, so it never lags behind
  or slows the sim. Its input size follows the image (1280 for the 720p camera).
- **Position per sighting:** the depth image within `--depth-max` (10 m), else the ray through the
  box bottom (the feet) hits flat ground at the takeoff height, or the real terrain with `--dem`.
- **Persons:** sightings within `--cluster-m` (4 m) are one person (running mean). After
  `--min-sightings` (3) the person is pinged: a green `PING person N: lat, lon (... E, ... N m from
  takeoff)` line and a `geometry_msgs/PointStamped` on `/search_test/person_pings`.
- **Head detector** (optional): Muhammad's head model on each person crop in its own thread, for
  the head-size distance in the run log. The weights are not in git: put Muhammad's `medium.pt` at
  `~/cogidrone_deps/models/head_medium.pt` (or pass `--head-model`). Without it the head columns
  stay empty.
- **Stop:** it lands by itself at the end. `Ctrl-C` sends land, writes the summary and exits.

## Output

| Where | What |
|---|---|
| `~/search_test_runs/<time>_<world>/` (`--out`) | `summary.txt` (persons, found / missed / false vs the truth, position errors, camera Hz, real-time factor and YOLO fps while flying), `sightings.csv`, `persons.csv`, `person_NN.jpg` per person, `truth_<id>.jpg` for each true person that was in the camera view (sim) |
| `~/flight_runs/latest/` (`--log-dir`) | Run log for the flight viewer, replaced every run. `--keep` also keeps a copy `~/flight_runs/<time>_<world>/`; `--no-runlog` turns it off. Format: [flight viewer README](../../tools/flight_viewer/README.md#run-log-format-version-2) |

Open the run in the viewer:

```bash
python3 tools/flight_viewer/view.py
```

## Options

`--help` lists everything. The main ones:

| Option | Default | |
|---|---|---|
| `--area x0,y0,x1,y1` | `-40,-40,40,40` | Search rectangle, m east / north of takeoff. Lawnmower area, and the area scored against the truth |
| `--route "e,n;e,n;..."` | | Fixed waypoints (m east / north of takeoff) instead of the lawnmower |
| `--spacing` | 40 | Lawnmower leg spacing, m |
| `--alt` | 15 | Height over takeoff, m |
| `--speed` | 4 | m/s |
| `--cam-pitch`, `--cam-x`, `--cam-z` | 0, launch defaults | **Same values as the launch's** `cam_pitch:=`, `cam_x:=`, `cam_z:=` |
| `--conf` | 0.3 | YOLO person confidence |
| `--imgsz` | 0 | YOLO input size; 0 = image width, at least 960 |
| `--world` | | Sim world name: enables the scoring against `<world>_people.yaml` |
| `--dem` | off | Bymarka worlds: real terrain instead of flat ground. Needs the Kartverket terrain cache from `make_world.py` (`~/cogidrone_deps/simassets-cache/bymarka`) |
| `--depth-max` | 10 | Trust depth up to this range, m |
| `--max-range` | 150 | Ignore ground hits further than this, m |
| `--cluster-m`, `--min-sightings` | 4, 3 | Same person within this; sightings before a ping |
| `--match-m`, `--eval-margin` | 5, 0 | Scoring: found = within this of the truth; grow the scored area by this |
| `--no-depth-bridge`, `--depth-bridge` | | The script bridges the Gazebo depth camera itself, unless `--alt` is above `--depth-max` (the ground is out of range; skipping it is ~10 % faster). Real drone: `--no-depth-bridge` |
| `--show` / `--no-show` | on with `$DISPLAY` | The camera window |
| `--log-dir`, `--keep`, `--no-runlog` | `~/flight_runs/latest` | Run log (above). Sampling: `--track-hz`, `--frame-every`, `--frame-min-move`, `--frame-min-turn`, `--max-frames`, `--frame-width` |
| `--head-model`, `--head-conf`, `--head-every`, `--head-min-px`, `--head-imgsz`, `--head-height-m`, `--body-height-m` | see `--help` | Head detector, explained in the [flight viewer README](../../tools/flight_viewer/README.md#writing-the-log) |

Negative numbers at the start of `--route` / `--area` need the `=` form: `--route="-12,-42;20,-48"`.

## Known limits

Stock yolo11n misses most lying people and takes some tree trunks for people; PX4 and camera
clocks differ in SITL, so the pose used is the latest one when the image arrives; after one
landing PX4 refuses the next arming in the same sim (restart the sim between runs).

## Bymarka demos

Start the sim headless (no Gazebo window, see [docs/sim.md](../../docs/sim.md#6-flight-test-mode-one-window)),
then the script. The Bymarka worlds must be generated first
([docs/sim.md, section 8](../../docs/sim.md#8-generating-the-bymarka-worlds)). Every route was
checked against the laser data: the highest obstacle within 8 m of it (terrain + canopy height, at
least 4 m for saplings) is >= 5 m below the flight height. Pass the launch's `cam_pitch` to
`--cam-pitch`. Stock yolo11n finds the animated people (walking, waving) from 20-35 m; it rarely
finds the static ones and takes some small trees for people (false pings).

| World | Highest obstacle on the route | Height | People it passes (<= 35 m) |
|---|---|---|---|
| `bymarka_dense_s` (150 m) | 9.3 m | 15 m | 10 walking, 25 walking, 26 waving, 11 waving, 24 sitting |
| `bymarka_dense_q` (300 m) | 12.3 m | 18 m | 16 waving, 22 sitting, 23 waving, 10 walking |
| `bymarka_dense` (600 m) | (level camera, older demo) | 14 m | 10 walking, 11 waving, 16 waving |

1/16 world, ~1.5 min flight:

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_s headless:=true autonomy:=false cam_pitch:=20
python3 scripts/search_test/search_test.py --world bymarka_dense_s --dem --cam-pitch 20 --alt 15 --speed 3 --conf 0.4 --route="-12,-42;20,-48;22,20;-5,30;-28,28;-5,10" --area=-80,-60,70,90
```

1/4 world, ~3 min flight:

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_q headless:=true autonomy:=false cam_pitch:=20
python3 scripts/search_test/search_test.py --world bymarka_dense_q --dem --cam-pitch 20 --alt 18 --speed 3 --conf 0.4 --route="0,-70;50,-75;95,-15;100,30;10,30;5,70" --area=-150,-130,150,170
```

Full dense world (level camera):

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense headless:=true autonomy:=false
python3 scripts/search_test/search_test.py --world bymarka_dense --dem --cam-pitch 0 --alt 14 --speed 3 --route="8,-45;5,30;-30,30;5,30" --area=-40,-100,20,40
```
