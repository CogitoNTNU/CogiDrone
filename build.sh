#!/usr/bin/env bash
set -euo pipefail

echo "=== CogiDrone ARM64 Docker Build ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="cogidrone-arm64-build"

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: Docker is not installed or is not available in PATH."
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "ERROR: Docker is not running."
    exit 1
fi

echo "Building Docker image..."
docker build \
    --tag "$IMAGE_NAME" \
    "$SCRIPT_DIR/docker"

echo "Configuring and building..."
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "$SCRIPT_DIR:/workspace" \
    --workdir /workspace \
    "$IMAGE_NAME" \
    bash -c '
        cmake --preset linux-arm64
        cmake --build build/linux-arm64
    '

echo
echo "=== Build successful ==="