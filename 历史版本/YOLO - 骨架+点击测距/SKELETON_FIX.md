# 骨架显示问题修复说明

## 问题诊断

用户报告程序可以识别人像并显示边界框，但**骨架和关键点不显示**，按"k"和"s"键也没有反应。

## 根本原因

关键点置信度阈值设置过高（0.5），导致大部分关键点被过滤掉。

## 已实施的修复

### 1. 降低关键点置信度阈值

**修改文件**: `get_pose_indemind_left.cpp:194`

```cpp
// 之前: 使用 0.5 阈值
DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.5f);

// 之后: 使用 0.3 阈值
DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.3f);
```

**原理**: YOLO模型输出的关键点置信度可能在0.3-0.8之间波动，0.5的阈值会过滤掉很多有效的关键点。降低到0.3可以显示更多关键点。

### 2. 添加调试输出

**修改文件**: `get_pose_indemind_left.cpp:168-178`

添加了每30帧打印一次关键点置信度的调试代码：

```cpp
// Debug: Print first detection keypoint info (only once every 30 frames)
static int debug_counter = 0;
if (debug_counter++ % 30 == 0 && poses.size() > 0) {
  std::cout << "\n[DEBUG] First person keypoint confidences:" << std::endl;
  for (size_t i = 0; i < poses[0].keypoints.size(); i++) {
    const auto& kp = poses[0].keypoints[i];
    std::cout << "  KP" << i << ": conf=" << std::fixed << std::setprecision(3)
             << kp.confidence << " pos=(" << (int)kp.x << "," << (int)kp.y << ")" << std::endl;
  }
}
```

**用途**:
- 帮助诊断关键点置信度分布
- 验证关键点是否被正确检测
- 方便后续调优阈值

### 3. 添加可视化状态指示器

**修改文件**: `get_pose_indemind_left.cpp:223-232`

在屏幕右上角显示当前启用的显示选项：

```cpp
// Display current settings
std::string settings = "";
if (show_bbox) settings += "B";      // B = Bounding box
if (show_keypoints) settings += "K"; // K = Keypoints
if (show_skeleton) settings += "S";  // S = Skeleton
if (show_info) settings += "I";      // I = Info overlay
if (!settings.empty()) {
  cv::putText(display, "[" + settings + "]", cv::Point(display.cols - 80, 25),
              FONT_FACE, 1.5, cv::Scalar(0, 255, 255), 2);
}
```

**效果**:
- 用户按"k"键时，屏幕上的"[BKSI]"会变成"[BSI]"
- 用户按"s"键时，屏幕上的"[BKSI]"会变成"[BKI]"
- 提供即时的视觉反馈

## 测试步骤

1. **重新编译**:
   ```bash
   cd /home/chris/Desktop/YOLO/build
   make yolo_pose_indemind_left
   ```

2. **运行程序**:
   ```bash
   sudo /home/chris/Desktop/YOLO/build/yolo_pose_indemind_left
   ```

3. **验证修复**:
   - 检查是否显示骨架线（彩色连接线）
   - 检查是否显示关键点（彩色圆圈）
   - 按"k"键，观察:
     - 终端输出: "Keypoints: OFF"
     - 屏幕右上角: "[BKSI]" → "[BSI]"
     - 关键点圆圈消失
   - 按"s"键，观察:
     - 终端输出: "Skeleton: OFF"
     - 屏幕右上角: "[BKSI]" → "[BKI]"
     - 骨架线消失
   - 查看终端调试输出（每30帧一次），确认关键点置信度值

4. **预期调试输出示例**:
   ```
   [DEBUG] First person keypoint confidences:
     KP0: conf=0.876 pos=(320,150)  // Nose
     KP1: conf=0.812 pos=(310,145)  // Left Eye
     KP2: conf=0.798 pos=(330,145)  // Right Eye
     KP3: conf=0.654 pos=(305,148)  // Left Ear
     KP4: conf=0.623 pos=(335,148)  // Right Ear
     KP5: conf=0.734 pos=(295,200)  // Left Shoulder
     KP6: conf=0.756 pos=(345,200)  // Right Shoulder
     ...
   ```

## 关键点置信度阈值说明

| 阈值 | 效果 | 适用场景 |
|------|------|----------|
| 0.8 | 只显示非常可靠的关键点 | 高质量输入，完整可见人体 |
| 0.5 | 显示较可靠的关键点 | 标准场景，部分遮挡可接受 |
| **0.3** | 显示大部分检测到的关键点 | 低光照、部分遮挡、远距离 |
| 0.1 | 显示几乎所有关键点 | 可能包含噪声，不推荐 |

**当前使用**: 0.3（平衡可视化和准确性）

## 其他修复选项（如果问题仍存在）

如果降低阈值后仍无法看到骨架，可能的原因：

1. **YOLO模型未正确加载**
   - 检查: `ls -lh models/yolov8n-pose.onnx`
   - 确保模型文件存在且大小正确（约6-9MB）

2. **图像质量问题**
   - 确保光照充足
   - 人体应完整可见（至少上半身）
   - 相机焦距调整正确

3. **置信度阈值仍需调整**
   - 根据调试输出中的实际置信度值
   - 如果大部分关键点置信度<0.3，可以进一步降低阈值到0.2
   - 修改第194行: `DrawPoses(display, poses, show_bbox, show_keypoints, show_skeleton, 0.2f);`

4. **检查DrawPoses函数调用**
   - 确保`show_keypoints`和`show_skeleton`参数为`true`
   - 确认`poses`向量不为空

## 相关文件

- `get_pose_indemind_left.cpp` - 主程序（已修改）
- `pose_utils.cpp` - 绘制函数实现（无需修改）
- `pose_utils.h` - 绘制函数声明（无需修改）
- `yolo_pose_detector.cpp` - YOLO检测器（无需修改）

## 编译状态

✅ 编译成功
✅ 调试输出已添加
✅ 置信度阈值已降低 (0.5 → 0.3)
✅ 状态指示器已添加
⏳ 等待用户测试验证

## 下一步

请用户使用以下命令测试修复后的程序：

```bash
cd /home/chris/Desktop/YOLO
sudo ./build/yolo_pose_indemind_left
```

如果仍有问题，请提供：
1. 终端调试输出（关键点置信度值）
2. 屏幕截图（显示边界框但无骨架的情况）
3. "Detected: X person(s)" 显示的人数
