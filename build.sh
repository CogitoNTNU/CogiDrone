#!/usr/bin/env bash
set -euo pipefail

# * Configure
cmake --preset linux-arm64

# * Build
cmake --build build/linux-arm64

# Output
echo \e[1;92m Build successful \e[0m
