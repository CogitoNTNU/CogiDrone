#!/usr/bin/env bash
# Native (no Docker) user-level setup for CogiDrone on a Jetson. Run AFTER install-system.sh, without sudo.
#
#   scripts/jetson/install-user.sh sim      # + PX4 SITL (compiled here, ~15 min on AGX Orin)
#   scripts/jetson/install-user.sh drone    # flight Jetson: no PX4 SITL
#
# Each step is skipped if its output already exists; delete the folder in $COGI_DEPS to redo it.
# SKIP_CUVSLAM=1 skips the (long) cuVSLAM build.
# Versions are pinned below and match docker/sim/Dockerfile. PX4 and px4_msgs MUST match.
set -euo pipefail

PX4_VERSION=v1.17.0
PX4_MSGS_BRANCH=release/1.17
XRCE_AGENT_VERSION=v2.4.3          # ! must stay v2.x: PX4's client is v2 (see third-party/uxrce-agent/AGENT.md on px4MessageSetup)
CUVSLAM_VERSION=v17.0.0
TORCH_INDEX=https://download.pytorch.org/whl/cu132   # official aarch64 CUDA 13.2 wheels, run on JetPack 7.2
YOLO_MODEL=yolo11n.pt
CUDA_ARCH=87                       # Orin (AGX / NX / Nano). Thor would be 110.
QGC_VERSION=v5.1.4                 # ground station, aarch64 AppImage (sim role)

ROLE="${1:-}"
case "$ROLE" in
    sim|drone) ;;
    *) echo "usage: $0 sim|drone"; exit 1 ;;
esac
[ "$(id -u)" -ne 0 ] || { echo "ERROR: run as your normal user, not root"; exit 1; }

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COGI_DEPS="${COGI_DEPS:-$HOME/cogidrone_deps}"
JOBS="${JOBS:-$(nproc)}"
mkdir -p "$COGI_DEPS/bin" "$COGI_DEPS/models"
command -v nvcc >/dev/null || export PATH="/usr/local/cuda/bin:$PATH"
command -v nvcc >/dev/null || { echo "ERROR: nvcc not found, run install-system.sh first"; exit 1; }

# shellcheck disable=SC1091
set +u; source /opt/ros/jazzy/setup.bash; set -u

# ---------------------------------------------------------------------------
echo ">> [1/6] Python venv: torch (CUDA 13.2), ultralytics"
VENV="$COGI_DEPS/venv"
if [ ! -x "$VENV/bin/pip" ]; then
    rm -rf "$VENV"
    python3 -m venv --system-site-packages "$VENV"
fi
# numpy < 2 because the apt ROS message libraries are built against 1.x.
echo "numpy<2" > "$COGI_DEPS/constraints.txt"
PIP="$VENV/bin/pip install -c $COGI_DEPS/constraints.txt"
$PIP --upgrade pip wheel
$PIP --pre torch torchvision --extra-index-url "$TORCH_INDEX"
$PIP ultralytics
[ -f "$COGI_DEPS/models/$YOLO_MODEL" ] || \
    (cd "$COGI_DEPS/models" && YOLO_CONFIG_DIR="$COGI_DEPS/ultralytics" "$VENV/bin/python" -c "from ultralytics import YOLO; YOLO('$YOLO_MODEL')")

# ---------------------------------------------------------------------------
echo ">> [2/6] Micro XRCE-DDS Agent $XRCE_AGENT_VERSION"
if [ ! -x "$COGI_DEPS/xrce/bin/MicroXRCEAgent" ]; then
    rm -rf "$COGI_DEPS/src/xrce"
    git clone --depth 1 -b "$XRCE_AGENT_VERSION" https://github.com/eProsima/Micro-XRCE-DDS-Agent.git "$COGI_DEPS/src/xrce"
    # Built WITHOUT ROS in the environment so its superbuild uses its own Fast DDS, not Jazzy's.
    env -i HOME="$HOME" PATH=/usr/bin:/bin bash -c "
        cmake -S '$COGI_DEPS/src/xrce' -B '$COGI_DEPS/src/xrce/build' -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_INSTALL_PREFIX='$COGI_DEPS/xrce' -DCMAKE_INSTALL_RPATH='$COGI_DEPS/xrce/lib' \
        && cmake --build '$COGI_DEPS/src/xrce/build' -j$JOBS \
        && cmake --install '$COGI_DEPS/src/xrce/build'"
fi
# The agent ships its own Fast-DDS; keep ROS's copy out of its library path.
cat > "$COGI_DEPS/bin/xrce-agent" <<EOF
#!/bin/sh
# udp4 for SITL (default), or: xrce-agent serial --dev /dev/ttyTHS1 -b 921600   (real Pixhawk on TELEM2)
if [ "\$#" -eq 0 ]; then set -- udp4 -p "\${XRCE_PORT:-8888}"; fi
exec env LD_LIBRARY_PATH="$COGI_DEPS/xrce/lib" "$COGI_DEPS/xrce/bin/MicroXRCEAgent" "\$@"
EOF
chmod +x "$COGI_DEPS/bin/xrce-agent"

# ---------------------------------------------------------------------------
echo ">> [3/6] px4_msgs $PX4_MSGS_BRANCH"
if [ ! -f "$COGI_DEPS/px4_ws/install/setup.bash" ]; then
    mkdir -p "$COGI_DEPS/px4_ws/src"
    [ -d "$COGI_DEPS/px4_ws/src/px4_msgs" ] || \
        git clone --depth 1 -b "$PX4_MSGS_BRANCH" https://github.com/PX4/px4_msgs.git "$COGI_DEPS/px4_ws/src/px4_msgs"
    # Plain /usr/bin/python3 for codegen: the venv's numpy must not leak into rosidl.
    (cd "$COGI_DEPS/px4_ws" && MAKEFLAGS=-j"$JOBS" colcon build --event-handlers console_cohesion- \
        --cmake-args -DCMAKE_BUILD_TYPE=Release -DPython3_EXECUTABLE=/usr/bin/python3)
fi

# ---------------------------------------------------------------------------
if [ "$ROLE" = sim ]; then
    echo ">> [4/6] PX4-Autopilot $PX4_VERSION (SITL)"
    PX4="$COGI_DEPS/PX4-Autopilot"
    [ -d "$PX4/.git" ] || git clone --depth 1 --recursive --shallow-submodules -j8 -b "$PX4_VERSION" \
        https://github.com/PX4/PX4-Autopilot.git "$PX4"
    $PIP -r "$PX4/Tools/setup/requirements.txt"
    # Our drone model and world live in the repo; PX4 looks for them in its own folders.
    ln -sfn "$REPO/sim/models/cogidrone_x500" "$PX4/Tools/simulation/gz/models/cogidrone_x500"
    ln -sfn "$REPO/sim/worlds/cogidrone.sdf" "$PX4/Tools/simulation/gz/worlds/cogidrone.sdf"
    if [ ! -x "$PX4/build/px4_sitl_default/bin/px4" ]; then
        # Built WITHOUT ROS sourced so it links against the system Gazebo, with the venv's python.
        env -i HOME="$HOME" PATH="$VENV/bin:/usr/bin:/bin" TERM=dumb bash -c "cd '$PX4' && make px4_sitl j=$JOBS"
    fi
    # QGroundControl. Start it with `qgc` (see scripts/jetson/qgc for why not the AppImage directly).
    [ -x "$COGI_DEPS/bin/QGroundControl.AppImage" ] || {
        curl -fL -o "$COGI_DEPS/bin/QGroundControl.AppImage" \
            "https://github.com/mavlink/qgroundcontrol/releases/download/$QGC_VERSION/QGroundControl-aarch64.AppImage"
        chmod +x "$COGI_DEPS/bin/QGroundControl.AppImage"
    }
    ln -sfn "$REPO/scripts/jetson/qgc" "$COGI_DEPS/bin/qgc"
    # People used as YOLO targets in the world (works offline after).
    for m in "Casual female" "Walking person"; do
        gz fuel download -u "https://fuel.gazebosim.org/1.0/OpenRobotics/models/${m}" \
            || echo "WARN: could not pre-download '${m}', Gazebo will fetch it at runtime"
    done
else
    echo ">> [4/6] skipping PX4 SITL (drone role)"
fi

# ---------------------------------------------------------------------------
if [ "${SKIP_CUVSLAM:-0}" = 1 ]; then
    echo ">> [5/6] skipping cuVSLAM (SKIP_CUVSLAM=1)"
else
echo ">> [5/6] cuVSLAM $CUVSLAM_VERSION (C++ lib + PyCuVSLAM), sm_$CUDA_ARCH"
CUV="$COGI_DEPS/cuVSLAM"
[ -d "$CUV/.git" ] || git clone --depth 1 -b "$CUVSLAM_VERSION" https://github.com/nvidia-isaac/cuVSLAM.git "$CUV"
(cd "$CUV" && git lfs install --local >/dev/null && git lfs pull) || echo "WARN: git lfs pull failed (only test data needs it)"
if [ ! -f "$CUV/build/bin/libcuvslam.so" ]; then
    env -i HOME="$HOME" PATH="/usr/local/cuda/bin:/usr/bin:/bin" bash -c "
        cmake -S '$CUV' -B '$CUV/build' -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_CUDA_ARCHITECTURES=$CUDA_ARCH -DCUDAToolkit_ROOT=/usr/local/cuda \
        && cmake --build '$CUV/build' --parallel $JOBS"
fi
CUVSLAM_BUILD_DIR="$CUV/build" $PIP "$CUV/python/"
fi

# ---------------------------------------------------------------------------
echo ">> [6/6] Shell setup"
LINE="source $REPO/scripts/jetson/env.sh"
grep -qxF "$LINE" "$HOME/.bashrc" || echo "$LINE" >> "$HOME/.bashrc"

echo
echo "Done ($ROLE). Open a new shell (or: $LINE), then:"
echo "  cb                                   # build ros2_ws"
echo "  scripts/jetson/smoke-test.sh $ROLE   # check that everything works"
