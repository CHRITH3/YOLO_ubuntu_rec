# OAK RGBD Migration Notes

This package keeps the original INDEMIND source file and adds a new OAK/DepthAI target:

- `get_pose_oak_rgbd.cpp`
- `app/oak_rgbd_capture.h`
- `app/oak_rgbd_capture.cpp`
- `build_oak_rgbd_linux.sh`

## Hardware/profile used by the new target

- OAK-FFC-4P, RVC2
- CAM_A: AR0234 / B0368, RGB master image, 640x400
- CAM_B/C: OV9282 / B0413 stereo, 640x400
- FSYNC: CAM_B OUTPUT master, CAM_A/C INPUT
- Depth: `CV_16UC1`, millimeters, aligned to CAM_A
- YOLO default model: `models/yolov8n-pose-640.onnx`

## Build

The CMake defaults match your local depthai-core path:

```bash
export DEPTHAI_AUTOCALIBRATION=OFF
./build_oak_rgbd_linux.sh
```

Equivalent manual build:

```bash
export DEPTHAI_AUTOCALIBRATION=OFF
cmake -S . -B build_oak \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_OAK_RGBD_TARGET=ON \
  -DBUILD_INDEMIND_TARGET=OFF \
  -DDEPTHAI_CORE_ROOT=/home/chris4/workspace/from_git/depthai-core \
  -DDEPTHAI_CORE_BUILD_DIR=/home/chris4/workspace/from_git/depthai-core/build \
  -DCMAKE_PREFIX_PATH=/home/chris4/workspace/from_git/depthai-core/build

cmake --build build_oak --parallel
```

## Run

Put `yolov8n-pose-640.onnx` back under `models/`, then run:

```bash
export DEPTHAI_AUTOCALIBRATION=OFF
./build_agent_out/yolo_pose_oak_rgbd models/yolov8n-pose-640.onnx
```

For the Flask realtime page integration, run the same target in local server mode:

```bash
export DEPTHAI_AUTOCALIBRATION=OFF
./build_agent_out/yolo_pose_oak_rgbd models/yolov8n-pose-640.onnx --server --port 8081
```

Server mode exposes `GET /stream`, `GET /status`, `POST /click`,
`POST /filter_params`, and `POST /control` on `127.0.0.1:8081`.

When REC is enabled (`r` in the OpenCV UI, or `POST /control` with
`{"action":"start_record"}` in server mode), the raw CAM_A RGB stream is
saved as `runs/<session_id>/rgb.mp4`. Stopping REC closes the MP4 writer.

The old target can still be built by setting `-DBUILD_INDEMIND_TARGET=ON` if the INDEMIND SDK files are present.

## Notes

- Do not resize the business-layer image to 640x640. The OAK frame stays 640x400; `YOLOPoseDetector` letterboxes internally to 640x640 and maps keypoints back to 640x400.
- OAK `StereoDepth.depth` is already millimeters; the new code does not multiply depth by 1000.
- The first migration version uses StereoDepth post-processing: speckle -> spatial, median off, temporal off.
