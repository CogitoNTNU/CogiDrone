set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED ENV{ARM_GCC_ROOT})
    message(FATAL_ERROR
        "ARM_GCC_ROOT is not set. "
        "Set it to the root of the aarch64-none-linux-gnu toolchain."
    )
endif()

set(ARM_GCC_ROOT "$ENV{ARM_GCC_ROOT}")

set(CMAKE_C_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-gcc")

set(CMAKE_CXX_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-g++")

set(CMAKE_ASM_COMPILER
    "${ARM_GCC_ROOT}/bin/aarch64-none-linux-gnu-gcc")
    
