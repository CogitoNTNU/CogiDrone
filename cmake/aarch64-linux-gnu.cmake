set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Determine if the host system is Windows, and set the compiler prefix and executable suffix accordingly
if(WIN32)
    set(EXE_SUFFIX ".exe")
    set(COMPILER_PREFIX "aarch64-none-linux-gnu")
elseif(UNIX)
    set(EXE_SUFFIX "")
    set(COMPILER_PREFIX "aarch64-linux-gnu")
else()
    message(FATAL_ERROR "Unsupported host platform")
endif()


# // # Set the compilers to the cross-compilation toolchain
# // if(NOT DEFINED ENV{ARM_GCC_ROOT})
# //     message(FATAL_ERROR
# //         "ARM_GCC_ROOT is not set. "
# //         "Set it to the root of the ${COMPILER_PREFIX} toolchain."
# //     )
# // endif()

# // # Set the root of the cross-compilation toolchain from the environment variable
# // set(ARM_GCC_ROOT "$ENV{ARM_GCC_ROOT}")


# Resolve the toolchain root:
#   1. ARM_GCC_ROOT env var (Docker/macOS workflow)
#   2. toolchain.windows.config (Windows native toolchain, machine-local, gitignored)
if(DEFINED ENV{ARM_GCC_ROOT})
    set(ARM_GCC_ROOT "$ENV{ARM_GCC_ROOT}")
elseif(WIN32 AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../toolchain.windows.config")
    file(READ "${CMAKE_CURRENT_LIST_DIR}/../toolchain.windows.config" _twc)
    string(REGEX MATCH "ARM_GCC_ROOT=\"([^\"]+)\"" _twc_match "${_twc}")
    if(NOT _twc_match)
        message(FATAL_ERROR
            "toolchain.windows.config exists but contains no ARM_GCC_ROOT=\"...\" line. "
            "See toolchain.windows.config.example."
        )
    endif()
    set(ARM_GCC_ROOT "${CMAKE_MATCH_1}")
else()
    message(FATAL_ERROR
        "ARM_GCC_ROOT is not set and toolchain.windows.config was not found. "
        "Set ARM_GCC_ROOT to the root of the ${COMPILER_PREFIX} toolchain "
        "(or copy toolchain.windows.config.example to toolchain.windows.config on Windows)."
    )
endif()

# Set the compilers to the cross-compilation toolchain
set(CMAKE_C_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-gcc${EXE_SUFFIX}")

set(CMAKE_CXX_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-g++${EXE_SUFFIX}")

set(CMAKE_ASM_COMPILER
    "${ARM_GCC_ROOT}/bin/${COMPILER_PREFIX}-gcc${EXE_SUFFIX}")
