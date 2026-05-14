这一页解释 RGBD 链路中一个最基础但最容易出错的约定：**深度图像素值以毫米为单位，像素坐标通过相机内参逆矩阵反投影为相机坐标系下的 3D 点**。本文只覆盖深度单位、内参矩阵来源、反投影公式与代码落点；深度采样鲁棒性请继续阅读 [鲁棒深度采样与无效深度过滤策略](17-lu-bang-shen-du-cai-yang-yu-wu-xiao-shen-du-guo-lu-ce-lue)，RGB-Depth 配对与同步请回看 [OAK DepthAI 管线、RGB-Depth 配对与时间同步](15-oak-depthai-guan-xian-rgb-depth-pei-dui-yu-shi-jian-tong-bu)。Sources: [README_OAK_RGBD.md](README_OAK_RGBD.md#L10-L17), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L646-L659), [app/depth_utils.cpp](app/depth_utils.cpp#L6-L28)

## 架构假设与验证结论

本文的验证假设是：OAK 新链路把 DepthAI 输出的深度帧保持为 `CV_16UC1` 毫米图，并读取 CAM_A 在 640×400 输出尺寸下的内参矩阵；业务层随后使用共享的 `cv_in_left_inv` 对 YOLO 关键点像素做反投影，得到单位同样为毫米的相机坐标。代码验证结果与该假设一致：README 明确写出深度为 `CV_16UC1`、millimeters、aligned to CAM_A；OAK 采集模块检查深度尺寸与类型为 640×400、`CV_16UC1`；主程序读取 `GetCameraMatrix()` 与 `GetCameraMatrixInv()` 后打印 `fx/fy/cx/cy`。Sources: [README_OAK_RGBD.md](README_OAK_RGBD.md#L10-L17), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L433-L455), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L646-L659)

```mermaid
flowchart LR
    A[OAK CAM_A RGB<br/>640x400] --> B[YOLO Pose<br/>输出关键点 u,v]
    C[StereoDepth depth<br/>CV_16UC1 毫米] --> D[同像素读取 Z_mm]
    E[DepthAI Calibration<br/>CAM_A K] --> F[K_inv]
    B --> G[像素齐次向量<br/>u,v,1]
    D --> H[Z = double z_mm]
    F --> I[反投影<br/>P_cam = K_inv * Z * [u v 1]^T]
    G --> I
    H --> I
    I --> J[相机坐标点<br/>X,Y,Z 单位 mm]
```

上图中的关键约束是 **RGB 图、深度图与内参必须属于同一成像平面和同一输出尺寸**。OAK 管线通过 `StereoDepth::setOutputSize(cfg_.rgb_width, cfg_.rgb_height)` 设置深度输出大小，通过 `rgb_out->link(stereo->inputAlignTo)` 将深度对齐到 CAM_A RGB 输出；随后采集循环只接受 RGB 为 `CV_8UC3` 且深度为 `CV_16UC1`、尺寸均为配置宽高的帧。Sources: [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L353-L370), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L433-L455)

## 深度图单位：OAK 直接使用毫米，旧 INDEMIND 链路转换为毫米

OAK 链路的深度图在工程中命名为 `depth_mm`，类型为 `cv::Mat`，与 RGB 图一起封装在 `TimedRgbdFrame` 中；采集模块将 DepthAI 的深度帧通过 `DepthToU16()` 转为或保持 `CV_16UC1`，再写入 `frame.depth_mm`，业务层直接把 `rgbd.depth_mm` 作为 `depth_data` 使用，没有乘以 1000 的转换步骤。Sources: [app/oak_rgbd_capture.h](app/oak_rgbd_capture.h#L35-L40), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L142-L159), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L450-L455), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L731-L747)

README 对这一单位约定给出了迁移说明：OAK `StereoDepth.depth` 已经是毫米，新代码不再把深度乘以 1000；这与旧 INDEMIND 入口形成对照，旧入口的深度回调接收 `cv::Mat depth` 后显式执行 `depth.convertTo(depth_mm, CV_16U, 1000.0)`，把米转换为毫米后再入队。Sources: [README_OAK_RGBD.md](README_OAK_RGBD.md#L54-L58), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L790-L802)

| 链路 | 深度输入约定 | 代码处理 | 业务层变量 | 反投影单位 |
|---|---:|---|---|---:|
| OAK RGBD 新链路 | `CV_16UC1`，毫米 | 直接保留/转换为 `CV_16U`，不乘 1000 | `depth_mm` / `depth_data` | 毫米 |
| INDEMIND 旧链路 | 回调深度按米处理 | `convertTo(..., CV_16U, 1000.0)` | `depth_mm` / `depth_data` | 毫米 |

这张表的工程意义是：进入反投影代码之前，两个入口都把深度统一成 **uint16 毫米深度图**；因此后续公式中的 `Z` 可以直接写成 `static_cast<double>(z_mm)`，不需要在关键点反投影处区分设备来源。Sources: [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L875-L889), [get_pose_indemind_left.cpp](get_pose_indemind_left.cpp#L790-L802)

## 相机内参矩阵 K：fx、fy、cx、cy 的存放方式

项目将相机内参与其逆矩阵抽象为两个全局 `cv::Mat`：`cv_in_left` 与 `cv_in_left_inv`。头文件暴露这两个矩阵，源文件定义它们；这使主循环、鼠标深度交互、ROI 平面拟合等模块可以共享同一套相机模型。Sources: [app/camera_intrinsics.h](app/camera_intrinsics.h#L1-L9), [app/camera_intrinsics.cpp](app/camera_intrinsics.cpp#L1-L5)

OAK 链路从 DepthAI 标定中读取 CAM_A 在 `cfg_.rgb_width`、`cfg_.rgb_height` 输出尺寸下的内参，然后构造 3×3 的 `CV_64F` 矩阵：`K(0,0)=fx`、`K(1,1)=fy`、`K(0,2)=cx`、`K(1,2)=cy`，其余保持单位矩阵默认值；随后缓存 `K_` 与 `K.inv()`，主程序启动成功后再拷贝到全局 `cv_in_left` 与 `cv_in_left_inv`。Sources: [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L293-L306), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L246-L254), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L646-L659)

用矩阵形式表示，代码中的内参矩阵就是标准针孔相机模型：`K = [[fx, 0, cx], [0, fy, cy], [0, 0, 1]]`。这里的 `fx/fy` 是以像素为单位的焦距，`cx/cy` 是主点像素坐标；它们必须对应当前参与 YOLO 与深度查表的图像分辨率，OAK 当前打印的分辨率为 640×400。Sources: [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L293-L306), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L654-L659)

## 像素反投影：从 `[u, v, 1]` 到相机坐标 `[X, Y, Z]`

核心反投影代码出现在关键点 3D 计算处：先把 YOLO 关键点坐标转成整数像素 `px/py`，确认像素落在深度图范围内，再取得该像素附近的 `z_mm`；随后构造齐次像素向量 `[px, py, 1]^T`，计算 `kp_camera_cor = cv_in_left_inv * Z * kp_img_cor`，最后将结果写入 `cv::Point3d cam_pt`。Sources: [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L863-L889)

等价的数学表达是：`P_cam = Z * K^{-1} * [u, v, 1]^T`，展开后得到 `X = (u - cx) * Z / fx`、`Y = (v - cy) * Z / fy`、`Z = z_mm`。由于代码中的 `Z` 来自 `uint16_t z_mm` 并转换为 `double`，所以 `X/Y/Z` 的单位全部为毫米；这也是调试输出中鼠标深度信息显示 `unit: mm` 的原因。Sources: [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L875-L889), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L673-L681), [app/depth_region.h](app/depth_region.h#L121-L130)

鼠标交互路径使用同一公式验证了业务层约定：`DepthRegion` 将当前鼠标点写成 `[point_.x, point_.y, 1]^T`，通过 `RobustDepthMedianU16()` 取得 `z_mm`，再执行 `mouse_left_cor = cv_in_left_inv * Z * mouse_img_cor`，最终把 `x/y/z` 作为相机坐标展示。Sources: [app/depth_region.h](app/depth_region.h#L111-L130)

## 反投影与投影的对偶关系

项目中也存在从相机坐标投回像素坐标的 `ProjectPoint()`，它先检查 `p_cam.z > 0` 与内参矩阵非空，然后计算 `pt_img = K * pt_cam / p_cam.z`，取前两个分量作为像素坐标。这与反投影公式正好互为对偶：反投影用 `K_inv` 和深度 `Z` 从像素恢复 3D，投影用 `K` 和 `z` 从 3D 恢复像素。Sources: [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L171-L180), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L1065-L1078)

| 方向 | 输入 | 矩阵 | 深度/归一化 | 输出 | 代码位置 |
|---|---|---|---|---|---|
| 像素 → 相机 3D | `[u, v, 1]` 与 `z_mm` | `K_inv` | 乘以 `Z` | `[X, Y, Z]` 毫米 | 关键点与鼠标反投影 |
| 相机 3D → 像素 | `[X, Y, Z]` | `K` | 除以 `Z` | `[u, v]` 像素 | 坐标轴与点可视化 |

这个对偶关系在调试三维结果时很有用：如果一个相机坐标点通过 `ProjectPoint()` 投回后明显偏离原关键点，优先检查三个事实是否同时成立：深度图是否与 RGB 对齐、`K` 是否对应当前分辨率、深度单位是否已经统一为毫米。Sources: [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L353-L370), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L444-L455), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L171-L180)

## 与 ROI 平面拟合的边界

ROI 平面拟合也复用相同的像素反投影基础：在 ROI mask 内按步长遍历深度像素，过滤 `0` 与 `>=10000` 的深度值后，把 `[x, y, 1]^T` 与 `z_mm` 反投影为 `pt_cam`，再把这些相机坐标点作为平面拟合样本。本文只说明这些样本如何从深度像素进入相机坐标系；RANSAC、床面坐标系构建与后续变换属于 [四点 ROI、RANSAC 平面拟合与床面坐标系构建](18-si-dian-roi-ransac-ping-mian-ni-he-yu-chuang-mian-zuo-biao-xi-gou-jian)。Sources: [app/depth_region.h](app/depth_region.h#L315-L335)

另一个相关但不同的操作是 `PixelToPlanePoint()`：它只用 `K_inv * [u, v, 1]^T` 得到一条相机射线，再与已知平面求交；这里没有直接读取该像素的深度值，而是用平面方程求交参数 `t`。因此它不是“深度图反投影”，而是“像素射线与平面求交”。Sources: [app/depth_region.h](app/depth_region.h#L1139-L1149)

## 实现检查清单

当你修改相机分辨率、替换设备或调整深度输出时，首先检查 `cfg_.rgb_width/rgb_height`、DepthAI `getCameraIntrinsics()` 的宽高参数、StereoDepth 输出尺寸、RGB/Depth 帧尺寸检查是否仍然一致；当前 OAK 配置与启动逻辑使用 640×400，并在启动后打印 `OAK CAM_A Intrinsics @ 640x400`。Sources: [app/oak_rgbd_capture.h](app/oak_rgbd_capture.h#L11-L17), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L293-L306), [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L353-L356), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L620-L659)

当你排查 3D 关键点异常时，按顺序确认：`cv_in_left` 与 `cv_in_left_inv` 非空，关键点像素没有越界，`z_mm` 已成功取得，反投影使用的是 `cv_in_left_inv * Z * [px, py, 1]^T`。这些条件在主程序中都有显式检查或控制流保护。Sources: [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L646-L652), [get_pose_oak_rgbd.cpp](get_pose_oak_rgbd.cpp#L869-L889)

## 下一步阅读

如果你想理解“为什么 RGB 和 Depth 能在同一像素坐标下查表”，继续阅读 [OAK DepthAI 管线、RGB-Depth 配对与时间同步](15-oak-depthai-guan-xian-rgb-depth-pei-dui-yu-shi-jian-tong-bu)；如果你想理解 `RobustDepthMedianU16()` 如何过滤无效值并选择局部中值，继续阅读 [鲁棒深度采样与无效深度过滤策略](17-lu-bang-shen-du-cai-yang-yu-wu-xiao-shen-du-guo-lu-ce-lue)；如果你关心反投影点如何进一步形成床面坐标系，继续阅读 [四点 ROI、RANSAC 平面拟合与床面坐标系构建](18-si-dian-roi-ransac-ping-mian-ni-he-yu-chuang-mian-zuo-biao-xi-gou-jian)。Sources: [app/oak_rgbd_capture.cpp](app/oak_rgbd_capture.cpp#L353-L370), [app/depth_utils.cpp](app/depth_utils.cpp#L6-L28), [app/depth_region.h](app/depth_region.h#L315-L335)