#!/usr/bin/env bash
# * CogiDrone - Jetson native build
# ? Runs ON the Jetson. Builds the C++ binary and, when needed, the TensorRT
# ? engine plans from the ONNX exports.
# ?
# ! NOT the cross-compile path. The `linux-arm64` preset is for building on a
# ! workstation to catch compile errors. This script assumes aarch64 + JetPack,
# ! because TensorRT and CUDA are only present there.
set -euo pipefail


# * Output
# ? printf, not echo -e: `echo -e` is a bashism that silently prints the escape
# ? codes literally under sh/dash.
RESET='\033[0m'
BOLD='\033[1m'
DIM='\033[2m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD_RED='\033[1;91m'
BOLD_GREEN='\033[1;92m'
BOLD_YELLOW='\033[1;93m'
BOLD_CYAN='\033[1;96m'

step()  { printf "\n${BOLD_CYAN}==> %s${RESET}\n" "$*"; }
info()  { printf "${CYAN}    %s${RESET}\n" "$*"; }
ok()    { printf "${GREEN}    ✓ %s${RESET}\n" "$*"; }
warn()  { printf "${YELLOW}    ! %s${RESET}\n" "$*"; }
fail()  { printf "\n${BOLD_RED}✗ %s${RESET}\n" "$*" >&2; exit 1; }


# * Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build/jetson"
BINARY="$BUILD_DIR/src/cogidrone"
MODELS_DIR="models"

# ? The two models the follow mission needs. DepthAnything is deferred ([#40]).
PERSON_ONNX="$MODELS_DIR/best.onnx"
PERSON_ENGINE="$MODELS_DIR/best.engine"
HEAD_ONNX="$MODELS_DIR/medium.onnx"
HEAD_ENGINE="$MODELS_DIR/medium.engine"


# * Flags
BUILD_ENGINES=0
CLEAN=0
JOBS="$(nproc)"

for arg in "$@"; do
    case "$arg" in
        --engines) BUILD_ENGINES=1 ;;
        --clean)   CLEAN=1 ;;
        -j*)       JOBS="${arg#-j}" ;;
        -h|--help)
            printf "${BOLD}CogiDrone - Jetson native build${RESET}\n\n"
            printf "  ${CYAN}--engines${RESET}  Rebuild the TensorRT .engine plans from .onnx\n"
            printf "  ${CYAN}--clean${RESET}    Delete $BUILD_DIR before configuring\n"
            printf "  ${CYAN}-jN${RESET}       Build with N jobs (default: nproc)\n"
            exit 0
            ;;
        *) fail "Unknown argument: $arg" ;;
    esac
done


# * 1. Preflight
step "Preflight"

[[ "$(uname -m)" == "aarch64" ]] \
    || fail "Not aarch64. This script is Jetson-only - use the linux-arm64 preset to cross-compile."

if [[ -f /etc/nv_tegra_release ]] || [[ -d /usr/src/tensorrt ]]; then
    ok "Jetson detected"
else
    warn "No Tegra marker found - continuing anyway"
fi

command -v cmake >/dev/null || fail "cmake not found"
command -v ninja >/dev/null || fail "ninja not found (sudo apt install ninja-build)"
ok "cmake $(cmake --version | head -n1 | awk '{print $3}')"
ok "ninja $(ninja --version)"

# ? TensorRT ships with JetPack, but trtexec is not always on PATH.
TRTEXEC="$(command -v trtexec || true)"
if [[ -z "$TRTEXEC" && -x /usr/src/tensorrt/bin/trtexec ]]; then
    TRTEXEC=/usr/src/tensorrt/bin/trtexec
fi

if [[ -n "$TRTEXEC" ]]; then
    ok "trtexec: $TRTEXEC"
else
    warn "trtexec not found - engine rebuilds will be skipped"
fi

# ? nvinfer is the inference runtime. Its absence means the CMake configure
# ? will fail, so catch it here with a readable message.
if ldconfig -p | grep libnvinfer.so >/dev/null; then
    ok "TensorRT runtime present"
else
    fail "libnvinfer.so not found. Is JetPack installed correctly?"
fi

if pkg-config --exists opencv4 2>/dev/null; then
    ok "OpenCV $(pkg-config --modversion opencv4)"
else
    warn "opencv4 pkg-config not found - CMake may still find it via find_package"
fi


# * 2. TensorRT engines
# ? An .engine is a serialized plan for ONE GPU architecture and ONE TensorRT
# ? version. It cannot be shipped from a workstation - it must be built here.
# ? The .onnx is portable and can be committed; the .engine should not be.
step "TensorRT engines"

build_engine() {
    local onnx="$1"
    local engine="$2"
    local name="$3"

    if [[ -f "$engine" && "$BUILD_ENGINES" -eq 0 ]]; then
        ok "$name.engine up to date ${DIM}(use --engines to rebuild)${RESET}"
        return
    fi

    if [[ ! -f "$onnx" ]]; then
        warn "$name.onnx missing - skipping $name.engine"
        info "Export it on a workstation:"
        info "  yolo export model=$MODELS_DIR/$name.pt format=onnx imgsz=640 nms=False"
        return
    fi

    if [[ -z "$TRTEXEC" ]]; then
        warn "trtexec unavailable - skipping $name.engine"
        return
    fi

    info "Building $name.engine (FP16)..."
    # ? --memPoolSize replaces the deprecated --workspace in TensorRT 10+.
    # ? 2 GiB is generous for a 640x640 YOLO; the actual peak is far lower.
    local LOG="/tmp/trtexec-$name.log"
    if ! "$TRTEXEC" \
        --onnx="$onnx" \
        --saveEngine="$engine" \
        --fp16 \
        --memPoolSize=workspace:2048MiB \
        >"$LOG" 2>&1; then
        fail "trtexec failed for $name. Log: $LOG (tail: $(tail -n 3 "$LOG" | tr '\n' ' '))"
    fi

    ok "$name.engine built"
}

build_engine "$PERSON_ONNX" "$PERSON_ENGINE" "best"
build_engine "$HEAD_ONNX"   "$HEAD_ENGINE"   "medium"


# * 3. Configure
step "Configure"

if [[ "$CLEAN" -eq 1 ]]; then
    info "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

cmake --preset jetson-native || fail "CMake configure failed"
ok "Configured in $BUILD_DIR"


# * 4. Build
step "Build"

cmake --build "$BUILD_DIR" -j "$JOBS" || fail "Build failed"
ok "Built $BINARY"


# * 5. Report
step "Done"

if [[ -x "$BINARY" ]]; then
    printf "${BOLD_GREEN}    Binary: ${RESET}${BOLD}%s${RESET}\n" "$BINARY"
    printf "${DIM}    %s${RESET}\n" "$(du -h "$BINARY" | cut -f1)"
else
    warn "Binary not found at $BINARY - check the CMake target name"
fi

printf "\n${DIM}    Run:${RESET} ${CYAN}./%s${RESET}\n\n" "$BINARY"
