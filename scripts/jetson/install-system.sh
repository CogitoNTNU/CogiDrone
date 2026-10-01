#!/usr/bin/env bash
# Native (no Docker) system setup for CogiDrone on a Jetson running JetPack 7.2 (L4T r39.2, Ubuntu 24.04).
# Needs sudo. Run it once per Jetson, then run install-user.sh (no sudo).
#
#   sudo scripts/jetson/install-system.sh sim     # dev/sim Jetson: everything incl. Gazebo + PX4 SITL deps
#   sudo scripts/jetson/install-system.sh drone   # flight Jetson (Orin Nano Super): no Gazebo / PX4 SITL
#
# Versions (see docs/jetson.md for why):
#   CUDA 13.2, cuDNN 9.20, TensorRT 10.16 (JetPack 7.2.1 apt repo, already configured by L4T)
#   ROS 2 Jazzy, Gazebo Harmonic (sim only)
#
# ! Do NOT install the `nvidia-jetpack` meta package. It pulls NVIDIA's OpenCV 4.8 (and VPI, which
# ! needs it), which replaces Ubuntu's libopencv-dev 4.6 that every ROS 2 Jazzy package is built
# ! against (cv_bridge, image_transport...). We install CUDA / cuDNN / TensorRT directly instead.
set -euo pipefail

ROLE="${1:-}"
case "$ROLE" in
    sim|drone) ;;
    *) echo "usage: sudo $0 sim|drone"; exit 1 ;;
esac
[ "$(id -u)" -eq 0 ] || { echo "ERROR: run with sudo"; exit 1; }
TARGET_USER="${SUDO_USER:-$(logname 2>/dev/null || echo root)}"

export DEBIAN_FRONTEND=noninteractive
APT="apt-get install -y --no-install-recommends"

# ! The L4T repo carries libopencv-dev 4.8 at a higher version than Ubuntu's 4.6, so even
# ! `ros-jazzy-cv-bridge` would pull NVIDIA's headers on top of Ubuntu's 4.6 runtime libs.
# ! Block NVIDIA's OpenCV outright so apt always resolves the ROS-compatible one.
cat > /etc/apt/preferences.d/cogidrone-no-nvidia-opencv <<'EOF'
Package: libopencv libopencv-dev libopencv-python libopencv-samples opencv-licenses opencv-samples-data nvidia-opencv nvidia-opencv-dev
Pin: origin repo.download.nvidia.com
Pin-Priority: -1
EOF

echo ">> [1/5] CUDA 13.2 + cuDNN + TensorRT (JetPack 7.2 components, no NVIDIA OpenCV/VPI)"
apt-get update
$APT \
    cuda-toolkit-13-2 \
    libcudnn9-dev-cuda-13 \
    tensorrt python3-libnvinfer \
    nvidia-l4t-jetson-multimedia-api
# cuda-toolkit does not create the unversioned symlink on every L4T build.
[ -e /usr/local/cuda ] || ln -s /usr/local/cuda-13.2 /usr/local/cuda

echo ">> [2/5] Build tools"
$APT \
    build-essential cmake ninja-build ccache git git-lfs gdb \
    curl wget lsb-release gnupg ca-certificates \
    python3-dev python3-pip python3-venv python3-setuptools python3-wheel \
    libssl-dev libxml2-dev libxml2-utils libboost-all-dev libeigen3-dev \
    rsync unzip zip file

echo ">> [3/5] ROS 2 Jazzy packages (ros-base is assumed present; adds what our nodes and cuVSLAM need)"
$APT \
    ros-jazzy-ros-base \
    python3-colcon-common-extensions python3-rosdep python3-vcstool \
    ros-jazzy-vision-msgs \
    ros-jazzy-cv-bridge \
    ros-jazzy-image-transport \
    ros-jazzy-tf2-ros ros-jazzy-tf2-geometry-msgs \
    ros-jazzy-rosbag2-storage-mcap \
    ros-jazzy-diagnostic-updater \
    ros-jazzy-foxglove-bridge \
    ros-jazzy-realsense2-camera ros-jazzy-librealsense2 \
    ros-jazzy-rtabmap-ros   # CPU VIO + 3D mapping; fallback if cuVSLAM/nvblox are not ready
[ -f /etc/ros/rosdep/sources.list.d/20-default.list ] || rosdep init || true

if [ "$ROLE" = sim ]; then
    echo ">> [4/5] Gazebo Harmonic + PX4 SITL build deps (sim only)"
    wget -q https://packages.osrfoundation.org/gazebo.gpg -O /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/gazebo-stable.list
    apt-get update
    # Mirrors PX4-Autopilot v1.17.0 Tools/setup/ubuntu.sh --no-nuttx (minus the Python bits,
    # which install-user.sh puts in the venv instead of --break-system-packages).
    $APT \
        gz-harmonic libunwind-dev cppzmq-dev \
        ros-jazzy-ros-gz-bridge ros-jazzy-ros-gz-image ros-jazzy-rqt-image-view \
        astyle cppcheck lcov shellcheck \
        libopencv-dev pkg-config protobuf-compiler \
        libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad gstreamer1.0-libav
else
    echo ">> [4/5] skipping Gazebo / PX4 SITL (drone role)"
fi

echo ">> [5/5] Groups for $TARGET_USER (serial link to the Pixhawk, GPU, cameras)"
for g in dialout video render plugdev; do
    getent group "$g" >/dev/null && usermod -aG "$g" "$TARGET_USER"
done
# The Pixhawk usually sits on the Jetson UART; nvgetty grabs ttyTCU0/ttyTHS* consoles otherwise.
systemctl disable --now nvgetty.service 2>/dev/null || true

# Earlier `docker compose` runs leave root-owned build/install/__pycache__ dirs that break native colcon.
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
chown -R "$TARGET_USER:" "$REPO/ros2_ws"

echo
echo "Done ($ROLE). Next, as your normal user (no sudo):"
echo "  scripts/jetson/install-user.sh $ROLE"
echo "Log out and back in once so the new groups apply."
