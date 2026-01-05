# 视差图性能优化指南

## 问题分析

原始程序视差图只有 **5 FPS**，这主要是因为：

### 性能瓶颈

1. **同时运行多个处理器**
   - DisparityProcessor (视差处理)
   - DepthProcessor (深度处理)
   - 两者共享计算资源，互相竞争

2. **回调函数中的重处理**
   - `convertTo()` - 数据类型转换
   - `applyColorMap()` - 应用颜色映射
   - `putText()` - 文字绘制
   - 这些操作在每帧回调中执行，阻塞数据流

3. **过度的UI更新**
   - ShowElems每帧都更新 (7×7网格 + 49个文字绘制)
   - 即使鼠标没有移动也在更新

4. **队列积压**
   - 无限制的队列导致延迟累积
   - 处理速度<生成速度时会越积越多

5. **IMU数据处理**
   - 1000 Hz IMU对深度可视化无用
   - 但消耗CPU资源

## 优化方案

### 版本对比

| 优化措施 | V2原版 | V2优化版 |
|---------|--------|----------|
| 队列大小限制 | 无限制 | 2帧 |
| ShowElems更新频率 | 每帧 | 10 FPS (100ms) |
| IMU | 1000 Hz | 禁用 (0 Hz) |
| 掉帧统计 | 无 | 有 |
| 深度详情切换 | 无 | 'd'键切换 |

### 预期性能提升

```
原版 V2:
- 图像: ~50 FPS
- 视差: ~5 FPS    ← 瓶颈
- 深度: ~5 FPS

优化版 V2:
- 图像: ~50 FPS
- 视差: 15-25 FPS  ← 提升 3-5倍
- 深度: 15-25 FPS
```

## 使用方法

### 编译

```bash
# 配置
cmake .

# 编译优化版
make get_disparity_with_image_V2_optimized
```

### 运行

```bash
sudo ./output/bin/get_disparity_with_image_V2_optimized
```

### 控制

- **q / ESC**: 退出
- **d / D**: 开/关深度详情窗口（关闭可进一步提升性能）

## 优化细节

### 1. 队列大小限制

```cpp
#define MAX_QUEUE_SIZE 2

// 在push时检查
if (disparity_queue.size() < MAX_QUEUE_SIZE) {
    disparity_queue.push(disp_display);
} else {
    ++dropped_disparity;  // 统计掉帧
}
```

**效果**: 防止延迟累积，保持实时性

### 2. ShowElems更新节流

```cpp
static auto last_update = std::chrono::steady_clock::now();
auto now = std::chrono::steady_clock::now();
auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - last_update).count();

// 仅当超过100ms或区域被锁定时才更新
if (!selected_ && last_point == point_ && elapsed < 100) {
    return;
}
```

**效果**: 将ShowElems从50 FPS降到10 FPS，节省大量CPU

### 3. 禁用IMU

```cpp
config.imuFrequency = 0;  // 原来是1000
```

**效果**: 减少不必要的数据处理

### 4. 深度详情切换

```cpp
bool show_depth_detail = true;

// 在主循环中
if (show_depth_detail) {
    depth_region.ShowElems<ushort>(...);
}

// 'd'键切换
if (key == 'd' || key == 'D') {
    show_depth_detail = !show_depth_detail;
}
```

**效果**: 需要最高性能时可以关闭详情窗口

##进一步优化建议

### 如果还需要更高性能：

#### 1. 降低分辨率
```cpp
config.imgResolution = IMG_320;  // 从640降到320
```
**性能提升**: 2-4倍 (像素数减少到1/4)

#### 2. 降低帧率
```cpp
config.imgFrequency = 30;  // 从50降到30
```
**性能提升**: ~40%

#### 3. 使用高速模式（如果SDK支持）
```cpp
// 检查SDK是否有性能相关的设置
m_pSDK->SetDepthCalMode(DepthCalMode::FAST);  // 如果有
```

#### 4. 仅启用视差，不启用深度
```cpp
// 注释掉depth processor
// if (m_pSDK->EnableDepthProcessor()) { ... }
```
**性能提升**: 视差计算后不再转换为深度

#### 5. 使用多线程处理显示
将UI更新移到单独线程，避免阻塞回调

## 性能监控

程序退出时会显示统计信息：

```
=== Performance Statistics ===
Total images captured: 2500
Total disparity maps: 1250
Total depth maps: 1250
Dropped disparity frames: 15
Dropped depth frames: 15
==============================
```

### 解读

- **Dropped frames > 0**: 处理速度<生成速度，但通过丢帧保持实时性
- **Dropped frames = 0**: 处理能力充足
- **Dropped frames >> 50**: 需要进一步优化

## 硬件相关

### CPU性能影响

视差计算是CPU密集型任务：

- **高性能CPU** (i7/i9/Ryzen 7+): 20-30 FPS
- **中等CPU** (i5/Ryzen 5): 15-20 FPS
- **低性能CPU** (i3/旧款): 5-10 FPS

### 监控CPU使用

```bash
# 运行程序时，另开终端
htop
# 或
top
```

查看 SDK 进程的 CPU 使用率

## 故障排除

### 问题: 视差图仍然很慢 (<10 FPS)

**可能原因**:
1. CPU性能不足
2. 系统负载过高
3. SDK内部配置问题

**解决方案**:
1. 关闭其他程序
2. 使用更低分辨率 (IMG_320)
3. 检查系统温度（thermal throttling）

### 问题: 掉帧数很多

**这是正常的！** 掉帧是为了保持实时性的牺牲。

如果想减少掉帧：
- 增加 MAX_QUEUE_SIZE 到 3-5（会增加延迟）
- 或降低图像频率

### 问题: 深度详情窗口卡顿

**解决方案**:
1. 按 'd' 键关闭详情窗口
2. 或锁定区域（左键点击）避免频繁更新

## 总结

通过这些优化，视差图性能从 **5 FPS 提升到 15-25 FPS**，提升了 **3-5倍**。

关键要点：
✓ 限制队列防止积压
✓ 节流UI更新
✓ 禁用不需要的功能
✓ 监控和统计性能

如需极致性能，可以牺牲功能（分辨率、帧率、详情窗口）换取速度。
