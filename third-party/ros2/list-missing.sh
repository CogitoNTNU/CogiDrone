#!/usr/bin/env bash

cd "$(dirname "$0")"   # third-party/ros2

# list every quoted include in the ROS headers, keep only package-style ones
grep -rhoE '#include "[a-z0-9_]+/[a-z_]+' opt/ros/jazzy/include | sort -u | \
  sed 's/#include "//' | cut -d/ -f1 | grep -v '^detail$' | grep -v '^fastdds$' | sort -u | \
  
while read pkg; do
  # search both the ROS tree and the sysroot (usr/include)
  [ -n "$(find opt usr -type d -name "$pkg" -print -quit 2>/dev/null)" ] || echo "MISSING: $pkg"

done