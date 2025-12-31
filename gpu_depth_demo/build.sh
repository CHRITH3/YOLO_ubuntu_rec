#!/bin/bash
# Build script for GPU Depth Demo

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${GREEN}=== Building GPU Depth Demo ===${NC}"
echo ""

# Check for CUDA
if command -v nvcc &> /dev/null; then
    echo -e "CUDA Toolkit: ${GREEN}$(nvcc --version | grep release | awk '{print $6}')${NC}"
else
    echo -e "CUDA Toolkit: ${YELLOW}Not found${NC}"
fi

# Check for NVIDIA GPU
if command -v nvidia-smi &> /dev/null; then
    GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -n1)
    echo -e "GPU: ${GREEN}${GPU_NAME}${NC}"
else
    echo -e "GPU: ${YELLOW}nvidia-smi not available${NC}"
fi

# Check for OpenCV CUDA installation
OPENCV_CUDA_PATH="/opt/opencv4-cuda"
if [ -d "$OPENCV_CUDA_PATH" ]; then
    echo -e "OpenCV CUDA: ${GREEN}Found at ${OPENCV_CUDA_PATH}${NC}"
    OPENCV_VERSION=$(cat ${OPENCV_CUDA_PATH}/lib/cmake/opencv4/OpenCVConfig-version.cmake 2>/dev/null | grep "set(OpenCV_VERSION" | head -1 | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    echo -e "             Version: ${GREEN}${OPENCV_VERSION}${NC}"
else
    echo -e "OpenCV CUDA: ${YELLOW}Not installed${NC}"
    echo -e "             Will use system OpenCV (CPU mode)"
    echo ""
    echo -e "${CYAN}To enable GPU acceleration:${NC}"
    echo "  ./install_opencv_cuda.sh"
    echo ""
fi

# Create build directory
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Clean previous CMake cache if OpenCV path changed
if [ -f "CMakeCache.txt" ]; then
    CACHED_OPENCV=$(grep "OpenCV_DIR:PATH" CMakeCache.txt 2>/dev/null | cut -d= -f2)
    if [ -d "$OPENCV_CUDA_PATH" ] && [[ "$CACHED_OPENCV" != *"opencv4-cuda"* ]]; then
        echo -e "${YELLOW}Cleaning CMake cache (OpenCV path changed)...${NC}"
        rm -rf CMakeCache.txt CMakeFiles/
    fi
fi

echo ""
echo -e "${GREEN}Configuring...${NC}"

# Run cmake
cmake .. -DCMAKE_BUILD_TYPE=Release

echo ""
echo -e "${GREEN}Building...${NC}"

# Build
make -j$(nproc)

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo "To run the demo:"
echo -e "  ${CYAN}sudo ./build/gpu_depth_demo${NC}"
echo ""
echo "Options:"
echo "  --cpu    Force CPU stereo matching"
echo "  --sgm    Use SGM algorithm (higher quality)"
echo "  --help   Show help message"
echo ""
echo "Controls:"
echo "  g - Toggle GPU/CPU mode"
echo "  s - Toggle SGM/BM algorithm"
echo "  q - Quit"
echo ""
