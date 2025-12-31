cmake_minimum_required(VERSION 3.10)
project(gpu_depth_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# OpenCV Configuration
# =============================================================================
# Priority:
#   1. Use OpenCV CUDA version if installed at /opt/opencv4-cuda
#   2. Fall back to system OpenCV (3.4.3)
#
# To force a specific OpenCV, set OpenCV_DIR before running cmake:
#   cmake -DOpenCV_DIR=/opt/opencv4-cuda/lib/cmake/opencv4 ..
# =============================================================================

# Check for CUDA-enabled OpenCV first
set(OPENCV_CUDA_INSTALL_PATH "/opt/opencv4-cuda")

if(NOT DEFINED OpenCV_DIR)
    if(EXISTS "${OPENCV_CUDA_INSTALL_PATH}/lib/cmake/opencv4/OpenCVConfig.cmake")
        message(STATUS "Found OpenCV CUDA installation at ${OPENCV_CUDA_INSTALL_PATH}")
        set(OpenCV_DIR "${OPENCV_CUDA_INSTALL_PATH}/lib/cmake/opencv4")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/opencv_cuda_path.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/opencv_cuda_path.cmake")
        message(STATUS "Using OpenCV path from opencv_cuda_path.cmake")
    endif()
endif()

# Find OpenCV
find_package(OpenCV REQUIRED)
message(STATUS "")
message(STATUS "OpenCV Configuration:")
message(STATUS "  Version: ${OpenCV_VERSION}")
message(STATUS "  Install path: ${OpenCV_INSTALL_PATH}")
message(STATUS "  Libraries: ${OpenCV_LIBS}")

# Check for CUDA support in OpenCV
set(HAVE_OPENCV_CUDA FALSE)

# Method 1: Check for cudastereo module
list(FIND OpenCV_LIBS "opencv_cudastereo" _cudastereo_idx)
if(NOT _cudastereo_idx EQUAL -1)
    set(HAVE_OPENCV_CUDA TRUE)
    message(STATUS "  CUDA stereo: FOUND")
endif()

# Method 2: Check for CUDA version variable
if(OpenCV_CUDA_VERSION)
    set(HAVE_OPENCV_CUDA TRUE)
    message(STATUS "  CUDA version: ${OpenCV_CUDA_VERSION}")
endif()

# Method 3: Try to find the library file
if(NOT HAVE_OPENCV_CUDA)
    find_library(OPENCV_CUDASTEREO_LIB
        NAMES opencv_cudastereo
        PATHS
            "${OpenCV_INSTALL_PATH}/lib"
            "${OPENCV_CUDA_INSTALL_PATH}/lib"
            /usr/local/lib
        NO_DEFAULT_PATH
    )
    if(OPENCV_CUDASTEREO_LIB)
        set(HAVE_OPENCV_CUDA TRUE)
        message(STATUS "  CUDA stereo lib: ${OPENCV_CUDASTEREO_LIB}")
    endif()
endif()

if(HAVE_OPENCV_CUDA)
    add_definitions(-DHAVE_CUDA)
    message(STATUS "  CUDA support: ENABLED")
else()
    message(STATUS "  CUDA support: DISABLED (CPU mode only)")
endif()

message(STATUS "")

# Find CUDA (for additional info)
find_package(CUDA QUIET)
if(CUDA_FOUND)
    message(STATUS "CUDA Toolkit: ${CUDA_VERSION}")
    include_directories(${CUDA_INCLUDE_DIRS})
endif()

# =============================================================================
# INDEMIND SDK
# =============================================================================
set(INDEMIND_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(INDEMIND_INCLUDE_DIR "${INDEMIND_SDK_ROOT}/include")
set(INDEMIND_LIB_DIR "${INDEMIND_SDK_ROOT}/lib")

find_library(INDEMIND_LIB
    NAMES indemind indem
    PATHS ${INDEMIND_LIB_DIR}
    NO_DEFAULT_PATH
)

if(NOT INDEMIND_LIB)
    find_library(INDEMIND_LIB NAMES indemind indem)
endif()

if(INDEMIND_LIB)
    message(STATUS "INDEMIND library: ${INDEMIND_LIB}")
else()
    message(FATAL_ERROR "INDEMIND library not found!")
endif()

# =============================================================================
# Build Target
# =============================================================================
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${INDEMIND_INCLUDE_DIR}
)

add_executable(gpu_depth_demo gpu_depth_demo.cpp)

target_link_libraries(gpu_depth_demo
    ${OpenCV_LIBS}
    ${INDEMIND_LIB}
    pthread
)

# Set RPATH for runtime library loading
set_target_properties(gpu_depth_demo PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${INDEMIND_LIB_DIR};${OPENCV_CUDA_INSTALL_PATH}/lib"
)

# =============================================================================
# Build Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV: ${OpenCV_VERSION}")
message(STATUS "  CUDA in OpenCV: ${HAVE_OPENCV_CUDA}")
if(CUDA_FOUND)
    message(STATUS "  CUDA Toolkit: ${CUDA_VERSION}")
endif()
message(STATUS "  INDEMIND SDK: ${INDEMIND_SDK_ROOT}")
message(STATUS "===========================")
message(STATUS "")

if(NOT HAVE_OPENCV_CUDA)
    message(STATUS "")
    message(STATUS "NOTE: OpenCV CUDA support not found.")
    message(STATUS "      To enable GPU acceleration, run:")
    message(STATUS "        ./install_opencv_cuda.sh")
    message(STATUS "      Then rebuild this project.")
    message(STATUS "")
endif()
