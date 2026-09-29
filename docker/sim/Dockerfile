# syntax=docker/dockerfile:1
#
# CogiDrone simulation / development image.
#
#   target "dev" : ROS 2 Jazzy + px4_msgs + YOLO (ultralytics, CPU).
#                  Builds natively on amd64 and arm64 (Apple Silicon).
#   target "sim" : dev + Gazebo Harmonic + PX4 SITL + Micro XRCE-DDS Agent.
#
# Versions are pinned here and nowhere else. PX4 and px4_msgs MUST match.

ARG ROS_DISTRO=jazzy

# ---------------------------------------------------------------------------
FROM ros:${ROS_DISTRO}-ros-base AS dev

ARG ROS_DISTRO
ARG PX4_MSGS_BRANCH=release/1.17
ARG YOLO_MODEL=yolo11n.pt

ENV DEBIAN_FRONTEND=noninteractive \
    ROS_DISTRO=${ROS_DISTRO} \
    PYTHONUNBUFFERED=1

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        curl \
        wget \
        nano \
        python3-pip \
        python3-venv \
        python3-colcon-common-extensions \
        libgl1 \
        libglib2.0-0 \
        ros-${ROS_DISTRO}-vision-msgs \
        ros-${ROS_DISTRO}-rosbag2-storage-mcap \
    && rm -rf /var/lib/apt/lists/*

# Python deps live in a venv that can still see the ROS (apt) packages.
# It is added to PYTHONPATH so ROS nodes run by /usr/bin/python3 find them.
# numpy is kept < 2 because the ROS message libraries are built against 1.x.
RUN python3 -m venv --system-site-packages /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir \
        --index-url https://download.pytorch.org/whl/cpu torch torchvision \
    && /opt/venv/bin/pip install --no-cache-dir "numpy<2" ultralytics
ENV PYTHONPATH=/opt/venv/lib/python3.12/site-packages \
    PATH=${PATH}:/opt/venv/bin \
    YOLO_CONFIG_DIR=/tmp/Ultralytics

# Bake the default YOLO weights into the image so it works offline.
RUN mkdir -p /opt/models && cd /opt/models \
    && python3 -c "from ultralytics import YOLO; YOLO('${YOLO_MODEL}')"

# px4_msgs: the ROS 2 message definitions for talking to PX4.
RUN mkdir -p /opt/px4_ws/src \
    && git clone --depth 1 -b ${PX4_MSGS_BRANCH} https://github.com/PX4/px4_msgs.git /opt/px4_ws/src/px4_msgs \
    && source /opt/ros/${ROS_DISTRO}/setup.bash \
    && cd /opt/px4_ws \
    && MAKEFLAGS=-j4 colcon build --executor sequential --event-handlers console_cohesion- \
    && rm -rf build log

COPY docker/sim/env.sh /opt/cogidrone/env.sh
COPY docker/sim/entrypoint.sh /opt/cogidrone/entrypoint.sh
RUN chmod +x /opt/cogidrone/entrypoint.sh \
    && echo "source /opt/cogidrone/env.sh" >> /root/.bashrc

WORKDIR /workspace
ENTRYPOINT ["/opt/cogidrone/entrypoint.sh"]
CMD ["bash"]

# ---------------------------------------------------------------------------
FROM dev AS sim

ARG ROS_DISTRO
ARG PX4_VERSION=v1.17.0
ARG XRCE_AGENT_VERSION=v2.4.3
# Parallel compile jobs. Each needs ~1 GB RAM; raise it if Docker has more memory.
ARG BUILD_JOBS=4

# Gazebo Harmonic (the version PX4 and ROS 2 Jazzy both target).
RUN apt-get update && apt-get install -y --no-install-recommends lsb-release gnupg \
    && wget -q https://packages.osrfoundation.org/gazebo.gpg -O /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/gazebo-stable.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        gz-harmonic \
        libunwind-dev \
        cppzmq-dev \
        ros-${ROS_DISTRO}-ros-gz-bridge \
        ros-${ROS_DISTRO}-rqt-image-view \
        # PX4 SITL build deps (mirrors Tools/setup/ubuntu.sh --no-nuttx)
        astyle ccache file gdb libssl-dev libxml2-dev libxml2-utils make ninja-build \
        python3-dev python3-setuptools python3-wheel rsync unzip zip \
        libeigen3-dev libopencv-dev pkg-config protobuf-compiler \
        libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad gstreamer1.0-libav \
    && rm -rf /var/lib/apt/lists/*

# PX4 firmware, built once for SITL. Nobody has to compile PX4 by hand.
# Built WITHOUT ROS sourced so it links against the system Gazebo.
RUN git clone --depth 1 --recursive --shallow-submodules -b ${PX4_VERSION} \
        https://github.com/PX4/PX4-Autopilot.git /opt/PX4-Autopilot \
    && /opt/venv/bin/pip install --no-cache-dir "numpy<2" -r /opt/PX4-Autopilot/Tools/setup/requirements.txt \
    && cd /opt/PX4-Autopilot \
    && make px4_sitl j=${BUILD_JOBS}

# Micro XRCE-DDS Agent: bridges PX4 (uORB) <-> ROS 2 (DDS) over UDP 8888.
RUN git clone --depth 1 -b ${XRCE_AGENT_VERSION} https://github.com/eProsima/Micro-XRCE-DDS-Agent.git /tmp/xrce \
    && cmake -S /tmp/xrce -B /tmp/xrce/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/xrce/build -j"${BUILD_JOBS}" \
    && cmake --install /tmp/xrce/build \
    && ldconfig \
    && rm -rf /tmp/xrce
# The agent ships its own Fast-DDS; keep ROS's copy out of its library path.
RUN printf '#!/bin/sh\nexec env -u LD_LIBRARY_PATH MicroXRCEAgent udp4 -p "${XRCE_PORT:-8888}" "$@"\n' \
        > /usr/local/bin/xrce-agent \
    && chmod +x /usr/local/bin/xrce-agent

# Our drone model and world live in the repo (mounted at /workspace) so they
# can be edited without rebuilding; PX4 looks for them in its own folders.
RUN ln -s /workspace/sim/models/cogidrone_x500 /opt/PX4-Autopilot/Tools/simulation/gz/models/cogidrone_x500 \
    && ln -s /workspace/sim/worlds/cogidrone.sdf /opt/PX4-Autopilot/Tools/simulation/gz/worlds/cogidrone.sdf

# Pre-fetch the people used as YOLO targets in the world (works offline after).
RUN for m in "Casual female" "Walking person"; do \
        gz fuel download -u "https://fuel.gazebosim.org/1.0/OpenRobotics/models/${m}" \
        || echo "WARN: could not pre-download '${m}', Gazebo will fetch it at runtime"; \
    done

ENV PX4_DIR=/opt/PX4-Autopilot \
    QT_X11_NO_MITSHM=1
