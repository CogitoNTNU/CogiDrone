set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED ENV{ARM_GCC_ROOT})
    message(FATAL_ERROR
        "ARM_GCC_ROOT is not set. "
        "Set it to the root of the aarch64-none-linux-gnu toolchain."
    )
endif()

# Set the root of the aarch64-none-linux-gnu toolchain from the environment variable
set(ARM_GCC_ROOT "$ENV{ARM_GCC_ROOT}")

# Determine if the host system is Windows, and set the executable suffix accordingly
if(WIN32)
    set(EXE_SUFFIX ".exe")
else()
    set(EXE_SUFFIX "")
endif()

# Set the compilers to the aarch64-none-linux-gnu toolchain
set(CMAKE_C_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-gcc${EXE_SUFFIX}")

set(CMAKE_CXX_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-g++${EXE_SUFFIX}")

set(CMAKE_ASM_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-gcc${EXE_SUFFIX}")
