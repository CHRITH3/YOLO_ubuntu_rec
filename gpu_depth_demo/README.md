# GPU Depth Calculation Demo

使用 GPU 加速的立体匹配算法计算深度图，可大幅提升深度帧率。

## 性能对比

| 方法 | 预期帧率 (640x400) | 精度 |
|------|-------------------|------|
| INDEMIND SDK (CPU) | ~7 FPS | 高 |
| OpenCV CPU StereoBM | ~15 FPS | 中 |
| OpenCV CPU StereoSGBM | ~8 FPS | 高 |
| **OpenCV CUDA StereoBM** | **50-100 FPS** | 中 |
| **OpenCV CUDA StereoSGM** | **30-60 FPS** | 高 |

## 文件结构

```
gpu_depth_demo/
├── gpu_depth_demo.cpp      # 主程序源码
├── CMakeLists.txt          # CMake 配置
├── build.sh                # 编译脚本
├── install_opencv_cuda.sh  # OpenCV CUDA 安装脚本
└── README.md               # 本文档
```

## 快速开始

### 1. 编译（CPU 模式）

当前可以直接编译运行 CPU 模式：

```bash
cd gpu_depth_demo
./build.sh
sudo ./build/gpu_depth_demo --cpu
```

### 2. 安装 OpenCV CUDA（启用 GPU 加速）

运行安装脚本，将 OpenCV 4.8.0 with CUDA 安装到 `/opt/opencv4-cuda`：

```bash
./install_opencv_cuda.sh
```

**注意**：
- 安装到独立目录 `/opt/opencv4-cuda`
- **不会影响**系统现有的 OpenCV 3.4.3
- 编译时间约 30-60 分钟
- 需要约 10GB 磁盘空间

### 3. 重新编译 demo（GPU 模式）

安装完成后重新编译：

```bash
rm -rf build
./build.sh
sudo ./build/gpu_depth_demo
```

## 使用方法

```bash
# GPU 模式（默认，需要 OpenCV CUDA）
sudo ./build/gpu_depth_demo

# CPU 模式
sudo ./build/gpu_depth_demo --cpu

# 使用 SGM 算法（精度更高，速度较慢）
sudo ./build/gpu_depth_demo --sgm

# 显示帮助
./build/gpu_depth_demo --help
```

## 运行时控制

| 按键 | 功能 |
|------|------|
| `q` / `ESC` | 退出 |
| `g` | 切换 GPU/CPU 模式 |
| `s` | 切换 SGM/BM 算法 |
| `SPACE` | 保存当前帧 |

## 算法说明

### StereoBM (Block Matching)
- **速度**: 最快
- **精度**: 中等
- **适用**: 实时性要求高的场景

### StereoSGM (Semi-Global Matching)
- **速度**: 较慢
- **精度**: 高
- **适用**: 精度要求高的场景

## 双版本 OpenCV 共存

安装后系统中将有两个 OpenCV 版本：

| 版本 | 路径 | 用途 |
|------|------|------|
| OpenCV 3.4.3 | `/usr/local` (系统默认) | 其他项目 |
| OpenCV 4.8.0 + CUDA | `/opt/opencv4-cuda` | 本 demo |

```
安装后的状态:
├── /usr/local/                    <- OpenCV 3.4.3 (默认，自动使用)
└── /opt/opencv4-cuda/             <- OpenCV 4.8.0 (需要显式指定才使用)
```

### 在其他项目中使用 OpenCV 4.8.0 CUDA 版本

**方法一：在 CMakeLists.txt 中指定（推荐）**

```cmake
# 在 find_package 之前添加
set(OpenCV_DIR "/opt/opencv4-cuda/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
```

**方法二：编译时通过命令行指定**

```bash
cmake -DOpenCV_DIR=/opt/opencv4-cuda/lib/cmake/opencv4 ..
```

**方法三：设置环境变量（临时，仅当前终端有效）**

```bash
export OpenCV_DIR=/opt/opencv4-cuda/lib/cmake/opencv4
cmake ..
```

### 是否需要修改 .zshrc / .bashrc？

**不需要修改！** 这正是独立安装的好处：

| 场景 | 使用的 OpenCV |
|------|--------------|
| 不做任何设置 | 系统默认 3.4.3 |
| 指定 OpenCV_DIR | 4.8.0 CUDA 版 |

只有当你想让 4.8.0 成为**全局默认**版本时，才需要修改（**不推荐，会影响其他项目**）：

```bash
# 不推荐：让 4.8.0 成为默认
echo 'export OpenCV_DIR=/opt/opencv4-cuda/lib/cmake/opencv4' >> ~/.zshrc
```

## 卸载 OpenCV CUDA 版本

如果不再需要，可以完全卸载：

```bash
# 1. 删除安装目录
sudo rm -rf /opt/opencv4-cuda

# 2. 删除编译源码（可选，释放约 10GB 空间）
rm -rf ~/opencv_cuda_build

# 3. 删除本 demo 生成的配置文件（如果有）
rm -f /home/chris4/workspace/from_vm/YOLO/gpu_depth_demo/opencv_cuda_path.cmake
```

卸载后，系统恢复到只有 OpenCV 3.4.3 的状态，不影响任何其他项目。

## CUDA Toolkit 版本 vs Compute Capability

安装脚本中的 `CUDA_ARCH_BIN=8.9` 指的是 GPU 硬件架构，不是 CUDA Toolkit 版本：

| 名称 | 含义 | 示例 |
|------|------|------|
| **CUDA Toolkit** | NVIDIA 软件开发包版本 | 11.8 |
| **Compute Capability** | GPU 硬件架构代号 | 8.9 (RTX 4060) |

不同 GPU 的 Compute Capability：

| GPU | Compute Capability |
|-----|-------------------|
| RTX 4060/4070/4080/4090 | 8.9 |
| RTX 3060/3070/3080/3090 | 8.6 |
| RTX 2060/2070/2080 | 7.5 |
| GTX 1060/1070/1080 | 6.1 |

安装脚本会自动检测你的 GPU 并使用正确的架构。

## 集成到主项目

如需将 GPU 深度计算集成到 `get_pose_indemind_left.cpp`：

```cpp
// 1. 替换深度处理器
// 原来:
m_pSDK->EnableDepthProcessor();
m_pSDK->RegistDepthCallback(...);

// 改为:
m_pSDK->EnableRectifyProcessor();
m_pSDK->RegistRectifiedImgCallback([&](double time, cv::Mat left_rect, cv::Mat right_rect) {
    // GPU 计算深度
    cv::Mat disparity = ComputeDisparity(left_rect, right_rect, true, false);
    cv::Mat depth = DisparityToDepth(disparity, baseline, fx);
    // 使用 depth...
});
```

## 故障排除

### "No CUDA devices found"
```bash
# 检查 NVIDIA 驱动
nvidia-smi

# 检查 CUDA
nvcc --version
```

### 编译后仍是 CPU 模式
```bash
# 检查 OpenCV CUDA 是否正确安装
ls /opt/opencv4-cuda/lib/libopencv_cudastereo.so

# 清理并重新编译
rm -rf build
./build.sh
```

### 深度值不准确
- 检查相机标定参数
- 尝试使用 SGM 算法替代 BM
- 调整 `numDisparities` 和 `blockSize` 参数

## 依赖

- NVIDIA GPU (compute capability >= 6.0)
- CUDA Toolkit 11.x
- INDEMIND 双目相机
- OpenCV 4.x with CUDA (由 install_opencv_cuda.sh 安装)
