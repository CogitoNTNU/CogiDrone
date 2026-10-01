"""Start the whole simulation with one command.

    ros2 launch cogidrone_bringup sim.launch.py
    ros2 launch cogidrone_bringup sim.launch.py headless:=true autonomy:=false
    ros2 launch cogidrone_bringup sim.launch.py altitude:=1.0

Starts: Micro XRCE-DDS Agent, Gazebo, PX4 SITL, the Gazebo->ROS camera bridge and,
unless autonomy:=false, the YOLO detector and offboard controller.
"""

import os

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


def _gz_env():
    """What PX4's build/px4_sitl_default/rootfs/gz_env.sh would set, plus Harmonic's `gz` config."""
    gz = os.path.join(PX4_DIR, "Tools/simulation/gz")

    def append(var, *paths):
        return ":".join([p for p in [os.environ.get(var, "")] if p] + list(paths))

    return {
        # ROS ships its own `gz` without the simulator; point it at Gazebo Harmonic's config.
        "GZ_CONFIG_PATH": append("GZ_CONFIG_PATH", "/usr/share/gz"),
        "GZ_SIM_RESOURCE_PATH": append(
            "GZ_SIM_RESOURCE_PATH", os.path.join(gz, "models"), os.path.join(gz, "worlds")
        ),
        "GZ_SIM_SYSTEM_PLUGIN_PATH": append(
            "GZ_SIM_SYSTEM_PLUGIN_PATH",
            os.path.join(PX4_DIR, "build/px4_sitl_default/src/modules/simulation/gz_plugins"),
        ),
        "GZ_SIM_SERVER_CONFIG_PATH": os.path.join(
            PX4_DIR, "src/modules/simulation/gz_bridge/server.config"
        ),
    }


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
    }
    gz_server = ExecuteProcess(
        cmd=[
            "gz", "sim", "--verbose=1", "-r", "-s",
            os.path.join(PX4_DIR, "Tools/simulation/gz/worlds", world + ".sdf"),
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
            DeclareLaunchArgument("world", default_value="cogidrone"),
            DeclareLaunchArgument("model", default_value="cogidrone_x500"),
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
