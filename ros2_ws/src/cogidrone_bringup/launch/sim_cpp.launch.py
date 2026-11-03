"""The simulation, flown by the C++ drone core (src/) instead of the Python nodes.

    ros2 launch cogidrone_bringup sim_cpp.launch.py
    ros2 launch cogidrone_bringup sim_cpp.launch.py headless:=true yolo:=true
    ros2 launch cogidrone_bringup sim_cpp.launch.py headless:=true yolo:=true view:=true  # one window
    ros2 launch cogidrone_bringup sim_cpp.launch.py world:=sar   # the big search-and-rescue map
    ros2 launch cogidrone_bringup sim_cpp.launch.py cam_pitch:=30   # camera tilted 30 deg down

Starts sim.launch.py (Gazebo + PX4 SITL + XRCE agent + camera bridge, without the Python
autonomy) and the C++ `cogidrone` process (perception + navigation + pixhawk nodes). The
Pixhawk node connects Navigation to PX4: odometry in, velocity setpoints out, arm + takeoff.
Do not also run the Python `offboard` node (sim.launch.py autonomy:=true): two nodes would fight over PX4.

yolo:=true also runs the Python YOLO detector and feeds its detections to Navigation,
until the C++ perception publishes its own.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    sim_launch = os.path.join(
        get_package_share_directory("cogidrone_bringup"), "launch", "sim.launch.py"
    )
    yolo = LaunchConfiguration("yolo")
    # Navigation listens on /perception/detections. The C++ perception publishes nothing there
    # yet, so with yolo:=true the whole process is remapped to YOLO's topic.
    detections = PythonExpression(
        ["'/cogidrone/detections' if '", yolo, "' == 'true' else '/perception/detections'"]
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "headless", default_value="false", description="No Gazebo window"
            ),
            DeclareLaunchArgument(
                "altitude", default_value="2.5", description="Takeoff height in m"
            ),
            DeclareLaunchArgument(
                "auto_start",
                default_value="true",
                description="Send START_MISSION to Navigation after takeoff",
            ),
            DeclareLaunchArgument(
                "world", default_value="cogidrone", description="cogidrone or sar (see sim.launch.py)"
            ),
            DeclareLaunchArgument(
                "cam_pitch", default_value="0", description="Camera tilt in deg, positive = down"
            ),
            DeclareLaunchArgument("cam_x", default_value="0.12", description="Camera forward, m"),
            DeclareLaunchArgument("cam_z", default_value="0.242", description="Camera height, m"),
            DeclareLaunchArgument("cam_res", default_value="720p", description="720p or 480p (D435i modes)"),
            DeclareLaunchArgument("cam_fps", default_value="30", description="Color camera rate, Hz"),
            DeclareLaunchArgument("cam_aa", default_value="4", description="Color camera MSAA samples"),
            DeclareLaunchArgument(
                "view",
                default_value="false",
                description="With yolo:=true: one window with the camera and the YOLO boxes",
            ),
            DeclareLaunchArgument(
                "yolo",
                default_value="false",
                description="Feed the Python YOLO detections to Navigation",
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(sim_launch),
                launch_arguments={
                    "headless": LaunchConfiguration("headless"),
                    "world": LaunchConfiguration("world"),
                    "autonomy": "false",
                    "cam_pitch": LaunchConfiguration("cam_pitch"),
                    "cam_x": LaunchConfiguration("cam_x"),
                    "cam_z": LaunchConfiguration("cam_z"),
                    "cam_res": LaunchConfiguration("cam_res"),
                    "cam_fps": LaunchConfiguration("cam_fps"),
                    "cam_aa": LaunchConfiguration("cam_aa"),
                }.items(),
            ),
            Node(
                package="cogidrone_core",
                executable="cogidrone",
                output="screen",
                remappings=[
                    # Same topic realsense-ros uses on the real D435i.
                    ("/camera/image", "/camera/camera/color/image_raw"),
                    ("/perception/detections", detections),
                ],
                # No node name given, so these apply to every node in the process:
                # target_height is read by both Navigation and Pixhawk.
                parameters=[
                    {
                        "target_height": ParameterValue(
                            LaunchConfiguration("altitude"), value_type=float
                        ),
                        "auto_start": ParameterValue(
                            LaunchConfiguration("auto_start"), value_type=bool
                        ),
                    }
                ],
            ),
            Node(
                package="cogidrone_perception",
                executable="detector",
                parameters=[{"show": ParameterValue(LaunchConfiguration("view"), value_type=bool)}],
                output="screen",
                condition=IfCondition(yolo),
            ),
        ]
    )
