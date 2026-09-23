#!/usr/bin/env bash
set -euo pipefail

# HTTPS bypasses the campus HTTP cache (which served a corrupt deb).
ROS_BASE="https://packages.ros.org/ros2/ubuntu"
UB_BASE="https://ports.ubuntu.com/ubuntu-ports"
DEST="/c/Users/reale/Code/C++/CogiDrone/third-party/ros2"
CURL="curl.exe"
K="-k"                                                                                  # packages.ros.org origin presents a *.osuosl.org cert (known mismatch);
                                                                                        # sha256 checks below catch any actual tampering/corruption

mkdir -p "$DEST/debs" && cd "$DEST/debs"

get_index() {                                                                           # $1=base $2=name $3=component
  "$CURL" $K -fL "$1/dists/noble/$3/binary-arm64/Packages.gz" -o "$2.gz" \
    || "$CURL" $K -fL "$1/dists/noble/$3/binary-arm64/Packages.xz" -o "$2.xz"
  [ -f "$2.gz" ] && gzip -dkf "$2.gz" || xz -dkf "$2.xz"
}

pkgfile() {                                                                             # $1=index $2=package -> prints pool path
  awk -v pkg="$2" '
    /^Package:/ { p=$2 }
    p==pkg && /^Filename:/ { print $2; found=1; exit }
    END { if (!found) exit 1 }
  ' "$1"
}

fetch() {                                                                               # $1=base $2=index $3=package
  local f
  f=$(pkgfile "$2" "$3") || { echo "!! not in index: $3"; return 0; }
  local deb; deb=$(basename "$f")

  local want got
  want=$(awk -v pkg="$3" '/^Package: /{p=$2} p==pkg && /^SHA256:/{print $2; exit}' "$2")

  # use the cached deb if it verifies; otherwise download
  got=""
  if [ -f "$deb" ]; then got=$(sha256sum "$deb" | awk '{print $1}'); fi
  if [ "$got" != "$want" ]; then
    echo ">> $3"
    "$CURL" $K -fLO "$1/$f"
    got=$(sha256sum "$deb" | awk '{print $1}')
    if [ "$want" != "$got" ]; then
      echo "!! sha256 MISMATCH: $3 - bad copy, skipping"
      rm -f "$deb"
      return 0
    fi
  else
    echo ">> $3 (cached)"
  fi

  local member; member=$(ar t "$deb" | grep '^data\.tar')

  # Two-step: `ar x` writes the member to a real file (binary-safe).
  ar x "$deb" "$member"

  # MSYS2 cannot create POSIX symlinks: tar falls back to copying the
  # target, but the target may appear later in the archive. Extract
  # twice - pass 1 lays down real files (symlink errors tolerated),
  # pass 2 resolves the symlinks now that their targets exist.
  case "$member" in
    *.zst) tar --zstd -xf "$member" -C "$DEST" 2>/dev/null || true
           tar --zstd -xf "$member" -C "$DEST" 2>/dev/null || true ;;
    *.xz)  tar -Jxf       "$member" -C "$DEST" 2>/dev/null || true
           tar -Jxf       "$member" -C "$DEST" 2>/dev/null || true ;;
    *)     tar -xf        "$member" -C "$DEST" 2>/dev/null || true
           tar -xf        "$member" -C "$DEST" 2>/dev/null || true ;;
  esac
  rm -f "$member"
}

get_index "$ROS_BASE" ros-Packages main
get_index "$UB_BASE"  ub-Packages main
get_index "$UB_BASE"  ub-Packages-universe universe
cat ub-Packages-universe >> ub-Packages        # merge; awk takes the first match per package

# --- ROS 2 Jazzy: core + message packages (headers for IntelliSense) ---
ROS_PKGS=(
  ros-jazzy-rclcpp ros-jazzy-rcl ros-jazzy-rcl-interfaces
  ros-jazzy-service-msgs ros-jazzy-type-description-interfaces
  ros-jazzy-rcl-yaml-param-parser
  ros-jazzy-rcutils ros-jazzy-rcpputils
  ros-jazzy-rmw ros-jazzy-rmw-implementation ros-jazzy-rmw-fastrtps-cpp
  ros-jazzy-rmw-fastrtps-shared-cpp ros-jazzy-rmw-dds-common
  ros-jazzy-rosidl-runtime-c ros-jazzy-rosidl-runtime-cpp
  ros-jazzy-rosidl-typesupport-interface
  ros-jazzy-rosidl-typesupport-c ros-jazzy-rosidl-typesupport-cpp
  ros-jazzy-rosidl-typesupport-fastrtps-c ros-jazzy-rosidl-typesupport-fastrtps-cpp
  ros-jazzy-rosidl-typesupport-introspection-c
  ros-jazzy-rosidl-typesupport-introspection-cpp
  ros-jazzy-rosidl-dynamic-typesupport
  ros-jazzy-builtin-interfaces ros-jazzy-std-msgs ros-jazzy-sensor-msgs
  ros-jazzy-geometry-msgs ros-jazzy-nav-msgs
  ros-jazzy-rosgraph-msgs ros-jazzy-statistics-msgs
  ros-jazzy-action-msgs ros-jazzy-unique-identifier-msgs
  ros-jazzy-tracetools ros-jazzy-libstatistics-collector
  ros-jazzy-ament-index-cpp ros-jazzy-class-loader
  ros-jazzy-console-bridge-vendor
  ros-jazzy-fastcdr ros-jazzy-fastrtps
)

# ! NOT NEEDED: the cross-toolchain supplies its own 
# !             libstdc++/libc headers (see CMake toolchain file)
# --- Ubuntu noble arm64: minimal sysroot so std:: resolves ---
# // UB_PKGS=(libgcc-13-dev libstdc++-13-dev libc6-dev linux-libc-dev libconsole-bridge-dev)

for p in "${ROS_PKGS[@]}"; do fetch "$ROS_BASE" ros-Packages "$p"; done
for p in "${UB_PKGS[@]}";  do fetch "$UB_BASE"  ub-Packages  "$p"; done

echo "Done. Headers at $DEST/opt/ros/jazzy/include"