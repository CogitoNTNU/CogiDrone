# search_test (throwaway)

Flies a lawnmower path with PX4 offboard, runs stock YOLO (person class) on the drone camera, places
every person sighting on the map (depth, else ray onto the ground) and logs it to
`~/search_test_runs/<time>_<world>/` (sightings.csv, persons.csv, summary.txt, one picture per person).
In the sim it compares the found persons with `sim/worlds/<world>_people.yaml`.

```bash
ros2 launch cogidrone_bringup sim.launch.py world:=sar headless:=true autonomy:=false   # no C++ core
python3 scripts/search_test/search_test.py --world sar --area 20,0,140,160 --alt 15
```

`--help` lists the options (altitude, speed, camera pitch: pass the same `--cam-pitch` as the
launch's `cam_pitch:=`). **To remove it: delete this folder** (`git rm -r scripts/search_test`); nothing else uses it.

Known limits: stock yolo11n misses most lying people and takes some tree trunks for people; PX4 and
camera clocks differ in SITL, so the pose used is the latest one when the image arrives; after one
landing PX4 refuses the next arming in the same sim (restart the sim between runs).

With a display it opens one window: the drone camera with the YOLO boxes (confidence) and a status
line (state, height, speed, pings, camera Hz and resolution, real-time factor, YOLO fps). `--no-show`
turns it off; the same picture is on `/search_test/image` either way (only drawn if someone listens).
YOLO runs on the newest frame in its own thread and drops the rest, so it never lags behind or slows
the sim. Its input size follows the image (1280 for the 720p camera). The summary ends with the
camera Hz, real-time factor and YOLO fps measured while flying.

**Bymarka demos.** Start the sim headless (no Gazebo window, see docs/sim.md "Flight test mode"), then
the script. Every route was checked against the laser data: the highest obstacle within 8 m of it
(terrain + canopy height, at least 4 m for saplings) is >= 5 m below the flight height. Pass the
launch's `cam_pitch` to `--cam-pitch`. Stock yolo11n finds the animated people (walking, waving)
from 20-35 m; it rarely finds the static ones and takes some small trees for people (false pings).

| World | Highest obstacle on the route | Height | People it passes (<= 35 m) |
|---|---|---|---|
| `bymarka_dense_s` (150 m) | 9.3 m | 15 m | 10 walking, 25 walking, 26 waving, 11 waving, 24 sitting |
| `bymarka_dense_q` (300 m) | 12.3 m | 18 m | 16 waving, 22 sitting, 23 waving, 10 walking |
| `bymarka_dense` (600 m) | (level camera, older demo) | 14 m | 10 walking, 11 waving, 16 waving |

    # 1/16 world, ~1.5 min flight
    ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_s headless:=true autonomy:=false cam_pitch:=20
    python3 scripts/search_test/search_test.py --world bymarka_dense_s --dem --cam-pitch 20 --alt 15 --speed 3 \
        --conf 0.4 --route="-12,-42;20,-48;22,20;-5,30;-28,28;-5,10" --area=-80,-60,70,90

    # 1/4 world, ~3 min flight
    ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense_q headless:=true autonomy:=false cam_pitch:=20
    python3 scripts/search_test/search_test.py --world bymarka_dense_q --dem --cam-pitch 20 --alt 18 --speed 3 \
        --conf 0.4 --route="0,-70;50,-75;95,-15;100,30;10,30;5,70" --area=-150,-130,150,170

    # full dense world (level camera)
    ros2 launch cogidrone_bringup sim.launch.py world:=bymarka_dense headless:=true autonomy:=false
    python3 scripts/search_test/search_test.py --world bymarka_dense --dem --cam-pitch 0 --alt 14 --speed 3 \
        --route="8,-45;5,30;-30,30;5,30" --area=-40,-100,20,40
