"""Start the whole simulation with one command.

    ros2 launch cogidrone_bringup sim.launch.py
    ros2 launch cogidrone_bringup sim.launch.py headless:=true autonomy:=false

Starts: Micro XRCE-DDS Agent, PX4 SITL (which starts Gazebo), the Gazebo->ROS
camera bridge and, unless autonomy:=false, the YOLO detector and offboard controller.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

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


def _px4(context):
    env = {
        # 4001 = PX4's generic x500 airframe; our model is the same frame + a camera.
        "PX4_SYS_AUTOSTART": "4001",
        "PX4_SIM_MODEL": "gz_" + LaunchConfiguration("model").perform(context),
        "PX4_GZ_WORLD": LaunchConfiguration("world").perform(context),
        # ROS ships its own `gz` without the simulator; point it at Gazebo Harmonic's config.
        "GZ_CONFIG_PATH": os.environ.get("GZ_CONFIG_PATH", "") + ":/usr/share/gz",
    }
    if LaunchConfiguration("headless").perform(context).lower() == "true":
        env["HEADLESS"] = "1"  # PX4 only checks whether this is set at all
    return [
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
    ]


def generate_launch_description():
    bridge_config = os.path.join(
        get_package_share_directory("cogidrone_bringup"), "config", "gz_bridge.yaml"
    )
    autonomy = IfCondition(LaunchConfiguration("autonomy"))

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "headless", default_value="false", description="No Gazebo window"
            ),
            DeclareLaunchArgument(
                "autonomy", default_value="true", description="Start YOLO + controller"
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
                output="screen",
                condition=autonomy,
            ),
        ]
    )
