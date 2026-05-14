本页解释当前项目中 **COCO 17 人体关键点**如何被编号、存储、从 YOLOv8 Pose 输出中解析，并最终绘制成关键点圆点与彩色骨架线。架构假设是：项目把 YOLOv8-pose 的每个人体检测结果统一封装为 `PoseResult`，其中 `keypoints` 固定保存 17 个 `KeyPoint`；可视化层只依赖关键点索引、置信度阈值和骨架连线表，不重新理解模型输出张量。代码验证显示，该假设由 `KeypointType`、`KeyPoint`、`PoseResult`、`Postprocess()` 和 `DrawPoses()` 共同成立。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L15-L57), [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L219-L289), [pose_utils.cpp](pose_utils.cpp#L100-L171)

## 1. 本页在系统中的位置

在完整推理链路中，本页关注的是 **模型后处理之后、业务算法之前** 的人体姿态数据表示：模型输出先被解析为 `PoseResult`，每个 `PoseResult` 包含人体框、人体置信度和 17 个关键点；随后可视化函数根据这些关键点绘制骨架。若你想先理解模型如何运行，请阅读 [YOLOv8 Pose 的 ONNX Runtime 推理流程](12-yolov8-pose-de-onnx-runtime-tui-li-liu-cheng)；若你想理解坐标为什么需要回映射，请阅读 [Letterbox 预处理、坐标回映射与非极大值抑制](13-letterbox-yu-chu-li-zuo-biao-hui-ying-she-yu-fei-ji-da-zhi-yi-zhi)。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L170-L212), [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L215-L289), [pose_utils.cpp](pose_utils.cpp#L100-L171)

下面的图展示本页覆盖的数据边界：它从 YOLO 输出张量中的关键点三元组开始，到屏幕上的圆点和连线结束；深度融合、人体坐标系和业务判定不是本页主体。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L219-L276), [pose_utils.cpp](pose_utils.cpp#L125-L168)

```mermaid
flowchart LR
    A["YOLOv8-pose 输出<br/>[1, 56, 8400]"] --> B["Postprocess()<br/>解析候选人体"]
    B --> C["PoseResult<br/>bbox + box_confidence + keypoints[17]"]
    C --> D["RescaleCoordinates()<br/>关键点回到原图坐标"]
    D --> E["DrawPoses()<br/>绘制骨架线与关键点圆点"]
    E --> F["OpenCV 显示窗口"]
```

图中的 `56` 被代码注释拆解为 `4` 个 bbox 数值、`1` 个人体检测置信度和 `17*3` 个关键点数值；关键点从输出元素索引 `5` 开始，每个关键点依次读取 `x`、`y`、`visibility`，并写入 `result.keypoints[k]`。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L219-L276)

## 2. COCO 17 关键点编号

项目用 `enum KeypointType` 固定定义 COCO 17 个关键点的索引，编号从 `NOSE = 0` 到 `RIGHT_ANKLE = 16`。这意味着代码中访问关键点时不需要写魔法数字，例如可以用 `pose.keypoints[LEFT_HIP]` 访问左髋，用 `pose.keypoints[RIGHT_SHOULDER]` 访问右肩。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L15-L34)

| 索引 | 枚举名 | 英文显示名 | 身体区域 |
|---:|---|---|---|
| 0 | `NOSE` | Nose | 头部 |
| 1 | `LEFT_EYE` | Left Eye | 头部 |
| 2 | `RIGHT_EYE` | Right Eye | 头部 |
| 3 | `LEFT_EAR` | Left Ear | 头部 |
| 4 | `RIGHT_EAR` | Right Ear | 头部 |
| 5 | `LEFT_SHOULDER` | Left Shoulder | 躯干 / 左臂 |
| 6 | `RIGHT_SHOULDER` | Right Shoulder | 躯干 / 右臂 |
| 7 | `LEFT_ELBOW` | Left Elbow | 左臂 |
| 8 | `RIGHT_ELBOW` | Right Elbow | 右臂 |
| 9 | `LEFT_WRIST` | Left Wrist | 左臂 |
| 10 | `RIGHT_WRIST` | Right Wrist | 右臂 |
| 11 | `LEFT_HIP` | Left Hip | 躯干 / 左腿 |
| 12 | `RIGHT_HIP` | Right Hip | 躯干 / 右腿 |
| 13 | `LEFT_KNEE` | Left Knee | 左腿 |
| 14 | `RIGHT_KNEE` | Right Knee | 右腿 |
| 15 | `LEFT_ANKLE` | Left Ankle | 左腿 |
| 16 | `RIGHT_ANKLE` | Right Ankle | 右腿 |

表中枚举编号来自 `KeypointType`，英文显示名来自 `GetKeypointName()` 内部的静态名称数组；两者顺序一致，因此 `GetKeypointName(11)` 对应 `Left Hip`，也就是 `LEFT_HIP`。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L15-L34), [pose_utils.cpp](pose_utils.cpp#L249-L261)

## 3. 单个关键点：`KeyPoint`

单个关键点由 `KeyPoint` 表示，包含 `x`、`y`、`confidence` 和 `pos3d` 四个字段。对初学者来说，最重要的是先把它理解为“一个图像上的人体部位点”：`x` 和 `y` 是图像坐标，`confidence` 是关键点可见性/置信度，`pos3d` 是深度融合之后才会填充的三维位置。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L36-L46)

| 字段 | 类型 | 含义 | 初始值行为 |
|---|---|---|---|
| `x` | `float` | 关键点在图像中的 X 坐标 | 默认构造为 `0` |
| `y` | `float` | 关键点在图像中的 Y 坐标 | 默认构造为 `0` |
| `confidence` | `float` | 关键点置信度或可见性，范围注释为 `[0-1]` | 默认构造为 `0` |
| `pos3d` | `cv::Point3f` | 深度融合后的 3D 位置 | 默认构造为 `(0,0,0)` |

`KeyPoint(float x_, float y_, float conf_)` 构造函数只设置二维坐标和置信度，`pos3d` 仍初始化为 `(0,0,0)`；这说明二维姿态检测结果本身不自动包含 3D 坐标。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L36-L46)

## 4. 一个人的姿态结果：`PoseResult`

一个人的完整检测结果由 `PoseResult` 表示，它包含 `bbox`、`box_confidence`、`keypoints` 和 `person_id`。其中 `keypoints` 是 `std::vector<KeyPoint>`，构造函数会立即 `resize(17)`，这保证了后续代码可以按 COCO 索引访问 `0..16` 的关键点。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L48-L58)

| 字段 | 类型 | 作用 |
|---|---|---|
| `bbox` | `cv::Rect` | 人体检测框 |
| `box_confidence` | `float` | 人体检测置信度 |
| `keypoints` | `std::vector<KeyPoint>` | 固定 17 个 COCO 关键点 |
| `person_id` | `int` | 可选跟踪 ID，默认 `-1` |

`PoseResult` 的设计把“人体是否存在”和“每个身体部位是否可靠”分成两层：`box_confidence` 表示整个人体框的检测置信度，`KeyPoint::confidence` 表示单个关键点的可见性/置信度。后处理代码也正是先用 `conf_threshold_` 过滤人体候选，再逐个填充 17 个关键点。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L48-L58), [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L247-L276)

## 5. YOLOv8-pose 输出如何变成 17 个关键点

`Postprocess()` 明确记录当前 YOLOv8-pose 输出格式为 `[1, 56, 8400]`，其中 `56 = 4 + 1 + 17*3`：前 4 个元素是框的中心点和宽高，第 5 个元素是人体检测置信度，后面 51 个元素是 17 个关键点的 `x, y, visibility`。代码注释还说明这里的 `visibility` 在实现中作为类似置信度的值使用。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L215-L225)

解析时，代码先把输出从 `[1, 56, 8400]` 转置成更容易访问的 `[8400, 56]`，然后对每个候选人体读取 `ptr[0..4]` 得到 bbox 和人体置信度；如果人体置信度低于 `conf_threshold_`，该候选会被跳过。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L227-L256)

关键点读取从 `ptr[5]` 开始，循环 `k = 0..16`，每个关键点使用 `ptr[5 + k*3 + 0]` 作为 `x`，`ptr[5 + k*3 + 1]` 作为 `y`，`ptr[5 + k*3 + 2]` 作为 `visibility`，最终写成 `KeyPoint(kp_x, kp_y, kp_visibility)`。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L267-L276)

## 6. 坐标回映射：关键点为什么能画在原图上

模型输入经过 letterbox resize 和 padding，因此模型输出的关键点坐标最初位于模型输入尺寸坐标系中。后处理在 NMS 后调用 `RescaleCoordinates()`，对 bbox 和所有关键点执行同一个 `rescale` 逻辑：先减去 padding，再除以 `scale_factor_`，最后 clamp 到原图边界。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L281-L289), [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L348-L380)

对可视化来说，这一步非常关键：`DrawPoses()` 直接使用 `kp.x` 和 `kp.y` 调用 `cv::Point(cvRound(kp.x), cvRound(kp.y))`，因此只有在坐标已经回到原图空间后，骨架线和关键点圆点才会出现在正确位置。Sources: [pose_utils.cpp](pose_utils.cpp#L125-L137), [pose_utils.cpp](pose_utils.cpp#L158-L161)

## 7. 骨架连接表：`GetCocoSkeleton()`

骨架不是模型直接输出的，而是项目根据 COCO 17 关键点索引手工定义的连线表。`SkeletonConnection` 保存 `start_idx`、`end_idx` 和 `color`，`GetCocoSkeleton()` 返回一组固定连接，表示头部、躯干、左右手臂和左右腿。Sources: [pose_utils.h](pose_utils.h#L13-L24), [pose_utils.cpp](pose_utils.cpp#L10-L42)

| 身体区域 | 连接 | 颜色含义 |
|---|---|---|
| Head | `NOSE-LEFT_EYE`, `NOSE-RIGHT_EYE`, `LEFT_EYE-LEFT_EAR`, `RIGHT_EYE-RIGHT_EAR` | 黄色 |
| Torso | `LEFT_SHOULDER-RIGHT_SHOULDER`, `LEFT_SHOULDER-LEFT_HIP`, `RIGHT_SHOULDER-RIGHT_HIP`, `LEFT_HIP-RIGHT_HIP` | 青色 |
| Left arm | `LEFT_SHOULDER-LEFT_ELBOW`, `LEFT_ELBOW-LEFT_WRIST` | 绿色 |
| Right arm | `RIGHT_SHOULDER-RIGHT_ELBOW`, `RIGHT_ELBOW-RIGHT_WRIST` | 蓝色 |
| Left leg | `LEFT_HIP-LEFT_KNEE`, `LEFT_KNEE-LEFT_ANKLE` | 品红 |
| Right leg | `RIGHT_HIP-RIGHT_KNEE`, `RIGHT_KNEE-RIGHT_ANKLE` | 橙色 |

这张表完全对应 `GetCocoSkeleton()` 的返回值；例如左腿由左髋到左膝、左膝到左踝两条线组成，右腿由右髋到右膝、右膝到右踝两条线组成。Sources: [pose_utils.cpp](pose_utils.cpp#L14-L40)

## 8. `DrawPoses()` 的绘制顺序

`DrawPoses()` 的绘制顺序是：先可选绘制人体框和框置信度，再可选绘制骨架线，最后可选绘制关键点圆点。函数参数包括 `show_bbox`、`show_keypoints`、`show_skeleton` 和 `keypoint_conf_threshold`，因此调用方可以独立控制框、点、线是否显示。Sources: [pose_utils.h](pose_utils.h#L37-L52), [pose_utils.cpp](pose_utils.cpp#L100-L123)

骨架线绘制时，函数遍历 `GetCocoSkeleton()` 返回的每条连接，从 `pose.keypoints[conn.start_idx]` 和 `pose.keypoints[conn.end_idx]` 取出两个端点；只有两个端点的 `confidence` 都大于阈值时，才调用 `cv::line()` 画线。Sources: [pose_utils.cpp](pose_utils.cpp#L125-L140)

关键点圆点绘制时，函数遍历 `pose.keypoints`，只绘制 `confidence > keypoint_conf_threshold` 的点；圆点颜色按置信度分为红色、橙色和黄色，代码阈值分别是 `>0.8`、`>0.6` 和剩余可显示区间。Sources: [pose_utils.cpp](pose_utils.cpp#L142-L168)

## 9. 可视化置信度阈值

头文件给 `DrawPoses()` 的 `keypoint_conf_threshold` 默认值是 `0.5f`，但主循环调用时传入的是 `0.3f`，并且代码注释说明这是为了“使用更低的关键点阈值”。因此，当前主显示窗口会显示置信度大于 `0.3` 的关键点和骨架连接。Sources: [pose_utils.h](pose_utils.h#L46-L52), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L690-L697)

| 场景 | 阈值 | 影响 |
|---|---:|---|
| `DrawPoses()` 默认参数 | `0.5` | 只有较可靠关键点会显示 |
| 当前主循环调用 | `0.3` | 更容易显示被遮挡或较低置信度的关键点 |
| 骨架线判断 | 使用同一个阈值 | 连接两端都超过阈值才画线 |

这个阈值同时影响点和线：点的判断是单个关键点超过阈值，线的判断是连接两端关键点都超过阈值。Sources: [pose_utils.cpp](pose_utils.cpp#L131-L137), [pose_utils.cpp](pose_utils.cpp#L147-L161)

## 10. 运行时如何打开或关闭关键点与骨架

主程序启动时会打印控制说明，其中 `k` 用于切换关键点显示，`t` 用于切换骨架显示，`i` 用于切换信息覆盖层；对应的初始显示状态是 `show_bbox = false`、`show_keypoints = true`、`show_skeleton = true`、`show_info = true`。Sources: [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L586-L607)

在每帧显示时，主循环调用 `DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.3f)`，随后在 `show_info` 为真时调用 `DrawPoseInfo(display, poses, false)`；这说明关键点和骨架由 `DrawPoses()` 负责，文字信息由 `DrawPoseInfo()` 负责。Sources: [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L690-L697)

## 11. 调试输出：如何看懂 `KP0..KP16`

当检测到人体时，主循环每 30 次左右会打印第一人的关键点调试信息，格式为 `KP<index>: conf=<value> pos=(x,y)`。这里的 `KP0` 到 `KP16` 对应 COCO 17 枚举顺序，例如 `KP0` 是鼻子，`KP11` 是左髋，`KP16` 是右脚踝。Sources: [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L663-L675), [yolo_pose_detector.h](yolo_pose_detector.h#L15-L34)

这类调试输出适合新手确认两个问题：第一，模型是否真的返回了 17 个关键点；第二，某些骨架线不显示时，是因为端点坐标异常，还是因为端点置信度低于绘制阈值。代码中调试输出打印的是 `kp.confidence` 和整数化后的 `kp.x, kp.y`。Sources: [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L666-L674), [pose_utils.cpp](pose_utils.cpp#L131-L137)

## 12. 关键点名称在 3D Skeleton 面板中的使用

`GetKeypointName()` 将索引 `0..16` 转换为英文名称；在主程序的 Body Frame Metrics 面板中，代码遍历当前跟踪人体的 `poses[tracked_pose_index].keypoints`，用 `GetKeypointName(k)` 作为每行前缀显示关键点名称。Sources: [pose_utils.cpp](pose_utils.cpp#L249-L261), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L1188-L1213)

当床面坐标系已准备好且存在跟踪人体时，面板会显示 “3D Skeleton (trampoline coords, mm)” 并列出每个关键点的三维坐标；如果某个关键点无效，则该行显示 `N/A`。这里的名称仍然来自 COCO 17 的固定索引顺序。Sources: [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L1188-L1219), [pose_utils.cpp](pose_utils.cpp#L249-L261)

## 13. 初学者应记住的三个不变量

第一，`PoseResult::keypoints` 的长度在构造时固定调整为 17，因此 `NOSE` 到 `RIGHT_ANKLE` 的枚举索引就是访问关键点的稳定协议。Sources: [yolo_pose_detector.h](yolo_pose_detector.h#L15-L34), [yolo_pose_detector.h](yolo_pose_detector.h#L48-L57)

第二，模型输出中的关键点不是从索引 `6` 开始，而是从索引 `5` 开始；代码注释专门强调 “Keypoints start at index 5 (not 6!)”。Sources: [yolo_pose_detector.cpp](yolo_pose_detector.cpp#L267-L276)

第三，骨架线是否显示不只取决于某条连线是否存在于 `GetCocoSkeleton()`，还取决于连线两端关键点是否都超过当前 `keypoint_conf_threshold`。Sources: [pose_utils.cpp](pose_utils.cpp#L10-L42), [pose_utils.cpp](pose_utils.cpp#L125-L140)

## 14. 下一步阅读

如果你想继续理解这些 2D 关键点如何和深度图结合，请阅读 [OAK DepthAI 管线、RGB-Depth 配对与时间同步](15-oak-depthai-guan-xian-rgb-depth-pei-dui-yu-shi-jian-tong-bu) 与 [深度图单位、相机内参与像素反投影](16-shen-du-tu-dan-wei-xiang-ji-nei-can-yu-xiang-su-fan-tou-ying)；如果你想理解后续如何利用髋、肩、膝、踝等关键点做业务判断，请阅读 [团身、屈体、直体三种基础姿态判定](22-tuan-shen-qu-ti-zhi-ti-san-chong-ji-chu-zi-tai-pan-ding)。Sources: [pose_utils.h](pose_utils.h#L27-L35), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L760-L794), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L221-L248)