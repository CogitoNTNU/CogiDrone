@echo off
setlocal

set BUILD_DIR=build
set CONFIG=Release

cmake -S . -B %BUILD_DIR%
if errorlevel 1 exit /b %errorlevel%

cmake --build %BUILD_DIR% --config %CONFIG% --parallel
if errorlevel 1 exit /b %errorlevel%

echo Build completed successfully.
