# Sourced by every shell in the container (entrypoint + ~/.bashrc).

source "/opt/ros/${ROS_DISTRO}/setup.bash"
source /opt/px4_ws/install/setup.bash

if [ -f /workspace/ros2_ws/install/setup.bash ]; then
    source /workspace/ros2_ws/install/setup.bash
fi

# Build our ROS 2 packages: `cb` from anywhere in the container.
cb() {
    (cd /workspace/ros2_ws && colcon build --symlink-install "$@") \
        && source /workspace/ros2_ws/install/setup.bash
}
