# YOLO Pose Detection - Standalone Project
# Can be deployed to any directory or Windows system
# Author: Chris
# Date: 2025-10-17

cmake_minimum_required(VERSION 3.10)
project(YOLOPoseDetection VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow overriding output dir to avoid permission issues with root-owned build/.
if(NOT DEFINED YOLO_OUTPUT_DIR)
    set(YOLO_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/build_agent_out")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "=========================================")
message(STATUS "  YOLO Pose Detection Standalone Build")
message(STATUS "=========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Project directory: ${PROJECT_SOURCE_DIR}")

# ============================================
# Platform Detection
# ============================================
if(WIN32)
    set(OS_WINDOWS TRUE)
    message(STATUS "Platform: Windows")
elseif(UNIX AND NOT APPLE)
    set(OS_LINUX TRUE)
    message(STATUS "Platform: Linux")
elseif(APPLE)
    set(OS_MACOS TRUE)
    message(STATUS "Platform: macOS")
endif()

# ============================================
# Compiler Flags
# ============================================
if(MSVC)
    # Windows MSVC compiler
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    # GCC/Clang (Linux/macOS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")

# ============================================
# Find Dependencies
# ============================================

# 1. OpenCV (Required)
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "✓ Found OpenCV: ${OpenCV_VERSION}")
    message(STATUS "  Include: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${OpenCV_LIBS}")
else()
    message(FATAL_ERROR "✗ OpenCV not found! Please install OpenCV.")
endif()

# 2. ONNX Runtime (Required) - 优先使用 Conda 环境中的 GPU 版本
# 检测 Conda 环境
if(DEFINED ENV{CONDA_PREFIX})
    set(CONDA_PREFIX $ENV{CONDA_PREFIX})
    message(STATUS "✓ Detected Conda environment: ${CONDA_PREFIX}")

    # 优先从 Conda 环境查找
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            ${CONDA_PREFIX}/include/onnxruntime
            ${CONDA_PREFIX}/include
        NO_DEFAULT_PATH
        DOC "ONNX Runtime include directory"
    )

    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS
            ${CONDA_PREFIX}/lib
        NO_DEFAULT_PATH
        DOC "ONNX Runtime library"
    )
endif()

# 如果 Conda 环境中没找到，再从系统路径查找
if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIB)
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            /usr/local/include/onnxruntime
            /usr/include/onnxruntime
            C:/Program\ Files/onnxruntime/include
            ${PROJECT_SOURCE_DIR}/onnxruntime/include
        DOC "ONNX Runtime include directory"
    )

    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS
            /usr/local/lib
            /usr/lib
            C:/Program\ Files/onnxruntime/lib
            ${PROJECT_SOURCE_DIR}/onnxruntime/lib
        DOC "ONNX Runtime library"
    )
endif()

if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    message(STATUS "✓ Found ONNX Runtime")
    message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIR}")
    message(STATUS "  Library: ${ONNXRUNTIME_LIB}")
else()
    message(FATAL_ERROR "✗ ONNX Runtime not found!\n"
        "  Please install ONNX Runtime:\n"
        "  - Linux: Run ./install_onnxruntime.sh\n"
        "  - Windows: Download from https://github.com/microsoft/onnxruntime/releases\n"
        "  Or set ONNXRUNTIME_INCLUDE_DIR and ONNXRUNTIME_LIB manually.")
endif()

# 3. IMSEE SDK (Required)
# Look for libindemind.so in current directory's lib folder
set(IMSEE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(IMSEE_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

if(WIN32)
    set(IMSEE_LIB ${IMSEE_LIB_DIR}/indemind.lib)
    set(IMSEE_DLL ${IMSEE_LIB_DIR}/indemind.dll)
else()
    set(IMSEE_LIB ${IMSEE_LIB_DIR}/libindemind.so)
endif()

# Check if IMSEE SDK files exist
if(EXISTS ${IMSEE_INCLUDE_DIR}/imrsdk.h AND EXISTS ${IMSEE_LIB})
    message(STATUS "✓ Found IMSEE SDK")
    message(STATUS "  Include: ${IMSEE_INCLUDE_DIR}")
    message(STATUS "  Library: ${IMSEE_LIB}")
else()
    message(FATAL_ERROR "✗ IMSEE SDK not found!\n"
        "  Expected locations:\n"
        "    Headers: ${IMSEE_INCLUDE_DIR}/\n"
        "    Library: ${IMSEE_LIB}\n"
        "  Please copy SDK files to these locations.")
endif()

# ============================================
# Include Directories
# ============================================
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/app
    ${IMSEE_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# ============================================
# Source Files
# ============================================
set(COMMON_SOURCES
    yolo_pose_detector.cpp
    pose_utils.cpp
)

set(COMMON_HEADERS
    yolo_pose_detector.h
    pose_utils.h
)

set(APP_SOURCES
    app/camera_intrinsics.cpp
    app/depth_region.cpp
    app/depth_utils.cpp
    app/perf_stats.cpp
    app/runtime_state.cpp
)

# ============================================
# Build Executables
# ============================================

# YOLO Pose INDEMIND Left Camera (RGB only, no depth)
add_executable(yolo_pose_indemind_left
    get_pose_indemind_left.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
    ${APP_SOURCES}
)

set_target_properties(yolo_pose_indemind_left PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${YOLO_OUTPUT_DIR}
    OUTPUT_NAME "yolo_pose_indemind_left"
)

target_link_libraries(yolo_pose_indemind_left
    ${IMSEE_LIB}
    ${OpenCV_LIBS}
    ${ONNXRUNTIME_LIB}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(yolo_pose_indemind_left pthread)
endif()

if(UNIX)
    set_target_properties(yolo_pose_indemind_left PROPERTIES
        BUILD_RPATH "${IMSEE_LIB_DIR}"
        INSTALL_RPATH "${IMSEE_LIB_DIR}:$ORIGIN/../lib"
    )
endif()

# ============================================
# Installation Rules
# ============================================
install(TARGETS yolo_pose_indemind_left
    RUNTIME DESTINATION bin
)

# ============================================
# Post-build: Copy DLLs on Windows
# ============================================
if(WIN32 AND EXISTS ${IMSEE_DLL})
    add_custom_command(TARGET yolo_pose_detection POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${IMSEE_DLL}
            $<TARGET_FILE_DIR:yolo_pose_detection>
        COMMENT "Copying IMSEE DLL to output directory"
    )
endif()

# ============================================
# Build Summary
# ============================================
message(STATUS "=========================================")
message(STATUS "  Build Configuration Summary")
message(STATUS "=========================================")
message(STATUS "Executables:")
message(STATUS "  1. yolo_pose_indemind_left   (INDEMIND RGB only)")
message(STATUS "Output directory: ${YOLO_OUTPUT_DIR}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  ✓ OpenCV ${OpenCV_VERSION}")
message(STATUS "  ✓ ONNX Runtime")
message(STATUS "  ✓ IMSEE SDK")
message(STATUS "")
message(STATUS "To build:")
message(STATUS "  mkdir -p build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "")
message(STATUS "To run:")
if(WIN32)
    message(STATUS "  .\\build\\yolo_pose_indemind_left.exe     ")
else()
    message(STATUS "  sudo ./build/yolo_pose_indemind_left      ")
endif()
message(STATUS "=========================================")
