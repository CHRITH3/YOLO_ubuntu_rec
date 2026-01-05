#!/bin/bash
# ONNX Runtime Installation Script for IMSEE-SDK

set -e

echo "=========================================="
echo "ONNX Runtime Installation Script"
echo "=========================================="

# Configuration
ONNX_VERSION="1.17.0"
ARCH="linux-x64"
ONNX_FILE="onnxruntime-${ARCH}-${ONNX_VERSION}.tgz"
ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/${ONNX_FILE}"
INSTALL_PREFIX="/usr/local"

echo ""
echo "Configuration:"
echo "  Version: ${ONNX_VERSION}"
echo "  Architecture: ${ARCH}"
echo "  Install prefix: ${INSTALL_PREFIX}"
echo ""

# Check if already installed
if [ -f "${INSTALL_PREFIX}/lib/libonnxruntime.so" ]; then
    echo "✓ ONNX Runtime already installed at ${INSTALL_PREFIX}"
    echo ""
    echo "Library info:"
    ls -lh ${INSTALL_PREFIX}/lib/libonnxruntime.so*
    echo ""
    read -p "Reinstall? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation skipped."
        exit 0
    fi
fi

# Download
echo "[1/4] Downloading ONNX Runtime ${ONNX_VERSION}..."
if [ ! -f "${ONNX_FILE}" ]; then
    wget -q --show-progress "${ONNX_URL}"
    echo "✓ Downloaded ${ONNX_FILE}"
else
    echo "✓ ${ONNX_FILE} already exists, skipping download"
fi

# Extract
echo ""
echo "[2/4] Extracting archive..."
tar -xzf "${ONNX_FILE}"
echo "✓ Extracted to onnxruntime-${ARCH}-${ONNX_VERSION}/"

# Install headers
echo ""
echo "[3/4] Installing headers to ${INSTALL_PREFIX}/include/onnxruntime..."
sudo mkdir -p "${INSTALL_PREFIX}/include/onnxruntime"
sudo cp -r "onnxruntime-${ARCH}-${ONNX_VERSION}/include/"* "${INSTALL_PREFIX}/include/onnxruntime/"
echo "✓ Headers installed"

# Install libraries
echo ""
echo "[4/4] Installing libraries to ${INSTALL_PREFIX}/lib..."
sudo cp -r "onnxruntime-${ARCH}-${ONNX_VERSION}/lib/"* "${INSTALL_PREFIX}/lib/"
sudo ldconfig
echo "✓ Libraries installed"

# Verify installation
echo ""
echo "=========================================="
echo "Verification"
echo "=========================================="
echo ""
echo "Installed files:"
echo ""
echo "Headers:"
ls -lh ${INSTALL_PREFIX}/include/onnxruntime/*.h 2>/dev/null | head -5
echo "  ... ($(ls ${INSTALL_PREFIX}/include/onnxruntime/*.h 2>/dev/null | wc -l) total)"
echo ""
echo "Libraries:"
ls -lh ${INSTALL_PREFIX}/lib/libonnxruntime.so* 2>/dev/null
echo ""

# Library check
if ldconfig -p | grep -q libonnxruntime; then
    echo "✓ libonnxruntime.so is in library cache"
else
    echo "✗ Warning: libonnxruntime.so not in library cache"
    echo "  Run: sudo ldconfig"
fi

echo ""
echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""
echo "ONNX Runtime ${ONNX_VERSION} installed successfully"
echo ""
echo "Cleanup:"
echo "  rm -rf onnxruntime-${ARCH}-${ONNX_VERSION}/"
echo "  rm ${ONNX_FILE}"
echo ""
read -p "Remove downloaded files? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "onnxruntime-${ARCH}-${ONNX_VERSION}/"
    rm "${ONNX_FILE}"
    echo "✓ Cleanup complete"
fi

echo ""
echo "Next steps:"
echo "  1. Run: python3 prepare_yolo_model.py"
echo "  2. Run: cmake ."
echo "  3. Run: make get_pose_with_depth"
echo ""
