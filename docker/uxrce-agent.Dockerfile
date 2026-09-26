# One-shot arm64 build of the eProsima Micro XRCE-DDS Agent.
#
# The agent is the companion-computer half of PX4's uXRCE-DDS bridge: it proxies
# the client running on the Pixhawk into the DDS/ROS 2 network. It has no ROS
# dependency and is not linked into cogidrone - it is a separate process.
#
# Driven by third-party/uxrce-agent/get-agent.sh, not by build.sh.

FROM ubuntu:24.04 AS build

# ! HARD CONSTRAINT, not a preference. PX4's bundled uXRCE-DDS *client* is based
# ! on Micro XRCE-DDS Client v2.x, and the PX4 docs state it "is not compatible
# ! with the latest v3.x Agent version". Do not bump this to 3.x.
ARG AGENT_REF=v2.4.3

RUN apt-get update && \
    apt-get install -y \
        git \
        cmake \
        build-essential \
        libasio-dev \
        libtinyxml2-dev \
        libssl-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN git clone --depth 1 --branch "${AGENT_REF}" https://github.com/eProsima/Micro-XRCE-DDS-Agent.git

# ? Deliberately a plain ubuntu base, NOT a ROS image. The agent's CMake
# ? superbuild (UAGENT_SUPERBUILD, on by default) fetches the Fast DDS / Fast CDR
# ? versions this agent expects and installs them into the same prefix. On a ROS
# ? base it would instead find Jazzy's Fast DDS 2.14, which is not that version.
WORKDIR /src/Micro-XRCE-DDS-Agent/build
RUN cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/stage \
        -DUAGENT_BUILD_EXECUTABLE=ON \
        .. && \
    make -j"$(nproc)" && \
    make install

# Record what the executable actually needs, so get-agent.sh can vendor exactly
# those libraries rather than guessing.
RUN ldd /stage/bin/MicroXRCEAgent > /stage/ldd.txt || true

FROM scratch AS export
COPY --from=build /stage/ /stage/
