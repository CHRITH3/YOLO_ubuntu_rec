# YOLO_rec C++ 项目架构重建指南（新手向）

## 1. 先看全局：这个程序在做什么
这个工程编译出一个可执行程序 `yolo_pose_indemind_left`，核心能力是：
1. 从 INDEMIND 相机拿到左目图像与深度。
2. 用 YOLOv8-pose ONNX 模型做人体关键点检测。
3. 用深度图把关键点变成 3D 点，并建立蹦床平面坐标系。
4. 在新坐标系里做髋点轨迹/落点检测，并支持会话录制。

关键入口在 `get_pose_indemind_left.cpp:445`。

---

## 2. CMake 目标装配（先搞懂“怎么被编到一起”）

### 2.1 目标与源码集合
- 单一可执行目标：`yolo_pose_indemind_left`，定义在 `CMakeLists.txt:194`。
- 主入口文件：`get_pose_indemind_left.cpp`（`CMakeLists.txt:195`）。
- 通用模块：`yolo_pose_detector.cpp`、`pose_utils.cpp`（`CMakeLists.txt:171`）。
- `app/` 模块：`camera_intrinsics.cpp`、`depth_region.cpp`、`depth_utils.cpp`、`perf_stats.cpp`、`runtime_state.cpp`（`CMakeLists.txt:181`）。

### 2.2 三个外部依赖
- OpenCV：`find_package(OpenCV REQUIRED)`，`CMakeLists.txt:63`。
- ONNX Runtime：头库搜索与链接在 `CMakeLists.txt:79`、`CMakeLists.txt:109`、`CMakeLists.txt:120`。
- IMSEE/INDEMIND SDK：`include/` + `lib/libindemind.so`，`CMakeLists.txt:134`、`CMakeLists.txt:141`、`CMakeLists.txt:145`。

### 2.3 连接关系
`target_link_libraries(yolo_pose_indemind_left ...)` 在 `CMakeLists.txt:207`：
- `${IMSEE_LIB}`
- `${OpenCV_LIBS}`
- `${ONNXRUNTIME_LIB}`
- Linux 下额外 `pthread`（`CMakeLists.txt:213`）

---

## 3. 主要文件职责地图（你重建时按这个拆模块）

### 3.1 根目录核心文件
- `get_pose_indemind_left.cpp`
  - 主程序与主循环（`main` 在 `:445`）。
  - 相机初始化、图像/深度回调注册（`:532`、`:564`）。
  - 推理调用（`:656`）、绘制（`:692`）、3D 关键点构建（`:751` 起）、髋点/落点流程接线（`:893`）。
  - 键盘交互（`:1251` 起），包含录制开关、参数调整、保存 CSV。
- `yolo_pose_detector.h`
  - 定义关键数据结构：`KeyPoint`（`:37`）、`PoseResult`（`:49`）。
  - 定义检测器类 `YOLOPoseDetector`（`:61`）及完整推理接口。
- `yolo_pose_detector.cpp`
  - 实现 ONNX Runtime 推理全流程：`Init`（`:53`）→ `Preprocess`（`:119`）→ `Detect`（`:170`）→ `Postprocess`（`:215`）→ `NMS`（`:292`）→ 坐标回缩（`:348`）。
- `pose_utils.h` / `pose_utils.cpp`
  - 可视化与几何工具：骨架连线、关键点绘制、信息面板（`DrawPoses` `:100`，`DrawPoseInfo` `:173`）。
  - `MapPoseTo3D`（`:44`）提供“深度+内参→3D 点”通用方法（本项目主循环里主要用手写流程而非直接调用该函数）。

### 3.2 `app/` 目录核心文件
- `app/camera_intrinsics.h` / `app/camera_intrinsics.cpp`
  - 全局相机内参与逆矩阵：`cv_in_left`、`cv_in_left_inv`（`.h:6`，`.cpp:3`）。
- `app/depth_utils.h` / `app/depth_utils.cpp`
  - `RobustDepthMedianU16`（`.cpp:6`）：局部窗口中值滤波，过滤无效深度。
- `app/depth_region.h` / `app/depth_region.cpp`
  - `DepthRegion` 主类几乎全部实现在头文件（类定义 `.h:26`）。
  - 鼠标 4 点选 ROI + RANSAC 平面拟合 + 新坐标系构建（`OnMouse` `.h:50`，`TryFinalizePlaneFromROI` `.h:284`，`BuildFrameFromPlane` `.h:1152`）。
  - 髋点轨迹与落点检测状态机（`UpdateHipData` `.h:604`，`CheckLandingPoint` `.h:687`，`ConfirmLandingPoint` `.h:774`，`RecordLandingPoint` `.h:858`）。
  - `.cpp` 只做鼠标回调桥接：`OnDepthMouseCallback`（`.cpp:3`）。
- `app/perf_stats.h` / `app/perf_stats.cpp`
  - 性能样本缓存与周期打印：`PerfStats`（`.h:8`），`PrintIfNeeded`（`.cpp:24`）。
- `app/runtime_state.h` / `app/runtime_state.cpp`
  - 全局录制状态与会话目录管理：`RuntimeFlags`（`.h:8`），`DataSession`（`.h:13`），`CreateNewSession`（`.cpp:14`）。
- `app/queue_utils.h`
  - 模板工具 `ClearQueue<T>`（`:8`），用于“只处理最新帧”。

### 3.3 SDK 头（理解外设接口必看）
- `include/imrsdk.h`
  - `CIMRSDK`（`:164`），`Init`（`:175`），`RegistImgCallback`（`:227`），`RegistDepthCallback`（`:250`），`EnableDepthProcessor`（`:414`）。
- `include/types.h`
  - `RESOLUTION`（`:36`），`CameraParameter::_K`（`:163`），`MoudleAllParam::_left_camera`（`:378`）。

---

## 4. 显式调用/依赖关系（谁调用谁）

### 4.1 主调用链
1. `main` 初始化 SDK、内参、检测器（`get_pose_indemind_left.cpp:457`、`:481`、`:495`）。
2. SDK 回调把图像/深度推入队列（`:532`、`:564`）。
3. 主循环取最新帧（`:633`、`:642`）并清空队列（`ClearQueue`，`:638`、`:647`）。
4. 调用 `YOLOPoseDetector::Detect`（`:656`），内部再调用 `Preprocess/Postprocess/NMS`（`yolo_pose_detector.cpp:183`、`:210`、`:282`）。
5. 调用 `DrawPoses/DrawPoseInfo`（`get_pose_indemind_left.cpp:692`、`:696`）。
6. 使用 `RobustDepthMedianU16` + `cv_in_left_inv` 把关键点投影到相机 3D（`:773`、`:783`）。
7. 若床面坐标系已就绪，调用 `DepthRegion::TransformToNewFrame` 转到新坐标系（`:792`、`app/depth_region.h:378`）。
8. 把髋点列表喂给 `DepthRegion::UpdateHipData`，触发落点检测（`:893`，`app/depth_region.h:604`）。
9. 按键 `r/s/c` 通过 `g_runtime_flags` 与 `CreateNewSession` 控制落点录制与落盘（`:1316`、`:1322`、`:1333`）。

### 4.2 共享数据结构（跨文件）
- `PoseResult/KeyPoint`：定义在 `yolo_pose_detector.h:37`，被 `pose_utils` 与 `main` 共用。
- `cv_in_left/cv_in_left_inv`：定义在 `app/camera_intrinsics.cpp:3`，主循环与 `DepthRegion` 都依赖。
- `g_perf_stats`：定义在 `app/perf_stats.cpp:7`，主循环写入并周期打印。
- `g_runtime_flags/g_current_session`：定义在 `app/runtime_state.cpp:10`，主循环与 `DepthRegion::RecordLandingPoint` 共同读取。
- `DepthRegion::HipInfo/LandingPoint`：定义在 `app/depth_region.h:522`、`:530`，作为“检测结果→业务记录”的桥梁。

### 4.3 生命周期（非常关键）
- 初始化期：SDK + 模型 + 内参矩阵。
- 运行期：回调线程产出数据，主线程消费最新数据并推理。
- 交互期：鼠标 4 点定 ROI 建床面坐标系；键盘控制显示/录制/参数。
- 结束期：释放 SDK，关闭窗口，输出统计（`get_pose_indemind_left.cpp:1344` 起）。

---

## 5. 这个项目里你必须看懂的 C++ 语法/模式

1. Lambda 回调捕获
- 图像回调：`m_pSDK->RegistImgCallback([&](...) { ... })`（`get_pose_indemind_left.cpp:532`）。
- 用 `[&]` 引用捕获，直接写外部队列和计数器。

2. 模板函数
- `template <typename T> void ClearQueue(std::queue<T>&)`（`app/queue_utils.h:7`）。
- 一份代码支持清空不同类型队列。

3. `extern` 全局状态
- 头文件声明，`.cpp` 唯一定义：
  - `cv_in_left`（`app/camera_intrinsics.h:6` / `.cpp:3`）
  - `g_perf_stats`（`app/perf_stats.h:23` / `.cpp:7`）
  - `g_runtime_flags`（`app/runtime_state.h:20` / `.cpp:10`）

4. RAII 与智能指针
- ONNX session 用 `std::unique_ptr<Ort::Session>`（`yolo_pose_detector.h:157`），避免手工释放。

5. STL 容器做时序算法
- `std::queue`：回调-主循环解耦。
- `std::deque`：性能滑窗、落点缓冲（`app/perf_stats.h:9`，`app/depth_region.h:1275`）。
- `std::vector`：批量姿态与采样点。

6. 数值几何实现方式
- RANSAC + 最小二乘拟合平面（`app/depth_region.h:1084`、`:1043`）。
- 内参矩阵投影/反投影（`get_pose_indemind_left.cpp:783`，`app/depth_region.h:452`）。
- 姿态旋转表示：矩阵→四元数/欧拉角（`get_pose_indemind_left.cpp:104`、`:141`）。

---

## 6. 端到端执行与数据流（按时间顺序）

1. 相机回调产生 `left_image` 与 `depth_mm`，写入两个队列。
2. 主循环取“最新一帧”图像，送入 YOLO，得到 `vector<PoseResult>`。
3. 以关键点像素坐标在深度图采样中值，得到每个关键点 3D（相机系）。
4. 用户完成 4 点 ROI 后，`DepthRegion` 从 ROI 深度样本拟合床面，建立新坐标系。
5. 髋点转新坐标系并进入落点状态机，检测极低点。
6. 若 `REC ON`，落点写入内存列表；按 `S` 时输出 `landing_points.csv`。
7. 各窗口实时显示：主图、region 面板、body metrics。

---

## 7. 新手“重建实现”路线图（建议按阶段提交）

### 阶段 A：先跑通最小闭环（2D）
1. 创建 `YOLOPoseDetector`（接口照 `yolo_pose_detector.h`）。
2. 在主循环里只做 `Detect + DrawPoses`。
3. 验证：窗口能实时显示关键点与骨架。

### 阶段 B：补 3D 关键点（相机坐标系）
1. 增加深度回调与深度队列。
2. 引入 `RobustDepthMedianU16`，用 `cv_in_left_inv * Z * [u,v,1]^T` 求 3D 点。
3. 验证：关键点有稳定的 3D 数值，invalid 深度会被过滤。

### 阶段 C：补 ROI 平面与新坐标系
1. 实现 `DepthRegion::OnMouse` 收集 4 点。
2. 实现 `TryFinalizePlaneFromROI`（ROI 采样→RANSAC→LeastSquares）。
3. 实现 `BuildFrameFromPlane` 与 `TransformToNewFrame`。
4. 验证：坐标轴可绘制，`IsCoordSystemReady()==true`。

### 阶段 D：补落点检测业务
1. 把 hip 列表喂 `UpdateHipData`。
2. 打通 `CheckLandingPoint -> ConfirmLandingPoint -> RecordLandingPoint`。
3. 接入 `RuntimeFlags/DataSession`，实现 `R/C/S` 录制流程。
4. 验证：REC 开启后能累计并导出 `landing_points.csv`。

### 阶段 E：工程化收尾
1. 接入 `PerfStats` 周期打印。
2. 整理键盘交互与帮助说明。
3. 检查队列上限与丢帧统计，避免延迟累积。

---

## 8. 本仓库实操：构建 / 运行 / 验证

### 8.1 构建前检查
1. `models/yolov8n-pose.onnx` 存在。
2. `include/imrsdk.h` 与 `lib/libindemind.so` 存在（`CMakeLists.txt:145`）。
3. 已安装 OpenCV、ONNX Runtime（或在 Conda 环境里）。

### 8.2 构建命令
```bash
cmake -S . -B build_agent -DCMAKE_BUILD_TYPE=Release
cmake --build build_agent -j
```
默认输出目录是 `build_agent_out`（`CMakeLists.txt:14`、`:203`）。

### 8.3 运行命令
```bash
sudo ./build_agent_out/yolo_pose_indemind_left
# 或指定模型
sudo ./build_agent_out/yolo_pose_indemind_left models/yolov8n-pose.onnx
```

### 8.4 功能验证清单
1. 主窗口出现人体关键点与骨架（`k/t/i` 可切换显示）。
2. 鼠标在主窗口点击 4 点后，`region` 窗口显示 `Trampoline Frame: READY`。
3. 按 `r` 后状态变 `REC: ON`，产生会话目录 `runs/YYYYMMDD_HHMMSS`。
4. 检测到落点后按 `s`，在会话目录得到 `landing_points.csv`。
5. 终端周期输出 `[Perf] Inference / Depth / Landing`。

---

## 9. 两个容易踩坑的点
1. `get_pose_indemind_left.cpp` 文件头注释写了“RGB only/no depth”，但实际代码已使用深度回调、3D 映射和落点检测（见 `:564`、`:773`、`:893`）。重建时以代码行为为准。
2. `DepthRegion` 主要实现放在 `.h` 而不是 `.cpp`，初学者容易误以为“没实现”。真正逻辑在 `app/depth_region.h`。

