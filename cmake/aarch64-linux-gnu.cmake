set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Determine if the host system is Windows, and set the compiler prefix and executable suffix accordingly
if(WIN32)
    set(EXE_SUFFIX ".exe")
    set(COMPILER_PREFIX "aarch64-none-linux-gnu")
else()
    set(EXE_SUFFIX "")
    set(COMPILER_PREFIX "aarch64-linux-gnu")
else()
    message(FATAL_ERROR "Unsupported host platform")
endif()

# Set the compilers to the cross-compilation toolchain
if(NOT DEFINED ENV{ARM_GCC_ROOT})
    message(FATAL_ERROR
        "ARM_GCC_ROOT is not set. "
        "Set it to the root of the ${COMPILER_PREFIX} toolchain."
    )
endif()

# Set the root of the cross-compilation toolchain from the environment variable
set(ARM_GCC_ROOT "$ENV{ARM_GCC_ROOT}")

# Set the compilers to the cross-compilation toolchain
set(CMAKE_C_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-gcc${EXE_SUFFIX}")

set(CMAKE_CXX_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-g++${EXE_SUFFIX}")

set(CMAKE_ASM_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-gcc${EXE_SUFFIX}")
