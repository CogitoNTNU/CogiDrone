"""Start the whole simulation with one command.

    ros2 launch cogidrone_bringup sim.launch.py
    ros2 launch cogidrone_bringup sim.launch.py headless:=true autonomy:=false
    ros2 launch cogidrone_bringup sim.launch.py altitude:=1.0
    ros2 launch cogidrone_bringup sim.launch.py cam_pitch:=30   # camera tilted 30 deg down
    ros2 launch cogidrone_bringup sim.launch.py cam_res:=480p cam_fps:=15   # lighter camera
    ros2 launch cogidrone_bringup sim.launch.py headless:=true view:=true   # one window: camera + YOLO boxes

Starts: Micro XRCE-DDS Agent, Gazebo, PX4 SITL, the Gazebo->ROS camera bridge and,
unless autonomy:=false, the YOLO detector and offboard controller. view:=true opens one window
with the camera and the YOLO boxes (the detector's own, no rqt needed).
"""

import math
import os
import re

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    OpaqueFunction,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue

PX4_DIR = os.environ.get("PX4_DIR", "/opt/PX4-Autopilot")
PX4_PARAM = os.path.join(PX4_DIR, "build/px4_sitl_default/bin/px4-param")
SET_NO_GCS_REQUIRED = f"""
ok=0
for i in $(seq 1 60); do
  sleep 2
  if {PX4_PARAM} show NAV_DLL_ACT 2>/dev/null | grep -q ": 0$"; then
    ok=$((ok+1)); [ $ok -ge 5 ] && exit 0
  else
    ok=0; {PX4_PARAM} set NAV_DLL_ACT 0 >/dev/null 2>&1
  fi
done
"""
# QGroundControl on the host listens on UDP 14550, but PX4's own GCS link only talks
# to localhost inside the container. Add a link aimed at the host (Docker Desktop).
QGC_LINK = f"""
host=$(getent ahostsv4 host.docker.internal | head -1 | cut -d' ' -f1)
[ -z "$host" ] && exit 0
for i in $(seq 1 60); do
  sleep 2
  {PX4_DIR}/build/px4_sitl_default/bin/px4-mavlink start -x -u 14590 -r 4000000 -f \\
    -t "$host" -o 14550 >/dev/null 2>&1 && exit 0
done
"""


def _repo_sim_dir():
    """The repo's sim/ folder. PX4's worlds folder links to our cogidrone.sdf (Dockerfile,
    scripts/jetson/install-user.sh), so follow that link instead of guessing where the repo is."""
    link = os.path.join(PX4_DIR, "Tools/simulation/gz/worlds/cogidrone.sdf")
    return os.path.dirname(os.path.dirname(os.path.realpath(link)))


def _gz_env():
    """What PX4's build/px4_sitl_default/rootfs/gz_env.sh would set, plus Harmonic's `gz` config,
    plus our own models (sim/models)."""
    gz = os.path.join(PX4_DIR, "Tools/simulation/gz")

    def append(var, *paths):
        return ":".join([p for p in [os.environ.get(var, "")] if p] + list(paths))

    return {
        # ROS ships its own `gz` without the simulator; point it at Gazebo Harmonic's config.
        "GZ_CONFIG_PATH": append("GZ_CONFIG_PATH", "/usr/share/gz"),
        "GZ_SIM_RESOURCE_PATH": append(
            "GZ_SIM_RESOURCE_PATH",
            os.path.join(gz, "models"),
            os.path.join(gz, "worlds"),
            os.path.join(_repo_sim_dir(), "models"),
        ),
        "GZ_SIM_SYSTEM_PLUGIN_PATH": append(
            "GZ_SIM_SYSTEM_PLUGIN_PATH",
            os.path.join(PX4_DIR, "build/px4_sitl_default/src/modules/simulation/gz_plugins"),
        ),
        "GZ_SIM_SERVER_CONFIG_PATH": os.path.join(
            PX4_DIR, "src/modules/simulation/gz_bridge/server.config"
        ),
        **_sky_env(),
    }


def _sky_env():
    """Photographic sky (sim/tools/sky): Harmonic ignores <sky><cubemap_uri>, so the sky texture
    is swapped through a copy of gz-rendering's media folder. Only if it has been built."""
    media = os.path.expanduser("~/cogidrone_deps/simassets-cache/sky/media_root")
    return {"GZ_RENDERING_RESOURCE_PATH": media} if os.path.isdir(media) else {}


# Front camera mount of cogidrone_x500 (camera_link relative to the model origin, which is on the
# ground under base_link). Defaults = the pose written in sim/models/cogidrone_x500/model.sdf.
CAM_DEFAULTS = {"cam_x": "0.12", "cam_z": "0.242", "cam_pitch": "0"}
_CAM_POSE = re.compile(r'(<link name="camera_link">\s*<pose>)[^<]*(</pose>)')
# Color modes of the real D435i (realsense-ros rgb_camera.color_profile). fx = fy is the same
# sensor: 720p is the full 16:9 frame scaled, 480p a 4:3 crop of it (docs/sim.md).
CAM_RES = {"720p": (1280, 720, 1.2175), "480p": (640, 480, 0.9634)}
# Color mode defaults = what model.sdf says.
COLOR_DEFAULTS = {"cam_res": "720p", "cam_fps": "30", "cam_aa": "4"}


def _color_sensor_edit(sdf, res, fps, aa):
    """Replace size, FOV, rate and anti-aliasing of the color sensor (not the depth one)."""
    w, h, hfov = CAM_RES[res]
    start = sdf.index('<sensor name="color"')
    end = sdf.index("</sensor>", start)
    block = sdf[start:end]
    for tag, val in (("update_rate", f"{fps:g}"), ("horizontal_fov", f"{hfov}"),
                     ("anti_aliasing", f"{aa}"), ("width", f"{w}"), ("height", f"{h}")):
        block, n = re.subn(rf"<{tag}>[^<]*</{tag}>", f"<{tag}>{val}</{tag}>", block, count=1)
        if n != 1:
            raise RuntimeError(f"<{tag}> not found in the color sensor of the drone model")
    return sdf[:start] + block + sdf[end:]


def _camera_model_env(context, model):
    """If the camera mount (cam_x / cam_z / cam_pitch) or color mode (cam_res / cam_fps / cam_aa)
    differ from the defaults, write a copy of the drone model with them and point PX4's spawn
    (PX4_GZ_MODELS) at it. The repo file stays as is."""
    cam = {k: float(LaunchConfiguration(k).perform(context)) for k in CAM_DEFAULTS}
    col = {k: LaunchConfiguration(k).perform(context).strip() for k in COLOR_DEFAULTS}
    if col["cam_res"] not in CAM_RES:
        raise RuntimeError(f"cam_res:={col['cam_res']}: use one of {', '.join(CAM_RES)}")
    fps, aa = float(col["cam_fps"]), int(col["cam_aa"])
    same_mount = all(cam[k] == float(v) for k, v in CAM_DEFAULTS.items())
    same_color = (col["cam_res"] == COLOR_DEFAULTS["cam_res"] and fps == float(COLOR_DEFAULTS["cam_fps"])
                  and aa == int(COLOR_DEFAULTS["cam_aa"]))
    if model != "cogidrone_x500" or (same_mount and same_color):
        return {}
    src = os.path.join(_repo_sim_dir(), "models", model, "model.sdf")
    with open(src) as f:
        sdf = f.read()
    pose = f"{cam['cam_x']} 0 {cam['cam_z']} 0 {math.radians(cam['cam_pitch']):.6f} 0"
    sdf, n = _CAM_POSE.subn(lambda m: m.group(1) + pose + m.group(2), sdf)
    if n != 1:
        raise RuntimeError(f"camera_link <pose> not found in {src}")
    sdf = _color_sensor_edit(sdf, col["cam_res"], fps, aa)
    root = os.path.expanduser("~/.cache/cogidrone/models")
    os.makedirs(os.path.join(root, model), exist_ok=True)
    with open(os.path.join(root, model, "model.sdf"), "w") as f:
        f.write(sdf)
    print(f"[sim.launch] camera: x={cam['cam_x']} z={cam['cam_z']} pitch={cam['cam_pitch']} deg down, "
          f"color {col['cam_res']} @ {fps:g} Hz, MSAA {aa} -> {root}/{model}/model.sdf")
    # px4-rc.gzsim spawns file://${PX4_GZ_MODELS}/<model>/model.sdf.
    return {"PX4_GZ_MODELS": root}


def _already_running():
    """PIDs + command lines of a Gazebo server/GUI or PX4 SITL that is already running."""
    found = []
    for pid in filter(str.isdigit, os.listdir("/proc")):
        try:
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                argv = f.read().split(b"\0")
        except OSError:
            continue
        # gz renames its process, so its whole command line can end up in argv[0].
        line = " ".join(a.decode(errors="replace") for a in argv if a)
        if line.startswith("gz sim") or line.split(" ")[0].endswith("px4_sitl_default/bin/px4"):
            found.append(f"  {pid}  {line[:80]}")
    return found


def _px4(context):
    # Two sims at once share Gazebo's transport, so PX4 gets IMU data from both and its
    # estimator goes haywire ("timestamp error", "Accel #0 fail: TIMEOUT"). Refuse instead.
    running = _already_running()
    if running:
        raise RuntimeError(
            "A simulator is already running. Stop it first (Ctrl-C in its terminal):\n"
            + "\n".join(running)
        )
    world = LaunchConfiguration("world").perform(context)
    # Worlds in the repo (sim/worlds) first, then PX4's own (default, baylands, ...).
    world_file = os.path.join(_repo_sim_dir(), "worlds", world + ".sdf")
    if not os.path.exists(world_file):
        world_file = os.path.join(PX4_DIR, "Tools/simulation/gz/worlds", world + ".sdf")
    gz_env = _gz_env()
    env = {
        **gz_env,
        # 4001 = PX4's generic x500 airframe; our model is the same frame + a camera.
        "PX4_SYS_AUTOSTART": "4001",
        "PX4_SIM_MODEL": "gz_" + LaunchConfiguration("model").perform(context),
        "PX4_GZ_WORLD": world,
        # We start Gazebo ourselves (below) instead of letting PX4 do it: PX4 starts it in the
        # background and orphans it, so it kept running after Ctrl-C and broke the next run.
        # In standalone mode PX4 just waits for the world and spawns the drone into it.
        "PX4_GZ_STANDALONE": "1",
        **_camera_model_env(context, LaunchConfiguration("model").perform(context)),
    }
    gz_server = ExecuteProcess(
        cmd=[
            "gz", "sim", "--verbose=1", "-r", "-s", world_file,
        ],
        additional_env=gz_env,
        output="screen",
    )
    gz_gui = ExecuteProcess(
        cmd=["gz", "sim", "-g"], additional_env=gz_env, output="log"
    )
    headless = LaunchConfiguration("headless").perform(context).lower() == "true"
    return ([gz_server] if headless else [gz_server, gz_gui]) + [
        ExecuteProcess(
            cmd=[os.path.join(PX4_DIR, "build/px4_sitl_default/bin/px4"), "-d"],
            cwd=PX4_DIR,
            additional_env=env,
            output="screen",
        ),
        # Sim only: allow arming without QGroundControl connected (keep this on the
        # real drone). PX4 raises this default late in startup, which overrides a
        # PX4_PARAM_ env var, so keep setting it until it has stayed 0 for a while.
        ExecuteProcess(
            cmd=["bash", "-c", SET_NO_GCS_REQUIRED],
            output="log",
        ),
        ExecuteProcess(cmd=["bash", "-c", QGC_LINK], output="log"),
    ]


def generate_launch_description():
    bridge_config = os.path.join(
        get_package_share_directory("cogidrone_bringup"), "config", "gz_bridge.yaml"
    )
    autonomy = IfCondition(LaunchConfiguration("autonomy"))

    return LaunchDescription(
        [
            # Keep Gazebo's transport (gz <-> PX4 <-> ros_gz_bridge) on loopback. By default it
            # binds to the first network interface (e.g. campus Wi-Fi), where sensor messages got
            # delayed enough for PX4 to report "Accel #0 fail: TIMEOUT" and refuse to arm.
            SetEnvironmentVariable("GZ_IP", os.environ.get("GZ_IP", "127.0.0.1")),
            DeclareLaunchArgument(
                "headless", default_value="false", description="No Gazebo window"
            ),
            DeclareLaunchArgument(
                "autonomy", default_value="true", description="Start YOLO + controller"
            ),
            DeclareLaunchArgument(
                "altitude", default_value="3.0", description="Takeoff height in m"
            ),
            DeclareLaunchArgument(
                "world",
                default_value="cogidrone",
                description="cogidrone (small test) or sar (600 m search-and-rescue map), "
                "or any world in sim/worlds or PX4's worlds folder",
            ),
            DeclareLaunchArgument("model", default_value="cogidrone_x500"),
            DeclareLaunchArgument(
                "cam_pitch",
                default_value=CAM_DEFAULTS["cam_pitch"],
                description="Front camera tilt in degrees, positive = down (D435i mount)",
            ),
            DeclareLaunchArgument(
                "cam_x", default_value=CAM_DEFAULTS["cam_x"], description="Camera forward of centre, m"
            ),
            DeclareLaunchArgument(
                "cam_z", default_value=CAM_DEFAULTS["cam_z"], description="Camera height over the model origin (ground), m"
            ),
            DeclareLaunchArgument(
                "cam_res", default_value=COLOR_DEFAULTS["cam_res"],
                description="Color camera mode like the real D435i: 720p (1280x720, 69.8 deg) or 480p (640x480, 55.2 deg)",
            ),
            DeclareLaunchArgument(
                "cam_fps", default_value=COLOR_DEFAULTS["cam_fps"], description="Color camera rate, Hz"
            ),
            DeclareLaunchArgument(
                "cam_aa", default_value=COLOR_DEFAULTS["cam_aa"],
                description="Color camera anti-aliasing (MSAA samples: 0, 2, 4, 8)",
            ),
            DeclareLaunchArgument(
                "view", default_value="false",
                description="Detector opens a window with the camera and YOLO boxes (needs autonomy:=true)",
            ),
            ExecuteProcess(cmd=["xrce-agent"], output="screen"),
            OpaqueFunction(function=_px4),
            Node(
                package="ros_gz_bridge",
                executable="parameter_bridge",
                parameters=[{"config_file": bridge_config}],
                output="screen",
            ),
            Node(
                package="cogidrone_perception",
                executable="detector",
                parameters=[{"show": ParameterValue(LaunchConfiguration("view"), value_type=bool)}],
                output="screen",
                condition=autonomy,
            ),
            Node(
                package="cogidrone_control",
                executable="offboard",
                parameters=[
                    {
                        "altitude": ParameterValue(
                            LaunchConfiguration("altitude"), value_type=float
                        )
                    }
                ],
                output="screen",
                condition=autonomy,
            ),
        ]
    )
