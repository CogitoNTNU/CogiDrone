#!/usr/bin/env bash
# Checks that the native Jetson setup works, piece by piece, then (sim role) flies a full SITL takeoff.
#
#   scripts/jetson/smoke-test.sh drone   # CUDA, TensorRT, torch, YOLO on GPU, cuVSLAM, ROS + px4_msgs, XRCE agent
#   scripts/jetson/smoke-test.sh sim     # all of the above + PX4 SITL/Gazebo headless takeoff to 3 m
set -uo pipefail

ROLE="${1:-sim}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
set +u; source "$HERE/env.sh"; set -u
LOG="$(mktemp -d /tmp/cogidrone-smoke.XXXX)"
PASS=0; FAIL=0; RESULTS=()

check() {   # $1=name, rest=command
    local name="$1"; shift
    local log="$LOG/$(echo "$name" | tr -c 'A-Za-z0-9\n' '_').log"
    printf '  %-44s' "$name"
    if "$@" >"$log" 2>&1; then
        echo "PASS"; PASS=$((PASS+1)); RESULTS+=("PASS  $name")
    else
        echo "FAIL  (log: $log)"; FAIL=$((FAIL+1)); RESULTS+=("FAIL  $name")
    fi
}

echo "== Toolchain / GPU"
check "nvcc (CUDA 13.2)"            bash -c 'nvcc --version | grep -q "release 13.2"'
check "torch sees the GPU"          python3 -c '
import torch; assert torch.cuda.is_available(), "no CUDA"
a = torch.randn(2048, 2048, device="cuda"); (a @ a).sum().item()
print(torch.__version__, torch.cuda.get_device_name(0))'
check "TensorRT python"             python3 -c 'import tensorrt; print(tensorrt.__version__)'
check "YOLO inference on GPU"       python3 -c '
import os, numpy as np
from ultralytics import YOLO
m = YOLO(os.environ["COGIDRONE_YOLO_MODEL"])
r = m(np.zeros((480, 640, 3), np.uint8), device=os.environ["COGIDRONE_YOLO_DEVICE"], verbose=False)
print("ok", r[0].speed)'
check "cuVSLAM (PyCuVSLAM) loads"   python3 -c 'import cuvslam; print(cuvslam.get_version())'

echo "== ROS 2 / PX4 link"
check "ROS 2 Jazzy + px4_msgs"      ros2 interface show px4_msgs/msg/VehicleStatus
check "colcon build ros2_ws"        bash -c "cd '$COGIDRONE_ROOT/ros2_ws' && colcon build --symlink-install"
check "XRCE agent starts (udp 8888)" bash -c '
xrce-agent udp4 -p 8888 & pid=$!; sleep 2
ss -ulpn | grep -q ":8888 "; ok=$?; kill $pid; exit $ok'
check "C++ core builds (src)"       bash -c "cmake -S '$COGIDRONE_ROOT' -B '$LOG/cpp' && cmake --build '$LOG/cpp' -j\$(nproc)"

if [ "$ROLE" = sim ]; then
    echo "== Full SITL: PX4 + Gazebo (headless) + XRCE + YOLO + offboard takeoff"
    set +u; source "$COGIDRONE_ROOT/ros2_ws/install/setup.bash"; set -u
    # Own process group (set -m), so cleanup reaches the launch and everything it started.
    set -m
    ros2 launch cogidrone_bringup sim.launch.py headless:=true >"$LOG/sim_launch.log" 2>&1 &
    SIM=$!
    set +m
    # SIGINT first (= Ctrl-C). If this script was started with SIGINT ignored (e.g. from another
    # background job), every child inherits that, so follow up with SIGTERM, then SIGKILL.
    stop_sim() {
        local sig
        for sig in INT TERM KILL; do
            kill -"$sig" -- -"$SIM" 2>/dev/null || return 0
            for _ in $(seq 1 15); do kill -0 -- -"$SIM" 2>/dev/null || return 0; sleep 1; done
        done
    }
    trap stop_sim EXIT
    # `ros2 topic echo` exits at once if the topic doesn't exist yet, so wait for PX4 to come up first.
    check "PX4 topics over DDS"     bash -c 'for i in $(seq 1 45); do ros2 topic list | grep -q /fmu/out/vehicle_status && break; sleep 2; done
timeout 30 ros2 topic echo --once /fmu/out/vehicle_status_v1 >/dev/null'
    check "Gazebo camera -> ROS"    bash -c 'timeout 60 ros2 topic echo --once --no-arr /camera/camera/color/image_raw >/dev/null'
    check "YOLO detections"         bash -c 'timeout 60 ros2 topic echo --once --no-arr /cogidrone/detections >/dev/null'
    # Offboard controller arms after the GPS fix and climbs to 3 m (NED: z < -2.5).
    check "takeoff to ~3 m"         bash -c '
for i in $(seq 1 24); do
  z=$(timeout 10 ros2 topic echo --once --field z /fmu/out/vehicle_local_position_v1 2>/dev/null | head -1)
  echo "t=$((i*5))s z=$z"
  python3 -c "import sys; sys.exit(0 if float(\"${z:-0}\") < -2.5 else 1)" 2>/dev/null && exit 0
  sleep 5
done; exit 1'
fi

echo
echo "== $PASS passed, $FAIL failed   (logs: $LOG)"
printf '%s\n' "${RESULTS[@]}" > "$LOG/summary.txt"
[ "$FAIL" -eq 0 ]
