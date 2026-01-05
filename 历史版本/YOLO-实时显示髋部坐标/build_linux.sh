#!/bin/bash
# YOLO Pose Detection - Linux Build Script
# Author: Chris
# Date: 2025-10-17

set -e  # Exit on error

echo "========================================="
echo "  YOLO Pose Detection - Linux Build"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if running from project directory
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found! Please run this script from YOLO project directory."
    exit 1
fi

echo "[1/5] Checking dependencies..."

# Check CMake
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found! Please install: sudo apt-get install cmake"
    exit 1
fi
print_success "CMake found: $(cmake --version | head -n1)"

# Check GCC
if ! command -v g++ &> /dev/null; then
    print_error "G++ not found! Please install: sudo apt-get install build-essential"
    exit 1
fi
print_success "G++ found: $(g++ --version | head -n1)"

# Check OpenCV
if ! pkg-config --exists opencv; then
    print_warning "OpenCV not found via pkg-config"
    print_warning "Please install: sudo apt-get install libopencv-dev"
    echo -n "Continue anyway? (y/n): "
    read -r response
    if [ "$response" != "y" ]; then
        exit 1
    fi
else
    print_success "OpenCV found: $(pkg-config --modversion opencv)"
fi

# Check ONNX Runtime
if [ ! -f "/usr/local/lib/libonnxruntime.so" ]; then
    print_warning "ONNX Runtime not found in /usr/local/lib/"
    print_warning "Please run: ./install_onnxruntime.sh"
    echo -n "Continue anyway? (y/n): "
    read -r response
    if [ "$response" != "y" ]; then
        exit 1
    fi
else
    print_success "ONNX Runtime found"
fi

# Check required directories
if [ ! -d "include" ]; then
    print_error "include/ directory not found!"
    exit 1
fi

if [ ! -f "lib/libindemind.so" ]; then
    print_error "lib/libindemind.so not found!"
    exit 1
fi

if [ ! -f "models/yolov8n-pose.onnx" ]; then
    print_warning "YOLO model not found!"
    print_warning "Please run: python3 prepare_yolo_model.py"
    echo -n "Continue anyway? (y/n): "
    read -r response
    if [ "$response" != "y" ]; then
        exit 1
    fi
fi

echo ""
echo "[2/5] Cleaning old build..."
if [ -d "build" ]; then
    rm -rf build
    print_success "Old build directory removed"
fi

echo ""
echo "[3/5] Creating build directory..."
mkdir -p build
cd build

echo ""
echo "[4/5] Configuring CMake..."
if cmake ..; then
    print_success "CMake configuration successful"
else
    print_error "CMake configuration failed!"
    exit 1
fi

echo ""
echo "[5/5] Building project..."
if make -j$(nproc); then
    print_success "Build successful!"
else
    print_error "Build failed!"
    exit 1
fi

cd ..

echo ""
echo "========================================="
echo "  Build Successful!"
echo "========================================="
echo ""
echo "Executable location: build/yolo_pose_detection"
echo ""
echo "To run:"
echo "  1. Ensure IMSEE camera is connected"
echo "  2. Check USB permissions: lsusb"
echo "  3. Run: sudo ./build/yolo_pose_detection"
echo ""
echo "For more information, see README.md"
