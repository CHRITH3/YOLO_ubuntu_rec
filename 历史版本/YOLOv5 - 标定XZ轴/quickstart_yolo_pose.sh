#!/bin/bash
# Quick Start Script for YOLO Pose Detection with IMSEE
# This script guides you through the complete setup process

set -e

echo "=========================================="
echo "YOLO Pose Detection - Quick Start"
echo "=========================================="
echo ""

DEMO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DEMO_DIR"

# Color codes
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${GREEN}[Step $1]${NC} $2"
}

print_warning() {
    echo -e "${YELLOW}⚠ WARNING:${NC} $1"
}

print_error() {
    echo -e "${RED}✗ ERROR:${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

# Step 1: Check prerequisites
print_step 1 "Checking prerequisites..."
echo ""

# Check CMake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -1)
    print_success "CMake found: $CMAKE_VERSION"
else
    print_error "CMake not found"
    echo "Install: sudo apt install cmake"
    exit 1
fi

# Check Python3
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    print_success "Python found: $PYTHON_VERSION"
else
    print_error "Python3 not found"
    echo "Install: sudo apt install python3 python3-pip"
    exit 1
fi

# Check OpenCV (via pkg-config)
if pkg-config --exists opencv4; then
    OPENCV_VERSION=$(pkg-config --modversion opencv4)
    print_success "OpenCV found: $OPENCV_VERSION"
elif pkg-config --exists opencv; then
    OPENCV_VERSION=$(pkg-config --modversion opencv)
    print_success "OpenCV found: $OPENCV_VERSION"
else
    print_warning "OpenCV not detected via pkg-config (may still work)"
fi

echo ""

# Step 2: Install ONNX Runtime
print_step 2 "Installing ONNX Runtime..."
echo ""

if [ -f "/usr/local/lib/libonnxruntime.so" ]; then
    print_success "ONNX Runtime already installed"
    ls -lh /usr/local/lib/libonnxruntime.so*
else
    echo "ONNX Runtime not found. Installing..."
    if [ -f "./install_onnxruntime.sh" ]; then
        ./install_onnxruntime.sh
    else
        print_error "install_onnxruntime.sh not found!"
        exit 1
    fi
fi

echo ""

# Step 3: Install Python dependencies
print_step 3 "Checking Python dependencies..."
echo ""

check_python_package() {
    python3 -c "import $1" 2>/dev/null
    return $?
}

NEED_INSTALL=false

if check_python_package torch; then
    print_success "PyTorch installed"
else
    echo "  PyTorch not found"
    NEED_INSTALL=true
fi

if check_python_package ultralytics; then
    print_success "Ultralytics installed"
else
    echo "  Ultralytics not found"
    NEED_INSTALL=true
fi

if [ "$NEED_INSTALL" = true ]; then
    echo ""
    read -p "Install missing Python packages? [Y/n] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        echo "Installing..."
        pip install torch torchvision ultralytics
        print_success "Python packages installed"
    fi
fi

echo ""

# Step 4: Prepare YOLO model
print_step 4 "Preparing YOLO model..."
echo ""

if [ -d "models" ] && [ -f "models/yolov8n-pose.onnx" ]; then
    print_success "Model already exists: models/yolov8n-pose.onnx"
    ls -lh models/yolov8n-pose.onnx
    echo ""
    read -p "Re-download model? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        python3 prepare_yolo_model.py
    fi
else
    echo "Model not found. Running prepare_yolo_model.py..."
    if [ -f "./prepare_yolo_model.py" ]; then
        python3 prepare_yolo_model.py
    else
        print_error "prepare_yolo_model.py not found!"
        exit 1
    fi
fi

echo ""

# Step 5: Configure with CMake
print_step 5 "Configuring with CMake..."
echo ""

cd "$DEMO_DIR"
cmake .

if [ $? -eq 0 ]; then
    print_success "CMake configuration successful"
else
    print_error "CMake configuration failed"
    exit 1
fi

echo ""

# Step 6: Compile
print_step 6 "Compiling get_pose_with_depth..."
echo ""

make get_pose_with_depth

if [ $? -eq 0 ]; then
    print_success "Compilation successful!"
else
    print_error "Compilation failed"
    exit 1
fi

echo ""

# Check if executable exists
if [ -f "./output/bin/get_pose_with_depth" ]; then
    print_success "Executable created: ./output/bin/get_pose_with_depth"
    ls -lh ./output/bin/get_pose_with_depth
else
    print_error "Executable not found!"
    exit 1
fi

echo ""
echo "=========================================="
echo "✓ Setup Complete!"
echo "=========================================="
echo ""
echo "To run the program:"
echo "  sudo ./output/bin/get_pose_with_depth"
echo ""
echo "Controls:"
echo "  q / ESC  : Quit"
echo "  b        : Toggle bounding box"
echo "  k        : Toggle keypoints"
echo "  s        : Toggle skeleton"
echo "  i        : Toggle info overlay"
echo "  SPACE    : Save frame"
echo ""
echo "For more information, see:"
echo "  README_YOLO_POSE.md"
echo ""

# Ask if user wants to run now
read -p "Run the program now? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Starting pose detection..."
    echo "=========================================="
    sudo ./output/bin/get_pose_with_depth
fi
