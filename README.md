# YOLO Pose Detection - Standalone Project

基于IMSEE双目相机的YOLOv8人体姿态检测系统 (独立版本)

## 依赖项

### 系统依赖

| 依赖 | 版本 | 说明 |
|------|------|------|
| Ubuntu | 18.04+ | 或其他 Linux x86-64 |
| GCC | 7+ | 需支持 C++14 |
| CMake | 3.10+ | 构建工具 |
| OpenCV | 3.0+ | 图像处理 |
| ONNX Runtime | 1.17.0+ | 模型推理 |
| Python | 3.6+ | 模型准备脚本 |

### 硬件依赖

- IMSEE 双目相机（INDEMIND）
- USB 3.0 接口

## 模型路径

```
models/
└── yolov8n-pose.onnx    # YOLOv8 姿态检测模型 (~6MB)
```

模型由 `prepare_yolo_model.py` 自动下载并转换。

## 快速开始

### Linux

```bash
# 1. 安装 ONNX Runtime
sudo ./install_onnxruntime.sh

# 2. 准备 YOLO 模型（需要 Python 环境）
pip install torch torchvision ultralytics
python3 prepare_yolo_model.py

# 3. 编译
mkdir -p build && cd build
cmake .. && make -j4

# 4. 连接 IMSEE 相机并运行
cd ..
sudo ./build/yolo_pose_detection
```

### 验证安装

```bash
# 检查 ONNX Runtime 是否安装
ldconfig -p | grep onnxruntime

# 检查模型文件
ls -lh models/yolov8n-pose.onnx

# 检查相机连接
lsusb | grep -i indemind
```

### Windows

查看 `README/DEPLOYMENT_GUIDE.md` 获取详细说明

## 项目特点

- ✅ 完全独立,可部署到任意目录
- ✅ 跨平台支持 (Linux/Windows)
- ✅ 17关键点人体姿态检测
- ✅ 3D深度融合
- ✅ 实时处理 (15-20 FPS)

## 文档

- **README/DEPLOYMENT_GUIDE.md** - 完整部署指南 (Linux/Windows)
- **README/README_YOLO_POSE.md** - 使用说明
- **README/PROBLEMS_AND_SOLUTIONS.md** - 问题解决
- **README/YOLO_IMPLEMENTATION_SUMMARY.md** - 实现细节

## 系统要求

- OpenCV 3.0+
- ONNX Runtime 1.17.0+
- IMSEE双目相机
- CMake 3.10+

## 目录结构

```
YOLO/
├── CMakeLists.txt          # 独立构建配置
├── *.cpp, *.h              # 源代码
├── include/                # SDK头文件
├── lib/                    # SDK库文件
├── models/                 # YOLO模型
│   └── yolov8n-pose.onnx
├── build/                  # 编译输出
└── README/                 # 文档
```

## 快捷键

- `q` / `ESC` : 退出
- `b` : 切换边界框
- `k` : 切换关键点
- `s` : 切换骨架
- `i` : 切换信息显示
- `SPACE` : 保存帧

## 技术栈

- **YOLO**: YOLOv8n-pose
- **推理**: ONNX Runtime
- **视觉**: OpenCV
- **相机**: IMSEE SDK

## 版本

- **版本**: 1.0.0
- **日期**: 2025-10-17
- **作者**: Chris

## 许可

基于 Apache License 2.0 (继承自IMSEE-SDK)
