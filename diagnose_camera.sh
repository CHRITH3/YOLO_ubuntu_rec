#!/bin/bash

echo "=== INDEMIND Camera Diagnostics ==="
echo ""

echo "1. Checking USB devices..."
lsusb | grep -i "camera\|indemind\|usb" || echo "  No camera-related USB devices found"
echo ""

echo "2. Checking video devices..."
ls -l /dev/video* 2>/dev/null || echo "  No video devices found"
echo ""

echo "3. Checking running processes..."
ps aux | grep -E "yolo|indemind" | grep -v grep || echo "  No related processes running"
echo ""

echo "4. Checking device permissions..."
groups | grep -q video && echo "  User is in 'video' group: YES" || echo "  User is in 'video' group: NO (may need: sudo usermod -a -G video $USER)"
echo ""

echo "5. Checking SDK library..."
if [ -f "/home/chris/Desktop/YOLO/lib/libindemind.so" ]; then
    echo "  SDK library: FOUND"
    ldd /home/chris/Desktop/YOLO/lib/libindemind.so | head -5
else
    echo "  SDK library: NOT FOUND"
fi
echo ""

echo "6. Suggested fixes:"
echo "   A. If camera was working before:"
echo "      - Unplug and replug the camera"
echo "      - Kill any hanging processes: sudo pkill -9 yolo_pose"
echo "      - Reset USB: sudo modprobe -r uvcvideo && sudo modprobe uvcvideo"
echo ""
echo "   B. If in a virtual machine:"
echo "      - Check VM settings to ensure USB device is connected"
echo "      - Try USB 2.0 instead of 3.0"
echo ""
echo "   C. Try the other program:"
echo "      - sudo ./build/yolo_pose_detection"
echo "      - If that fails too, it's a camera/SDK issue, not this program"
echo ""

echo "=== End Diagnostics ==="
