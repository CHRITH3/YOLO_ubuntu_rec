# get_pose_indemind_left.cpp Refactor
# get_pose_indemind_left.cpp 重构

## Goals
## 目标
- Split the monolithic file into layered modules while preserving runtime behavior.
- 在保持运行时行为不变的情况下，将单体文件拆分为分层模块。
- Keep build output identical: `yolo_pose_indemind_left` still links the same core detector and utilities.
- 保持构建输出一致：`yolo_pose_indemind_left` 仍链接相同的核心检测器和工具。

## New Layered Structure
## 新的分层结构
- **Application Layer**
- **应用层**
  - `get_pose_indemind_left.cpp`: entry point, camera initialization, main loop, UI/controls.
  - `get_pose_indemind_left.cpp`：入口点、相机初始化、主循环、UI/控制。
- **Runtime State Layer**
- **运行时状态层**
  - `app/runtime_state.h` + `app/runtime_state.cpp`: `RuntimeFlags`, `DataSession`, and `CreateNewSession()`.
  - `app/runtime_state.h` + `app/runtime_state.cpp`：`RuntimeFlags`、`DataSession`、`CreateNewSession()`。
- **Performance Layer**
- **性能层**
  - `app/perf_stats.h` + `app/perf_stats.cpp`: `PerfStats` and `g_perf_stats` timing/printing.
  - `app/perf_stats.h` + `app/perf_stats.cpp`：`PerfStats` 与 `g_perf_stats` 的计时/打印。
- **Camera/Depth Utilities Layer**
- **相机/深度工具层**
  - `app/camera_intrinsics.h` + `app/camera_intrinsics.cpp`: shared `cv_in_left` and `cv_in_left_inv` matrices.
  - `app/camera_intrinsics.h` + `app/camera_intrinsics.cpp`：共享 `cv_in_left` 与 `cv_in_left_inv` 矩阵。
  - `app/depth_utils.h` + `app/depth_utils.cpp`: `RobustDepthMedianU16()` depth sampling.
  - `app/depth_utils.h` + `app/depth_utils.cpp`：`RobustDepthMedianU16()` 深度采样。
  - `app/queue_utils.h`: `ClearQueue()` helper for queue draining.
  - `app/queue_utils.h`：用于队列清空的 `ClearQueue()` 辅助函数。
- **Interaction/Visualization Layer**
- **交互/可视化层**
  - `app/depth_region.h` + `app/depth_region.cpp`: `DepthRegion` class, landing point detection, coordinate transforms, and `OnDepthMouseCallback()`.
  - `app/depth_region.h` + `app/depth_region.cpp`：`DepthRegion` 类、落点检测、坐标变换，以及 `OnDepthMouseCallback()`。

## Mapping From Original File
## 与原文件的映射
- **Global camera intrinsics**
- **全局相机内参**
  - Original: `static cv::Mat cv_in_left, cv_in_left_inv;`
  - 原始：`static cv::Mat cv_in_left, cv_in_left_inv;`
  - Now: `app/camera_intrinsics.*`
  - 现在：`app/camera_intrinsics.*`
- **Runtime flags + session**
- **运行时标志 + 会话**
  - Original: `RuntimeFlags`, `DataSession`, `g_runtime_flags`, `g_current_session`, `CreateNewSession()`
  - 原始：`RuntimeFlags`、`DataSession`、`g_runtime_flags`、`g_current_session`、`CreateNewSession()`
  - Now: `app/runtime_state.*`
  - 现在：`app/runtime_state.*`
- **Performance statistics**
- **性能统计**
  - Original: `PerfStats` + `g_perf_stats`
  - 原始：`PerfStats` + `g_perf_stats`
  - Now: `app/perf_stats.*`
  - 现在：`app/perf_stats.*`
- **Depth helper**
- **深度辅助函数**
  - Original: `RobustDepthMedianU16()`
  - 原始：`RobustDepthMedianU16()`
  - Now: `app/depth_utils.*`
  - 现在：`app/depth_utils.*`
- **DepthRegion UI + landing point logic**
- **DepthRegion UI + 落点逻辑**
  - Original: `class DepthRegion { ... }` and `OnDepthMouseCallback()`
  - 原始：`class DepthRegion { ... }` 和 `OnDepthMouseCallback()`
  - Now: `app/depth_region.*`
  - 现在：`app/depth_region.*`
- **Queue helper**
- **队列辅助函数**
  - Original: `template <typename T> void clear(...)`
  - 原始：`template <typename T> void clear(...)`
  - Now: `app/queue_utils.h` (`ClearQueue()`)
  - 现在：`app/queue_utils.h`（`ClearQueue()`）
- **Main loop / behavior**
- **主循环 / 行为**
  - Original `main()` logic preserved in `get_pose_indemind_left.cpp` with updated includes and helper calls.
  - 原始 `main()` 逻辑保留在 `get_pose_indemind_left.cpp` 中，仅更新了包含项与辅助调用。

## Behavior Notes
## 行为说明
- No algorithmic changes were made; functionality is preserved.
- 未做算法变更；功能保持不变。
- Build still produces `yolo_pose_indemind_left` with the same dependencies and runtime behavior.
- 构建仍生成 `yolo_pose_indemind_left`，依赖和运行时行为一致。
