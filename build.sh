#!/usr/bin/env bash

set -e

echo "=== CogiDrone ARM64 Build ==="
echo

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$SCRIPT_DIR/toolchain.config"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "ERROR: toolchain.config not found."
    echo
    echo "Copy toolchain.config.example to toolchain.config"
    echo "and set ARM_GCC_ROOT to your Arm GNU Toolchain installation."
    exit 1
fi

# Load configuration
source "$CONFIG_FILE"

if [ -z "$ARM_GCC_ROOT" ]; then
    echo "ERROR: ARM_GCC_ROOT is not set in toolchain.config."
    exit 1
fi

if [ ! -x "$ARM_GCC_ROOT/bin/aarch64-none-linux-gnu-g++" ]; then
    echo "ERROR: ARM GCC compiler not found:"
    echo "  $ARM_GCC_ROOT/bin/aarch64-none-linux-gnu-g++"
    echo
    echo "Check ARM_GCC_ROOT in toolchain.config."
    exit 1
fi

echo "Toolchain:"
echo "  $ARM_GCC_ROOT"
echo

# Configure
cmake --preset linux-arm64

# Build
cmake --build build/linux-arm64

echo
echo "=== Build successful ==="
