#!/usr/bin/env bash
# One entry point for the CogiDrone sim container. Run from WSL (Windows), Linux or macOS.
#
#   ./scripts/sim.sh build   build the image (first time takes 20-40 min: PX4 is compiled)
#   ./scripts/sim.sh up      start the container in the background
#   ./scripts/sim.sh shell   open a shell inside it (run as many as you like)
#   ./scripts/sim.sh down    stop it
#
# Windows/Linux get the "sim" service (Gazebo + PX4). macOS gets "dev" (no simulator).
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

if ! command -v docker >/dev/null 2>&1 || ! docker info >/dev/null 2>&1; then
    echo "ERROR: Docker is not installed or not running." >&2
    exit 1
fi

if [ "$(uname -s)" = "Darwin" ]; then
    SERVICE=dev
else
    SERVICE=sim
fi
SERVICE="${COGIDRONE_SERVICE:-$SERVICE}"

start_x11_relay() {
    if ! pgrep -f x11-relay.py >/dev/null; then
        nohup python3 scripts/x11-relay.py >"$HOME/.cogidrone-x11-relay.log" 2>&1 &
        sleep 1
    fi
}

# Tell compose where the host's display lives, via .env (also used by the devcontainer).
write_display_env() {
    local x11 wslg display wayland runtime
    if grep -qi microsoft /proc/version 2>/dev/null; then
        # WSL2 + WSLg. Docker Desktop can't mount /mnt/wslg, so relay the X socket
        # through a normal WSL folder (scripts/x11-relay.py). Docker Engine in WSL can.
        if docker info 2>/dev/null | grep -q "Operating System: Docker Desktop"; then
            start_x11_relay
            x11="$HOME/.cogidrone/X11-unix" wslg=/tmp/cogidrone-no-wslg
            display=:0 wayland="" runtime=/tmp/runtime-root
        else
            x11=/mnt/wslg/.X11-unix wslg=/mnt/wslg
            display=:0 wayland=wayland-0 runtime=/mnt/wslg/runtime-dir
        fi
    elif [ "$(uname -s)" = "Linux" ]; then
        x11=/tmp/.X11-unix wslg=/tmp/cogidrone-no-wslg display="${DISPLAY:-:0}" wayland="" runtime=/tmp/runtime-root
        command -v xhost >/dev/null 2>&1 && xhost +local: >/dev/null 2>&1 || true
    else
        return 0
    fi
    cat > .env <<EOF
# Written by scripts/sim.sh. Safe to delete; it is regenerated.
X11_SOCKET_DIR=$x11
WSLG_DIR=$wslg
DISPLAY=$display
WAYLAND_DISPLAY=$wayland
SIM_XDG_RUNTIME_DIR=$runtime
EOF
}

case "${1:-help}" in
    build)
        write_display_env
        docker compose build "$SERVICE"
        ;;
    up)
        write_display_env
        docker compose up -d "$SERVICE"
        echo "Started '$SERVICE'. Open a shell with: ./scripts/sim.sh shell"
        ;;
    shell)
        docker compose exec "$SERVICE" bash
        ;;
    down)
        docker compose down
        pkill -f x11-relay.py 2>/dev/null || true
        ;;
    *)
        sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
        ;;
esac
