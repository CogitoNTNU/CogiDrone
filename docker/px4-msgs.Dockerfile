# One-shot arm64 build of `px4_msgs`.
#
# `px4_msgs` has no deb on packages.ros.org - it is only .msg source, which must
# go through rosidl codegen. So unlike every other ROS package in this project
# (see third-party/ros2/get.sh), it cannot simply be downloaded and extracted.
#
# This image runs that codegen once, natively for arm64, and the `export` stage
# carries only the install tree out via `buildx -o type=local`. Driven by
# third-party/ros2/get-px4-msgs.sh - not by build.sh.

FROM ros:jazzy-ros-base AS build

# ! Must match the uORB message set in the PX4 firmware flashed on the airframe.
# ! A mismatch does not error - topics silently never connect, or fields shift.
ARG PX4_MSGS_REF=release/1.15

RUN apt-get update && \
    apt-get install -y \
        git \
        python3-colcon-common-extensions && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /ws/src
RUN git clone --depth 1 --branch "${PX4_MSGS_REF}" https://github.com/PX4/px4_msgs.git

WORKDIR /ws
RUN . /opt/ros/jazzy/setup.sh && \
    colcon build --packages-select px4_msgs \
        --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF

# Artifacts only: `buildx -o type=local` exports this stage's filesystem, so a
# scratch base keeps the export to just what we vendor.
FROM scratch AS export
COPY --from=build /ws/install/px4_msgs/ /px4_msgs/
