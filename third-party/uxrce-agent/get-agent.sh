#!/usr/bin/env bash
set -euo pipefail

# Vendors the Micro XRCE-DDS Agent as an arm64 binary for the Jetson.
#
# Same idea as third-party/ros2/get-px4-msgs.sh: build once in Docker, commit the
# artifacts, keep build tooling off the target. The agent is a standalone process
# with no ROS dependency, so it gets its own bin/ and lib/ rather than joining the
# vendored ROS tree - see AGENT.md for why that separation is load-bearing.

DEST="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"                                    # third-party/uxrce-agent
REPO="$(cd "$DEST/../.." && pwd)"

AGENT_REF="${AGENT_REF:-v2.4.3}"                                                        # ! must stay on v2.x - see AGENT.md

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: Docker is not installed or is not available in PATH."
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "ERROR: Docker is not running."
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo ">> building Micro XRCE-DDS Agent ($AGENT_REF) for linux/arm64"
docker buildx build \
    --platform linux/arm64 \
    --target export \
    --build-arg "AGENT_REF=$AGENT_REF" \
    --file "$REPO/docker/uxrce-agent.Dockerfile" \
    --output "type=local,dest=$TMP" \
    "$REPO"

SRC="$TMP/stage"
[ -x "$SRC/bin/MicroXRCEAgent" ] || { echo "!! export produced no MicroXRCEAgent"; exit 1; }

echo ">> vendoring into $DEST"
rm -rf "$DEST/bin" "$DEST/lib"
mkdir -p "$DEST/bin" "$DEST/lib"

cp "$SRC/bin/MicroXRCEAgent" "$DEST/bin/"
chmod +x "$DEST/bin/MicroXRCEAgent"

# The superbuild installs Fast DDS / Fast CDR into the same prefix. Copy the
# shared objects and their SONAME symlinks, skipping static archives and cmake
# config dirs - the agent resolves these via LD_LIBRARY_PATH at runtime.
find "$SRC/lib" -maxdepth 1 \( -name '*.so' -o -name '*.so.*' \) -exec cp -a {} "$DEST/lib/" \;

echo
echo "Done. Micro XRCE-DDS Agent $AGENT_REF vendored:"
echo "  binary : $DEST/bin/MicroXRCEAgent ($(du -h "$DEST/bin/MicroXRCEAgent" | cut -f1))"
echo "  libs   : $(find "$DEST/lib" -name '*.so*' -type f | wc -l | tr -d ' ') files, $(du -sh "$DEST/lib" | cut -f1)"
echo
echo "Dependencies the executable reported at build time:"
sed 's/^/  /' "$SRC/ldd.txt" 2>/dev/null || echo "  (ldd.txt not produced)"
