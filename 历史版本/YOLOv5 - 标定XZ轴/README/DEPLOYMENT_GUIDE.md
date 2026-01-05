# YOLO 姿态检测项目 - 跨平台部署指南

## 项目概述

本项目是一个独立的YOLO姿态检测系统，已从IMSEE-SDK中分离出来，可以部署到任意目录或跨平台使用。

**版本**: 1.0.0
**日期**: 2025-10-17
**作者**: Chris

---

## 目录结构

```
YOLO/
├── CMakeLists.txt              # 独立的CMake配置文件 (支持Linux/Windows/macOS)
├── get_pose_with_depth.cpp     # 主程序
├── yolo_pose_detector.cpp      # YOLO推理引擎实现
├── yolo_pose_detector.h        # YOLO推理引擎头文件
├── pose_utils.cpp              # 工具函数实现
├── pose_utils.h                # 工具函数头文件
├── install_onnxruntime.sh      # ONNX Runtime安装脚本 (Linux)
├── prepare_yolo_model.py       # 模型准备脚本
├── quickstart_yolo_pose.sh     # 一键启动脚本 (Linux)
├── include/                    # IMSEE SDK头文件
│   ├── imrsdk.h
│   ├── imrdata.h
│   ├── types.h
│   └── ...
├── lib/                        # IMSEE SDK库文件
│   └── libindemind.so          # Linux共享库
│       或 indemind.dll         # Windows动态库
├── models/                     # YOLO模型文件
│   └── yolov8n-pose.onnx       # YOLOv8n-pose模型 (13MB)
├── build/                      # 编译输出目录 (自动生成)
│   └── yolo_pose_detection     # 可执行文件
└── README/                     # 文档目录
    ├── README_YOLO_POSE.md
    ├── PROBLEMS_AND_SOLUTIONS.md
    ├── DEPLOYMENT_GUIDE.md     # 本文件
    └── ...
```

---

## 一、Linux 系统部署

### 1.1 系统要求

- **操作系统**: Ubuntu 18.04+ / Debian 10+ / CentOS 8+
- **编译器**: GCC 7.0+ 或 Clang 6.0+
- **CMake**: 3.10+
- **依赖库**:
  - OpenCV 3.0+ (推荐3.4.3或4.x)
  - ONNX Runtime 1.17.0+
  - IMSEE SDK (已包含在lib目录)

### 1.2 快速部署 (推荐)

```bash
# 1. 将整个YOLO目录复制到任意位置
cp -r /source/YOLO /target/path/YOLO
cd /target/path/YOLO

# 2. 安装依赖 (如果系统上没有)
# OpenCV
sudo apt-get install libopencv-dev

# ONNX Runtime
./install_onnxruntime.sh

# 3. 准备YOLO模型 (如果models目录为空)
python3 prepare_yolo_model.py
# 选择: n (nano模型，最快)
# 输入大小: 640 (按Enter使用默认值)

# 4. 编译项目
mkdir -p build && cd build
cmake ..
make -j4

# 5. 运行
cd ..
sudo ./build/yolo_pose_detection
```

### 1.3 详细步骤

#### 步骤1: 检查依赖

```bash
# 检查OpenCV
pkg-config --modversion opencv

# 检查ONNX Runtime
ls /usr/local/lib/libonnxruntime.so

# 检查GCC版本
gcc --version  # 需要 >= 7.0

# 检查CMake版本
cmake --version  # 需要 >= 3.10
```

#### 步骤2: 安装缺失的依赖

**OpenCV安装**:

```bash
# 方法1: 使用包管理器 (简单但版本可能较旧)
sudo apt-get update
sudo apt-get install libopencv-dev

# 方法2: 从源码编译 (推荐，可获得最新版本)
# 参考: https://docs.opencv.org/master/d7/d9f/tutorial_linux_install.html
```

**ONNX Runtime安装**:

```bash
# 使用提供的脚本自动安装
./install_onnxruntime.sh

# 或手动安装
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-x64-1.17.0.tgz
tar -xzf onnxruntime-linux-x64-1.17.0.tgz
sudo cp onnxruntime-linux-x64-1.17.0/include/* /usr/local/include/onnxruntime/
sudo cp onnxruntime-linux-x64-1.17.0/lib/* /usr/local/lib/
sudo ldconfig
```

#### 步骤3: 配置和编译

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置项目
cmake ..

# 如果CMake找不到依赖，可以手动指定路径:
cmake .. \
  -DOpenCV_DIR=/path/to/opencv \
  -DONNXRUNTIME_INCLUDE_DIR=/path/to/onnxruntime/include \
  -DONNXRUNTIME_LIB=/path/to/onnxruntime/lib/libonnxruntime.so

# 编译 (使用多核加速)
make -j$(nproc)

# 检查编译结果
ls -lh yolo_pose_detection
```

#### 步骤4: 运行和测试

```bash
# 返回项目根目录
cd ..

# 检查动态库依赖
ldd build/yolo_pose_detection

# 确保相机已连接
lsusb | grep INDEMIND

# 设置USB权限 (如果需要)
sudo chmod 666 /dev/bus/usb/004/002  # 替换为实际设备路径

# 运行程序
sudo ./build/yolo_pose_detection

# 快捷键:
# q / ESC : 退出
# b       : 切换边界框显示
# k       : 切换关键点显示
# s       : 切换骨架显示
# i       : 切换信息叠加
# SPACE   : 保存当前帧
```

### 1.4 故障排除

**问题1: libindemind.so 找不到**

```bash
# 方法1: 设置LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/YOLO/lib:$LD_LIBRARY_PATH
./build/yolo_pose_detection

# 方法2: 使用ldconfig
sudo sh -c 'echo "/path/to/YOLO/lib" > /etc/ld.so.conf.d/indemind.conf'
sudo ldconfig
```

**问题2: 相机权限不足**

```bash
# 临时解决 (每次重启后需重新执行)
sudo chmod 666 /dev/bus/usb/BUS/DEVICE

# 永久解决: 创建udev规则
sudo nano /etc/udev/rules.d/99-indemind.rules
# 添加内容:
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", MODE="0666"

# 重新加载udev规则
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**问题3: 模型文件找不到**

```bash
# 检查模型文件是否存在
ls -lh models/yolov8n-pose.onnx

# 如果不存在，运行准备脚本
python3 prepare_yolo_model.py

# 或手动下载
wget https://github.com/ultralytics/assets/releases/download/v8.3.0/yolov8n-pose.pt
pip install ultralytics
python3 -c "from ultralytics import YOLO; YOLO('yolov8n-pose.pt').export(format='onnx')"
mv yolov8n-pose.onnx models/
```

---

## 二、Windows 系统部署

### 2.1 系统要求

- **操作系统**: Windows 10/11 (64-bit)
- **编译器**: Visual Studio 2019+ 或 MinGW-w64
- **CMake**: 3.10+
- **依赖库**:
  - OpenCV 3.0+ (推荐4.x)
  - ONNX Runtime 1.17.0+
  - IMSEE SDK (已包含)

### 2.2 准备工作

#### 步骤1: 安装开发工具

1. **安装Visual Studio 2019或更高版本**
   - 下载: https://visualstudio.microsoft.com/
   - 安装时选择"使用C++的桌面开发"工作负载

2. **安装CMake**
   - 下载: https://cmake.org/download/
   - 安装时选择"Add CMake to system PATH"

3. **安装Git** (可选，用于下载代码)
   - 下载: https://git-scm.com/download/win

#### 步骤2: 安装OpenCV

**方法1: 使用预编译包 (推荐)**

```powershell
# 1. 下载OpenCV
# 访问: https://opencv.org/releases/
# 下载: opencv-4.x.x-vc14_vc15.exe (例如 opencv-4.8.0-windows.exe)

# 2. 解压到C盘
# 例如: C:\opencv

# 3. 设置环境变量
# 系统变量 Path 添加:
C:\opencv\build\x64\vc15\bin

# 4. 创建 OpenCV_DIR 环境变量
# 变量名: OpenCV_DIR
# 变量值: C:\opencv\build
```

**方法2: 使用vcpkg**

```powershell
# 安装vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装OpenCV
.\vcpkg install opencv:x64-windows

# 设置环境变量
$env:OpenCV_DIR="C:\path\to\vcpkg\installed\x64-windows"
```

#### 步骤3: 安装ONNX Runtime

```powershell
# 1. 下载ONNX Runtime
# 访问: https://github.com/microsoft/onnxruntime/releases
# 下载: onnxruntime-win-x64-1.17.0.zip

# 2. 解压到合适位置
# 例如: C:\onnxruntime

# 3. 设置环境变量
# 系统变量 Path 添加:
C:\onnxruntime\lib

# 4. 创建环境变量
# ONNXRUNTIME_DIR = C:\onnxruntime
```

#### 步骤4: 准备IMSEE SDK

```powershell
# 1. 确保YOLO目录结构完整
# 检查 lib\ 目录下是否有:
# - indemind.dll
# - indemind.lib (如果使用MSVC)

# 2. 如果没有Windows版本的SDK文件,需要从原SDK中复制:
# 从 IMSEE-SDK\lib\win10-x64\ 复制:
# - indemind.dll
# - indemind.lib
# 到 YOLO\lib\
```

### 2.3 编译项目

#### 使用Visual Studio (推荐)

```powershell
# 1. 打开PowerShell或CMD，进入项目目录
cd C:\path\to\YOLO

# 2. 创建构建目录
mkdir build
cd build

# 3. 生成Visual Studio工程文件
cmake .. -G "Visual Studio 16 2019" -A x64

# 或者指定依赖路径:
cmake .. -G "Visual Studio 16 2019" -A x64 `
  -DOpenCV_DIR="C:\opencv\build" `
  -DONNXRUNTIME_INCLUDE_DIR="C:\onnxruntime\include" `
  -DONNXRUNTIME_LIB="C:\onnxruntime\lib\onnxruntime.lib"

# 4. 编译项目
cmake --build . --config Release

# 5. 可执行文件位置
# build\Release\yolo_pose_detection.exe
```

#### 使用MinGW

```powershell
# 1. 安装MinGW-w64
# 下载: https://www.mingw-w64.org/

# 2. 配置项目
cd C:\path\to\YOLO
mkdir build
cd build

cmake .. -G "MinGW Makefiles"

# 3. 编译
mingw32-make -j4

# 4. 可执行文件: build\yolo_pose_detection.exe
```

### 2.4 运行程序

```powershell
# 返回项目根目录
cd C:\path\to\YOLO

# 确保所有DLL在同一目录或PATH中
# 可能需要的DLL:
# - indemind.dll (lib目录)
# - onnxruntime.dll (C:\onnxruntime\lib)
# - OpenCV DLLs (C:\opencv\build\x64\vc15\bin)

# 方法1: 在build目录运行
cd build\Release
.\yolo_pose_detection.exe

# 方法2: 从项目根目录运行
.\build\Release\yolo_pose_detection.exe

# 如果提示缺少DLL,复制到可执行文件目录:
copy C:\onnxruntime\lib\*.dll build\Release\
copy C:\opencv\build\x64\vc15\bin\opencv_world*.dll build\Release\
copy lib\indemind.dll build\Release\
```

### 2.5 Windows故障排除

**问题1: 找不到DLL**

解决方案:
1. 将所有依赖DLL复制到可执行文件目录
2. 或将DLL路径添加到系统PATH

**问题2: CMake找不到OpenCV**

```powershell
# 明确指定OpenCV路径
cmake .. -DOpenCV_DIR="C:\opencv\build"
```

**问题3: 链接错误**

确保:
- 使用一致的编译器版本
- 使用匹配的库版本 (Debug/Release)
- OpenCV和ONNX Runtime的架构匹配 (x64)

---

## 三、将项目迁移到其他目录

### 3.1 完整迁移步骤

```bash
# Linux示例:
# 1. 复制整个项目目录
cp -r /current/path/YOLO /new/path/YOLO

# 2. 进入新目录
cd /new/path/YOLO

# 3. 清理旧的构建文件
rm -rf build

# 4. 重新编译
mkdir build && cd build
cmake ..
make -j4

# 5. 运行
cd ..
sudo ./build/yolo_pose_detection

# Windows示例:
# 1. 复制整个YOLO文件夹到新位置
# 2. 删除build目录
# 3. 重新执行编译步骤
```

### 3.2 关键点说明

**✅ 可以迁移的内容:**
- 所有源代码文件 (.cpp, .h)
- CMakeLists.txt
- include/ 和 lib/ 目录 (SDK文件)
- models/ 目录 (YOLO模型)
- 脚本文件 (.sh, .py)
- 文档文件

**⚠️ 需要重新生成的内容:**
- build/ 目录 (编译输出)
- CMake缓存文件

**📝 可能需要调整的内容:**
- 如果ONNX Runtime或OpenCV安装在非标准位置,需要重新指定路径
- USB设备权限 (Linux)

### 3.3 制作可分发包

如果要将项目分发给其他人:

```bash
# Linux
tar -czf yolo_pose_detection.tar.gz \
  --exclude='build' \
  --exclude='.git' \
  YOLO/

# Windows
# 使用7-Zip或WinRAR压缩YOLO目录
# 排除: build/, .git/
```

接收方只需:
1. 解压
2. 安装依赖 (OpenCV, ONNX Runtime)
3. 编译运行

---

## 四、项目配置说明

### 4.1 CMakeLists.txt 配置项

本项目的CMakeLists.txt已经配置为自动检测平台和依赖,但你也可以手动指定:

```cmake
# 手动指定OpenCV
cmake .. -DOpenCV_DIR=/path/to/opencv

# 手动指定ONNX Runtime
cmake .. \
  -DONNXRUNTIME_INCLUDE_DIR=/path/to/include \
  -DONNXRUNTIME_LIB=/path/to/lib/libonnxruntime.so

# 指定编译类型
cmake .. -DCMAKE_BUILD_TYPE=Release  # 或 Debug

# 指定安装前缀
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
```

### 4.2 项目依赖版本兼容性

| 依赖 | 最低版本 | 推荐版本 | 说明 |
|------|----------|----------|------|
| CMake | 3.10 | 3.22+ | 构建系统 |
| GCC/Clang | 7.0 | 11.0+ | C++14支持 |
| MSVC | 2017 | 2019+ | Windows编译 |
| OpenCV | 3.0 | 3.4.3 或 4.x | 图像处理 |
| ONNX Runtime | 1.17.0 | 1.17.0+ | 推理引擎 |
| IMSEE SDK | 2.0 | 2.0 | 相机驱动 |

### 4.3 编译选项

```bash
# 优化编译 (Release模式,最快运行速度)
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 调试编译 (Debug模式,包含调试符号)
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4

# 指定编译器
cmake .. -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11

# 查看详细编译信息
make VERBOSE=1
```

---

## 五、常见问题 (FAQ)

### Q1: 项目能否在没有IMSEE相机的情况下编译?

**A**: 可以编译,但无法运行。如果需要模拟测试,可以:
1. 修改代码读取视频文件而不是相机
2. 使用录制的相机数据

### Q2: 如何更换YOLO模型?

**A**:
```bash
# 1. 使用prepare_yolo_model.py准备其他大小的模型
python3 prepare_yolo_model.py
# 选择 s/m/l/x 而不是 n

# 2. 或手动导出自定义模型
from ultralytics import YOLO
model = YOLO('custom_model.pt')
model.export(format='onnx', imgsz=640)
mv custom_model.onnx models/yolov8n-pose.onnx

# 3. 无需重新编译,直接运行即可
```

### Q3: 性能优化建议?

**A**:
- 使用Release模式编译 (`-DCMAKE_BUILD_TYPE=Release`)
- 启用CPU优化 (已在CMake中配置 `-march=native`)
- 使用较小的模型 (yolov8n-pose vs yolov8m-pose)
- 降低输入分辨率 (640 -> 480)
- 如有GPU,使用CUDA版本的ONNX Runtime

### Q4: 如何在多台机器上部署?

**A**:
1. 在一台机器上完整配置并测试
2. 打包整个YOLO目录
3. 在目标机器上:
   - 安装依赖 (OpenCV, ONNX Runtime)
   - 解压项目
   - 重新编译 (因为二进制文件可能不兼容)
   - 配置USB权限 (Linux)

### Q5: 支持哪些操作系统?

**A**:
- ✅ Linux (Ubuntu, Debian, CentOS, Fedora, etc.)
- ✅ Windows 10/11 (x64)
- ✅ macOS (理论支持,未测试)
- ❌ ARM Linux (需要ARM版本的IMSEE SDK和ONNX Runtime)

### Q6: 可以在Docker容器中运行吗?

**A**: 可以,但需要处理USB设备访问:

```dockerfile
# Dockerfile示例
FROM ubuntu:20.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential cmake \
    libopencv-dev \
    wget

# 复制项目
COPY YOLO /app/YOLO

# 编译
WORKDIR /app/YOLO
RUN mkdir build && cd build && cmake .. && make -j4

# 运行时需要 --device 参数挂载USB设备
# docker run --device=/dev/bus/usb/004/002 -it yolo_pose
```

---

## 六、项目维护

### 6.1 更新依赖

```bash
# 更新ONNX Runtime
./install_onnxruntime.sh  # 脚本会下载最新版本

# 更新OpenCV (Ubuntu)
sudo apt-get update
sudo apt-get upgrade libopencv-dev

# 更新YOLO模型
python3 prepare_yolo_model.py
```

### 6.2 清理构建文件

```bash
# Linux
rm -rf build
rm -f CMakeCache.txt

# Windows (PowerShell)
Remove-Item -Recurse -Force build
```

### 6.3 查看版本信息

```bash
# 查看已编译的程序信息
./build/yolo_pose_detection --version  # (如果实现了--version参数)

# 查看依赖库版本
ldd build/yolo_pose_detection | grep -E "(opencv|onnx|indemind)"

# 查看CMake配置
cd build
cmake -L ..
```

---

## 七、技术支持

### 7.1 获取帮助

1. **查看文档**:
   - README/README_YOLO_POSE.md - 使用说明
   - README/PROBLEMS_AND_SOLUTIONS.md - 问题解决
   - README/YOLO_IMPLEMENTATION_SUMMARY.md - 技术细节

2. **调试技巧**:
   ```bash
   # 查看详细输出
   ./build/yolo_pose_detection 2>&1 | tee output.log

   # 使用gdb调试
   gdb ./build/yolo_pose_detection
   (gdb) run
   (gdb) backtrace  # 崩溃时查看调用栈

   # 检查依赖
   ldd ./build/yolo_pose_detection
   ```

### 7.2 报告问题

如果遇到问题,请提供:
1. 操作系统版本 (`uname -a` 或 `ver`)
2. 依赖库版本 (OpenCV, ONNX Runtime)
3. 完整的错误信息
4. CMake配置输出
5. 编译日志 (如果编译失败)

---

## 八、总结

### 8.1 快速参考

**Linux一键部署**:
```bash
cd /path/to/YOLO
./install_onnxruntime.sh
python3 prepare_yolo_model.py
mkdir build && cd build && cmake .. && make -j4 && cd ..
sudo ./build/yolo_pose_detection
```

**Windows一键部署** (PowerShell):
```powershell
cd C:\path\to\YOLO
# 手动安装OpenCV和ONNX Runtime (见2.2节)
mkdir build; cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
cd ..
.\build\Release\yolo_pose_detection.exe
```

### 8.2 项目特点

✅ **完全独立**: 不依赖原IMSEE-SDK目录结构
✅ **跨平台**: 支持Linux/Windows
✅ **易于部署**: 可复制到任意目录
✅ **自动检测**: CMake自动查找依赖
✅ **文档完善**: 包含完整的问题解决指南

### 8.3 文件清单

确保以下文件完整:

**必需文件** (21个):
- [x] CMakeLists.txt
- [x] *.cpp (3个源文件)
- [x] *.h (2个头文件)
- [x] include/*.h (6个SDK头文件)
- [x] lib/libindemind.so (或 .dll)
- [x] models/yolov8n-pose.onnx
- [x] install_onnxruntime.sh
- [x] prepare_yolo_model.py
- [x] quickstart_yolo_pose.sh

**可选文件**:
- [ ] README/*.md (文档文件)
- [ ] build/ (编译后自动生成)

---

**版本历史**:
- v1.0.0 (2025-10-17): 初始版本,支持Linux/Windows独立部署

**更新日期**: 2025-10-17
**维护者**: Chris
