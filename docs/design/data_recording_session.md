# 数据记录开关与会话模型设计文档

## 1. 概述

本文档描述蹦床AI裁判系统中数据记录开关与会话模型的设计方案。

### 1.1 设计目标
- 提供运行时录制开关，避免无效数据积累
- 建立会话概念，组织输出数据
- 支持落点数据的持久化存储

### 1.2 适用范围
- `get_pose_indemind_left.cpp` 主程序
- Phase 0 数据采集功能

## 2. 架构设计

### 2.1 核心数据结构

```cpp
// 运行时标志
struct RuntimeFlags {
  bool record_enabled = false;  // 录制开关，默认关闭
};
static RuntimeFlags g_runtime_flags;

// 数据会话
struct DataSession {
  std::string session_id;           // 会话ID (YYYYMMDD_HHMMSS)
  std::string output_dir;           // 输出目录 (runs/session_id/)
  std::chrono::system_clock::time_point start_time;
  bool active = false;
};
static DataSession g_current_session;

// 落点记录
struct LandingPoint {
  int landing_id;           // 落点编号
  int time_minutes;         // 时间-分钟
  int time_seconds;         // 时间-秒
  int64_t t_ms_since_start; // 精确时间戳(ms)
  double new_frame_x;       // 新坐标系X
  double new_frame_y;       // 新坐标系Y
  double new_frame_z;       // 新坐标系Z
};
```

### 2.2 状态机

```
[OFF] ---(R键)---> [ON] ---(R键)---> [OFF]
  |                  |
  |                  +--- 创建新会话
  |                  +--- 清空落点缓存
  |
  +--- 不记录落点
```

## 3. 功能模块

### 3.1 录制开关 (R/r键)

**触发条件:** 用户按下 R 或 r 键

**OFF → ON 流程:**
1. 调用 `CreateNewSession()` 创建新会话
2. 生成时间戳格式的 session_id
3. 创建输出目录 `runs/YYYYMMDD_HHMMSS/`
4. 清空之前的落点缓存
5. 设置 `record_enabled = true`

**ON → OFF 流程:**
1. 设置 `record_enabled = false`
2. 会话保持活跃状态（可继续保存）

### 3.2 落点记录

**记录条件:**
- `g_runtime_flags.record_enabled == true`
- 检测到有效落点

**数据流:**
```
落点检测 → CheckLandingPoint() → RecordLandingPoint() → landing_points_[]
```

### 3.3 数据保存 (S键)

**触发条件:** 用户按下 S 键

**前置条件:** 会话已激活 (`g_current_session.active == true`)

**输出格式:** CSV 文件
```csv
landing_id,time_min,time_sec,t_ms,new_x,new_y,new_z
1,0,5,5123,100.5,200.3,-50.2
```

### 3.4 清空缓存 (C键)

**功能:** 清空内存中的落点记录，不影响已保存的文件

## 4. 目录结构

```
YOLO/
├── runs/                          # 数据输出根目录
│   ├── 20250115_143052/          # 会话目录
│   │   └── landing_points.csv    # 落点数据
│   └── 20250115_150030/
│       └── landing_points.csv
```

## 5. 性能监控

### 5.1 PerfStats 结构

```cpp
struct PerfStats {
  std::deque<double> inference_ms;      // 推理耗时
  std::deque<double> depth_map_ms;      // 深度处理耗时
  std::deque<double> landing_detect_ms; // 落点检测耗时
  static constexpr size_t MAX_SAMPLES = 100;
};
```

### 5.2 输出格式

每2秒输出一次:
```
[Perf] Inference: 15.2/23.0ms | Depth: 0.8/1.2ms | Landing: 0.1/0.3ms
```

## 6. 用户界面

### 6.1 状态面板 (右上角)

| 元素 | 说明 |
|------|------|
| REC: ON/OFF | 录制状态（红色/灰色）|
| LP: N | 落点计数 |
| Last: (x,y,z) | 最后落点坐标 |

### 6.2 快捷键

| 按键 | 功能 |
|------|------|
| R/r | 切换录制开关 |
| C/c | 清空落点缓存 |
| S/s | 保存落点到CSV |

## 7. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2025-01-15 | 初始设计 |
