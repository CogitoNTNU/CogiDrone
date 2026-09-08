@echo off
setlocal

echo === CogiDrone ARM64 Build ===
echo.

REM Find configuration file relative to this script
set "CONFIG_FILE=%~dp0toolchain.config"

if not exist "%CONFIG_FILE%" (
    echo ERROR: toolchain.config not found.
    echo.
    echo Copy toolchain.config.example to toolchain.config
    echo and set ARM_GCC_ROOT to your Arm GNU Toolchain installation.
    exit /b 1
)

REM Read ARM_GCC_ROOT from toolchain.config
for /f "usebackq tokens=1,* delims==" %%A in ("%CONFIG_FILE%") do (
    if "%%A"=="ARM_GCC_ROOT" set "ARM_GCC_ROOT=%%B"
)

REM Remove surrounding quotes if present
if defined ARM_GCC_ROOT (
    set "ARM_GCC_ROOT=%ARM_GCC_ROOT:"=%"
)

if not defined ARM_GCC_ROOT (
    echo ERROR: ARM_GCC_ROOT is not set in toolchain.config.
    exit /b 1
)

if not exist "%ARM_GCC_ROOT%\bin\aarch64-none-linux-gnu-g++.exe" (
    echo ERROR: ARM GCC compiler not found:
    echo   "%ARM_GCC_ROOT%\bin\aarch64-none-linux-gnu-g++.exe"
    echo.
    echo Check ARM_GCC_ROOT in toolchain.config.
    exit /b 1
)

echo Toolchain:
echo   %ARM_GCC_ROOT%
echo.

REM Configure
cmake --preset linux-arm64

if errorlevel 1 (
    echo.
    echo CMake configuration failed.
    exit /b 1
)

REM Build
cmake --build build/linux-arm64

if errorlevel 1 (
    echo.
    echo Build failed.
    exit /b 1
)

echo.
echo === Build successful ===
