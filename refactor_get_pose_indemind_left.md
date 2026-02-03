# get_pose_indemind_left.cpp Refactor

## Goals
- Split the monolithic file into layered modules while preserving runtime behavior.
- Keep build output identical: `yolo_pose_indemind_left` still links the same core detector and utilities.

## New Layered Structure
- **Application Layer**
  - `get_pose_indemind_left.cpp`: entry point, camera initialization, main loop, UI/controls.
- **Runtime State Layer**
  - `app/runtime_state.h` + `app/runtime_state.cpp`: `RuntimeFlags`, `DataSession`, and `CreateNewSession()`.
- **Performance Layer**
  - `app/perf_stats.h` + `app/perf_stats.cpp`: `PerfStats` and `g_perf_stats` timing/printing.
- **Camera/Depth Utilities Layer**
  - `app/camera_intrinsics.h` + `app/camera_intrinsics.cpp`: shared `cv_in_left` and `cv_in_left_inv` matrices.
  - `app/depth_utils.h` + `app/depth_utils.cpp`: `RobustDepthMedianU16()` depth sampling.
  - `app/queue_utils.h`: `ClearQueue()` helper for queue draining.
- **Interaction/Visualization Layer**
  - `app/depth_region.h` + `app/depth_region.cpp`: `DepthRegion` class, landing point detection, coordinate transforms, and `OnDepthMouseCallback()`.

## Mapping From Original File
- **Global camera intrinsics**
  - Original: `static cv::Mat cv_in_left, cv_in_left_inv;`
  - Now: `app/camera_intrinsics.*`
- **Runtime flags + session**
  - Original: `RuntimeFlags`, `DataSession`, `g_runtime_flags`, `g_current_session`, `CreateNewSession()`
  - Now: `app/runtime_state.*`
- **Performance statistics**
  - Original: `PerfStats` + `g_perf_stats`
  - Now: `app/perf_stats.*`
- **Depth helper**
  - Original: `RobustDepthMedianU16()`
  - Now: `app/depth_utils.*`
- **DepthRegion UI + landing point logic**
  - Original: `class DepthRegion { ... }` and `OnDepthMouseCallback()`
  - Now: `app/depth_region.*`
- **Queue helper**
  - Original: `template <typename T> void clear(...)`
  - Now: `app/queue_utils.h` (`ClearQueue()`)
- **Main loop / behavior**
  - Original `main()` logic preserved in `get_pose_indemind_left.cpp` with updated includes and helper calls.

## Behavior Notes
- No algorithmic changes were made; functionality is preserved.
- Build still produces `yolo_pose_indemind_left` with the same dependencies and runtime behavior.
