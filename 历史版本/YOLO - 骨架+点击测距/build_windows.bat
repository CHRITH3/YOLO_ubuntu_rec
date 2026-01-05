@echo off
REM YOLO Pose Detection - Windows Build Script
REM Author: Chris
REM Date: 2025-10-17

echo =========================================
echo   YOLO Pose Detection - Windows Build
echo =========================================
echo.

REM Check if CMake is installed
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found! Please install CMake first.
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)

echo [1/4] Checking dependencies...

REM Check if Visual Studio is available
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] Visual Studio compiler not found in PATH
    echo Please run this script from "Developer Command Prompt for VS"
    echo Or install Visual Studio 2019+
    pause
    exit /b 1
)

echo [OK] Visual Studio found
echo.

REM Check if required directories exist
if not exist "include\" (
    echo [ERROR] include\ directory not found!
    echo Please ensure IMSEE SDK headers are in include\
    pause
    exit /b 1
)

if not exist "lib\" (
    echo [ERROR] lib\ directory not found!
    echo Please ensure IMSEE SDK libraries are in lib\
    pause
    exit /b 1
)

if not exist "models\yolov8n-pose.onnx" (
    echo [ERROR] YOLO model not found!
    echo Please run: python prepare_yolo_model.py
    pause
    exit /b 1
)

echo [2/4] Creating build directory...
if exist "build\" (
    echo Cleaning old build directory...
    rmdir /s /q build
)
mkdir build
cd build

echo.
echo [3/4] Configuring CMake...
cmake .. -G "Visual Studio 16 2019" -A x64
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    echo Please check:
    echo   1. OpenCV is installed and in PATH
    echo   2. ONNX Runtime is installed
    echo   3. All dependencies are correct
    cd ..
    pause
    exit /b 1
)

echo.
echo [4/4] Building project (Release mode)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo =========================================
echo   Build Successful!
echo =========================================
echo.
echo Executable location: build\Release\yolo_pose_detection.exe
echo.
echo To run:
echo   1. Ensure IMSEE camera is connected
echo   2. cd build\Release
echo   3. yolo_pose_detection.exe
echo.
echo Press any key to exit...
pause >nul
