# Native Jetson environment, the counterpart of docker/sim/env.sh. Source it from ~/.bashrc:
#
#   source ~/Cogito/CogiDrone/scripts/jetson/env.sh
#
# Everything install-user.sh builds lives in $COGI_DEPS (outside the repo).

COGIDRONE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export COGIDRONE_ROOT
export COGI_DEPS="${COGI_DEPS:-$HOME/cogidrone_deps}"

# CUDA 13.2 (JetPack 7.2)
if [ -d /usr/local/cuda ]; then
    export CUDA_HOME=/usr/local/cuda
    case ":$PATH:" in *":$CUDA_HOME/bin:"*) ;; *) export PATH="$CUDA_HOME/bin:$PATH" ;; esac
fi

source /opt/ros/jazzy/setup.bash
[ -f "$COGI_DEPS/px4_ws/install/setup.bash" ] && source "$COGI_DEPS/px4_ws/install/setup.bash"

# Python deps (torch, ultralytics, cuvslam) live in a venv that can still see the apt ROS packages.
# It goes on PYTHONPATH so ROS nodes run by /usr/bin/python3 find them, same as in the Docker image.
if [ -d "$COGI_DEPS/venv" ]; then
    export PYTHONPATH="$COGI_DEPS/venv/lib/python3.12/site-packages${PYTHONPATH:+:$PYTHONPATH}"
    export PATH="$PATH:$COGI_DEPS/venv/bin"
fi
export YOLO_CONFIG_DIR="${YOLO_CONFIG_DIR:-$COGI_DEPS/ultralytics}"
export COGIDRONE_YOLO_MODEL="${COGIDRONE_YOLO_MODEL:-$COGI_DEPS/models/yolo11n.pt}"
export COGIDRONE_YOLO_DEVICE="${COGIDRONE_YOLO_DEVICE:-cuda:0}"

# xrce-agent wrapper and other helpers
export PATH="$COGI_DEPS/bin:$PATH"

# PX4 SITL + Gazebo (sim Jetson only)
if [ -d "$COGI_DEPS/PX4-Autopilot" ]; then
    export PX4_DIR="$COGI_DEPS/PX4-Autopilot"
    # ROS ships its own `gz` without the simulator; let it find Gazebo Harmonic's `gz sim`.
    export GZ_CONFIG_PATH="${GZ_CONFIG_PATH:+$GZ_CONFIG_PATH:}/usr/share/gz"
    # Same as sim.launch.py: Gazebo transport on loopback, so `gz topic ...` sees the sim.
    export GZ_IP="${GZ_IP:-127.0.0.1}"
fi

# cuVSLAM (C++ lib for anything that links it directly)
if [ -f "$COGI_DEPS/cuVSLAM/build/bin/libcuvslam.so" ]; then
    export CUVSLAM_BUILD_DIR="$COGI_DEPS/cuVSLAM/build"
    export LD_LIBRARY_PATH="$CUVSLAM_BUILD_DIR/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

[ -f "$COGIDRONE_ROOT/ros2_ws/install/setup.bash" ] && source "$COGIDRONE_ROOT/ros2_ws/install/setup.bash"

# Build our ROS 2 packages: `cb` from anywhere.
cb() {
    (cd "$COGIDRONE_ROOT/ros2_ws" && colcon build --symlink-install "$@") \
        && source "$COGIDRONE_ROOT/ros2_ws/install/setup.bash"
}
