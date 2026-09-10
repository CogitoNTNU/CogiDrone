FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu && \
    rm -rf /var/lib/apt/lists

ENV ARM_GCC_ROOT=/usr
