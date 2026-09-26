#!/usr/bin/env bash
set -euo pipefail

# Starts the uXRCE-DDS agent: the bridge between PX4's uORB topics on the Pixhawk
# and the DDS network cogidrone talks to. Without this running, /fmu/out/* and
# /fmu/in/* do not exist and cogidrone silently sees nothing.
#
# Defaults to the serial link (Jetson <-> Pixhawk TELEM2). Override per run:
#   UXRCE_TRANSPORT=udp4 scripts/run-agent.sh          # local / SITL work
#   UXRCE_DEV=/dev/ttyUSB0 scripts/run-agent.sh        # different UART
#
# See third-party/uxrce-agent/AGENT.md for the matching PX4-side parameters -
# the agent alone is not enough, the flight controller must be told to speak.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AGENT_ROOT="$HERE/../third-party/uxrce-agent"

UXRCE_TRANSPORT="${UXRCE_TRANSPORT:-serial}"
UXRCE_DEV="${UXRCE_DEV:-/dev/ttyTHS1}"                                                  # Jetson TELEM2 UART; carrier-board dependent
UXRCE_BAUD="${UXRCE_BAUD:-921600}"                                                      # ! must match SER_TEL2_BAUD on the Pixhawk
UXRCE_PORT="${UXRCE_PORT:-8888}"                                                        # udp4 only

AGENT="$AGENT_ROOT/bin/MicroXRCEAgent"
[ -x "$AGENT" ] || {
    echo "ERROR: $AGENT not found or not executable."
    echo "       Run third-party/uxrce-agent/get-agent.sh to build and vendor it."
    exit 1
}

# ! The agent's own Fast DDS is 2.14.7 while the vendored ROS tree ships 2.14.6 -
# ! and both carry the SONAME libfastrtps.so.2.14, so they are interchangeable to
# ! the loader but are not the same build. Point the agent at its own lib dir ONLY,
# ! and never merge the two directories or put the ROS tree on this path.
export LD_LIBRARY_PATH="$AGENT_ROOT/lib"

case "$UXRCE_TRANSPORT" in
    serial)
        [ -e "$UXRCE_DEV" ] || echo "WARNING: $UXRCE_DEV does not exist - is the Pixhawk wired and powered?" >&2
        echo ">> agent: serial $UXRCE_DEV @ $UXRCE_BAUD"
        exec "$AGENT" serial --dev "$UXRCE_DEV" -b "$UXRCE_BAUD" "$@"
        ;;
    udp4)
        echo ">> agent: udp4 port $UXRCE_PORT"
        exec "$AGENT" udp4 -p "$UXRCE_PORT" "$@"
        ;;
    *)
        echo "ERROR: UXRCE_TRANSPORT must be 'serial' or 'udp4', got '$UXRCE_TRANSPORT'."
        echo "       PX4 supports only these two - see AGENT.md."
        exit 1
        ;;
esac
