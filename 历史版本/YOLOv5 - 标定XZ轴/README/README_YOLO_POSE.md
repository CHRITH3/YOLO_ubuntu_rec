# YOLO Pose Detection with IMSEE Depth Fusion

Real-time human pose detection and estimation using YOLOv8-pose model with 3D depth information from IMSEE dual-camera system.

## Features

- ✅ Real-time human pose detection (17 COCO keypoints)
- ✅ 3D coordinate mapping using depth data
- ✅ Multi-person detection
- ✅ Colored skeleton visualization
- ✅ Body height estimation
- ✅ Performance monitoring (FPS, inference time)
- ✅ Interactive controls for visualization

## Prerequisites

### 1. System Requirements

- Ubuntu 18.04+ / Linux x86-64
- GCC 7+ with C++14 support
- CMake 3.0+
- Python 3.6+ (for model preparation)

### 2. Dependencies

- OpenCV (already installed with IMSEE-SDK)
- IMSEE-SDK (dual-camera system)
- **ONNX Runtime 1.17.0** (new dependency)
- **Ultralytics** (for model export, Python)

## Installation

### Step 1: Install ONNX Runtime

```bash
cd /home/chris/workspace/IMSEE-SDK/demo
./install_onnxruntime.sh
```

This will:
- Download ONNX Runtime 1.17.0 for Linux x64
- Install headers to `/usr/local/include/onnxruntime`
- Install libraries to `/usr/local/lib`
- Update library cache with `ldconfig`

### Step 2: Install Python Dependencies (for model preparation)

```bash
pip install torch torchvision ultralytics
```

### Step 3: Prepare YOLO Model

```bash
python3 prepare_yolo_model.py
```

This interactive script will:
- Let you choose model size (n/s/m/l/x)
- Download the YOLOv8-pose pretrained model
- Convert it to ONNX format
- Save to `models/yolov8n-pose.onnx`

**Model Selection Guide:**
- `n` (nano): ~6MB, fastest, recommended for real-time (15-20 FPS on CPU)
- `s` (small): ~12MB, balanced
- `m` (medium): ~26MB, good accuracy
- `l` (large): ~52MB, high accuracy
- `x` (xlarge): ~98MB, best accuracy but slower

### Step 4: Compile

```bash
cd /home/chris/workspace/IMSEE-SDK/demo
cmake .
make get_pose_with_depth
```

If successful, you'll see:
```
✓ YOLO Pose Detection will be built
...
[100%] Built target get_pose_with_depth
```

## Usage

### Basic Usage

```bash
sudo ./output/bin/get_pose_with_depth
```

### With Custom Model

```bash
sudo ./output/bin/get_pose_with_depth models/yolov8s-pose.onnx
```

### Keyboard Controls

While the program is running:

- **q / ESC**: Quit the application
- **b**: Toggle bounding box on/off
- **k**: Toggle keypoints display on/off
- **s**: Toggle skeleton lines on/off
- **i**: Toggle info overlay on/off
- **SPACE**: Save current frame to disk

## Output

### On-Screen Display

The program shows:
- **Main Window**: "Pose Detection + Depth"
  - Live camera feed with overlaid skeleton
  - Bounding boxes around detected persons
  - Keypoint markers (colored by confidence)
  - Performance stats (FPS, inference time)
  - Detection count

- **Info Overlay** (toggle with 'i'):
  - Per-person confidence score
  - Average depth from camera
  - Estimated body height

### Console Output

On startup:
```
Initializing YOLO Pose Detector...
  Model: models/yolov8n-pose.onnx
  Input size: 640x640
  Input name: images
  Input shape: [1, 3, 640, 640]
  Output name: output0
  Output shape: [1, 56, 8400]
✓ YOLO Pose Detector initialized successfully

Camera Intrinsics:
  fx: 374.542, fy: 374.542
  cx: 318.848, cy: 197.364
```

On exit:
```
=== Performance Statistics ===

Total runtime: 60 seconds
Total images captured: 3000
Total depth maps: 1500
Total pose detections: 1200
Dropped image frames: 0
Dropped depth frames: 0

Average rates:
  Image: 50.0 FPS
  Depth: 25.0 FPS
  Pose: 20.0 FPS

==============================
```

## Visualization

### Color Coding

**Keypoints** (circles):
- 🔴 Red: High confidence (>0.8)
- 🟠 Orange: Medium confidence (0.6-0.8)
- 🟡 Yellow: Low confidence (0.5-0.6)

**Skeleton** (lines connecting keypoints):
- 🟡 Yellow: Head (nose, eyes, ears)
- 🔵 Cyan: Torso (shoulders, hips)
- 🟢 Green: Left arm (shoulder → elbow → wrist)
- 🔵 Blue: Right arm (shoulder → elbow → wrist)
- 🟣 Magenta: Left leg (hip → knee → ankle)
- 🟠 Orange: Right leg (hip → knee → ankle)

### COCO 17 Keypoints

```
 0: Nose          1: Left Eye       2: Right Eye
 3: Left Ear      4: Right Ear      5: Left Shoulder
 6: Right Shoulder 7: Left Elbow    8: Right Elbow
 9: Left Wrist    10: Right Wrist   11: Left Hip
12: Right Hip     13: Left Knee     14: Right Knee
15: Left Ankle    16: Right Ankle
```

## Performance

### Expected Performance

**YOLOv8n-pose (nano model):**
- CPU (Intel i5/i7): 15-20 FPS
- GPU (CUDA): 50+ FPS
- End-to-end latency: <100ms
- Multi-person: Up to 10+ persons simultaneously

**Memory Usage:**
- Model size: ~6MB (nano)
- Runtime memory: ~200MB

### Optimization Tips

**For better speed:**
1. Use YOLOv8n-pose (fastest model)
2. Reduce input size in model export (480 instead of 640)
3. Enable GPU acceleration (requires CUDA-enabled ONNX Runtime)
4. Close depth detail windows if not needed

**For better accuracy:**
1. Use YOLOv8m-pose or larger
2. Ensure good lighting conditions
3. Keep persons fully visible in frame
4. Maintain optimal distance (1-5 meters)

## File Structure

```
IMSEE-SDK/demo/
├── yolo_pose_detector.h          # YOLO inference class header
├── yolo_pose_detector.cpp        # YOLO inference implementation
├── pose_utils.h                  # Utility functions header
├── pose_utils.cpp                # 3D mapping and visualization
├── get_pose_with_depth.cpp       # Main program
├── prepare_yolo_model.py         # Model download/export script
├── install_onnxruntime.sh        # ONNX Runtime installation
├── CMakeLists.txt                # Build configuration (updated)
├── README_YOLO_POSE.md           # This file
└── models/
    └── yolov8n-pose.onnx         # YOLO model (created by script)
```

## Technical Details

### System Architecture

```
Camera → Image Capture → YOLO Detection → 3D Mapping → Visualization
           ↓                  ↓                ↓
         50 FPS          15-20 FPS      Depth Fusion
                                          ↓
                                    Keypoint 3D coords
```

### Data Flow

1. **Image Acquisition**: Left camera image at 50 FPS (640×400)
2. **Preprocessing**: BGR→RGB, resize, normalize, HWC→CHW
3. **Inference**: ONNX Runtime executes YOLOv8-pose
4. **Postprocessing**: NMS, coordinate transformation
5. **Depth Fusion**: Map 2D keypoints to 3D using depth map
6. **Visualization**: Draw skeleton, bbox, info overlay

### 3D Coordinate Calculation

For each keypoint (u, v) with depth Z:

```
X = (u - cx) × Z / fx
Y = (v - cy) × Z / fy
Z = Z
```

Where:
- (u, v): 2D pixel coordinates
- (cx, cy): Principal point (camera intrinsics)
- (fx, fy): Focal lengths (camera intrinsics)
- Z: Depth value from depth map (millimeters)
- (X, Y, Z): 3D coordinates in left camera frame

## Troubleshooting

### Problem: "Failed to initialize YOLO Pose Detector"

**Possible causes:**
1. ONNX Runtime not installed
2. Model file doesn't exist
3. Wrong model path

**Solutions:**
```bash
# Check ONNX Runtime
ldconfig -p | grep onnxruntime

# Check model file
ls -lh models/yolov8n-pose.onnx

# Reinstall if needed
./install_onnxruntime.sh
python3 prepare_yolo_model.py
```

### Problem: Low FPS (<10)

**Solutions:**
1. Use smaller model: `yolov8n-pose`
2. Check CPU usage: `htop`
3. Close other applications
4. Verify no thermal throttling

### Problem: Poor Detection Quality

**Solutions:**
1. Ensure good lighting
2. Keep full body visible
3. Optimal distance: 1-5 meters
4. Use larger model if needed (yolov8s or yolov8m)
5. Increase confidence threshold

### Problem: Depth Values Invalid

**Solutions:**
1. Ensure textured background (not plain white wall)
2. Check camera exposure
3. Verify depth processor is running
4. Depth valid range: 0.5-10 meters

### Problem: Compilation Errors

**Common issues:**

```bash
# Missing ONNX Runtime headers
error: onnxruntime_cxx_api.h: No such file or directory
→ Solution: ./install_onnxruntime.sh

# Undefined reference to Ort::Session
→ Solution: Check LD_LIBRARY_PATH, run sudo ldconfig

# CMake doesn't find ONNX Runtime
→ Solution: Verify installation in /usr/local/lib
```

## Advanced Usage

### Custom Confidence Thresholds

Modify in code:
```cpp
YOLOPoseDetector pose_detector(model_path, 640, 0.5f, 0.45f);
//                                              ↑      ↑
//                                         conf_thresh  iou_thresh
```

### Different Input Sizes

Export model with different size:
```python
model.export(format='onnx', imgsz=480)  # Instead of 640
```

Then update detector initialization:
```cpp
YOLOPoseDetector pose_detector(model_path, 480, 0.5f, 0.45f);
```

### GPU Acceleration

Requires CUDA-enabled ONNX Runtime:
```bash
# Download GPU version
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-x64-gpu-1.17.0.tgz
```

Modify `yolo_pose_detector.cpp`:
```cpp
session_options_.AppendExecutionProvider_CUDA(0);  // Use GPU 0
```

## Applications

- 🏃 Human activity recognition
- 🤸 Fitness/sports analysis
- 👴 Fall detection for elderly care
- 🎮 Motion capture for gaming
- 🏥 Physical therapy monitoring
- 🤖 Human-robot interaction
- 📏 Body measurement and sizing

## Known Limitations

1. **Occlusion**: Partially hidden persons have lower accuracy
2. **Distance**: Best results at 1-5 meters from camera
3. **Lighting**: Poor lighting reduces detection quality
4. **Multiple people**: Accuracy may decrease with >10 persons
5. **Unusual poses**: Sitting, lying down less accurate than standing

## References

- **YOLO**: [Ultralytics YOLOv8](https://github.com/ultralytics/ultralytics)
- **ONNX Runtime**: [Microsoft ONNX Runtime](https://github.com/microsoft/onnxruntime)
- **COCO Keypoints**: [COCO Dataset](https://cocodataset.org/#keypoints-2020)
- **IMSEE SDK**: INDEMIND dual-camera system

## License

Copyright 2020 Indemind Co., Ltd. All rights reserved.

Licensed under the Apache License, Version 2.0.

## Support

For issues or questions:
1. Check this README thoroughly
2. Review console error messages
3. Verify all prerequisites installed
4. Check model file exists and is valid

---

**Created**: 2025-10-15
**Version**: 1.0
**Author**: Claude Code Implementation
