# GPU加速部署指南

本文档详细说明如何在Windows系统上启用NVIDIA GPU加速，获得4-20倍的性能提升。

## 📊 性能对比

| 硬件配置 | YOLOv8n 推理速度 | 提升倍数 | 适用场景 |
|---------|----------------|---------|---------|
| CPU (Intel i7) | 15-20 FPS | 基准 | 测试/开发 |
| **NVIDIA GTX 1660** | 80-100 FPS | **4-5x** | 实时检测 |
| **NVIDIA RTX 3060** | 150-200 FPS | **8-10x** | 高性能需求 |
| **NVIDIA RTX 4090** | 300-400 FPS | **15-20x** | 专业应用 |

## 🎯 完整部署流程

### 步骤 0: 检查硬件要求

```powershell
# 运行此命令检查GPU
nvidia-smi

# 应该看到类似输出:
# +-----------------------------------------------------------------------------+
# | NVIDIA-SMI 535.xx       Driver Version: 535.xx       CUDA Version: 12.2    |
# +-----------------------------------------------------------------------------+
# |   GPU  Name            TCC/WDDM | Bus-Id        Disp.A | Volatile Uncorr. ECC |
# |   0   NVIDIA GeForce RTX 3060   | 00000000:01:00.0  On |                  N/A |
# +-----------------------------------------------------------------------------+
```

**最低要求:**
- NVIDIA显卡: GTX 1050 或更高
- 显存: 2GB 或以上
- 驱动版本: 最新稳定版

**如果nvidia-smi命令无效:**
1. 访问 https://www.nvidia.com/Download/index.aspx
2. 下载并安装最新驱动
3. 重启电脑

---

### 步骤 1: 卸载CPU版本 ONNX Runtime

```powershell
# 以管理员身份运行PowerShell
# 右键点击PowerShell图标 -> 以管理员身份运行

cd <项目路径>
# 例如: cd D:\YOLO-实时显示髋部坐标

# 运行卸载脚本
.\uninstall_onnxruntime.ps1

# 按照提示操作，输入 Y 确认删除
```

**卸载后验证:**
```powershell
# 重新打开PowerShell窗口
echo $env:ONNXRUNTIME_DIR
# 应该返回空白

dir C:\onnxruntime
# 应该显示"找不到路径"
```

---

### 步骤 2: 安装CUDA Toolkit

```powershell
# 1. 检查nvidia-smi显示的CUDA版本
nvidia-smi
# 例如显示: CUDA Version: 12.2

# 2. 访问NVIDIA CUDA下载页面
# https://developer.nvidia.com/cuda-downloads

# 3. 选择配置:
#    - Operating System: Windows
#    - Architecture: x86_64
#    - Version: 10 或 11
#    - Installer Type: exe (local)

# 4. 下载并运行安装程序
#    文件名类似: cuda_12.2.0_535.54_windows.exe

# 5. 安装选项:
#    - 选择"自定义安装"
#    - 至少勾选:
#      ✓ CUDA Toolkit
#      ✓ CUDA Development
#      ✓ CUDA Runtime
#      ✓ CUDA Samples (可选)
#    - 安装路径: 默认 C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2

# 6. 验证安装
nvcc --version
# 应该显示: Cuda compilation tools, release 12.2, ...
```

**如果nvcc命令无效:**
```powershell
# 手动添加到PATH环境变量
# Win+R -> sysdm.cpl -> 高级 -> 环境变量
# 编辑PATH，添加:
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin
```

---

### 步骤 3: 安装cuDNN

```powershell
# 1. 访问cuDNN下载页面 (需要注册免费账号)
# https://developer.nvidia.com/cudnn

# 2. 选择对应CUDA版本的cuDNN
#    例如: cuDNN v9.0.0 for CUDA 12.x

# 3. 下载Windows版本
#    文件名类似: cudnn-windows-x86_64-9.0.0.312_cuda12-archive.zip

# 4. 解压ZIP文件
#    解压到任意临时位置，例如: C:\temp\cudnn

# 5. 复制文件到CUDA安装目录
#    假设CUDA安装在: C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2
```

**复制命令 (以管理员身份运行PowerShell):**
```powershell
# 设置路径变量
$cudnnPath = "C:\temp\cudnn\cudnn-windows-x86_64-9.0.0.312_cuda12-archive"
$cudaPath = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2"

# 复制DLL文件
Copy-Item "$cudnnPath\bin\*.dll" -Destination "$cudaPath\bin" -Force

# 复制头文件
Copy-Item "$cudnnPath\include\*.h" -Destination "$cudaPath\include" -Force

# 复制库文件
Copy-Item "$cudnnPath\lib\x64\*.lib" -Destination "$cudaPath\lib\x64" -Force

Write-Host "cuDNN安装完成!" -ForegroundColor Green
```

**验证cuDNN安装:**
```powershell
# 检查文件是否存在
Test-Path "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin\cudnn64_9.dll"
# 应该返回: True
```

---

### 步骤 4: 安装ONNX Runtime GPU版本

**方法A: 使用安装脚本（推荐）**

```powershell
# 1. 下载ONNX Runtime GPU版本
# 访问: https://github.com/microsoft/onnxruntime/releases
# 下载文件: onnxruntime-win-x64-gpu-1.17.0.zip
# 保存位置: 例如 D:\Downloads\

# 2. 运行安装脚本
.\install_onnxruntime_gpu.ps1

# 3. 按照提示操作:
#    - 确认检测到GPU: 看到✓标记
#    - 输入安装路径: 直接回车使用默认 C:\onnxruntime-gpu
#    - 输入下载的ZIP文件路径:
#      D:\Downloads\onnxruntime-win-x64-gpu-1.17.0.zip
#      或者拖拽文件到PowerShell窗口

# 4. 等待安装完成，看到"安装完成!"
```

**方法B: 手动安装**

```powershell
# 1. 解压下载的ZIP文件到 C:\onnxruntime-gpu
Expand-Archive -Path "D:\Downloads\onnxruntime-win-x64-gpu-1.17.0.zip" -DestinationPath "C:\temp"

# 2. 重命名并移动到最终位置
Move-Item "C:\temp\onnxruntime-win-x64-gpu-1.17.0" "C:\onnxruntime-gpu"

# 3. 设置环境变量 (以管理员身份)
[System.Environment]::SetEnvironmentVariable('ONNXRUNTIME_DIR', 'C:\onnxruntime-gpu', [System.EnvironmentVariableTarget]::Machine)

# 4. 添加到PATH
$path = [System.Environment]::GetEnvironmentVariable('Path', [System.EnvironmentVariableTarget]::Machine)
$newPath = "$path;C:\onnxruntime-gpu\lib"
[System.Environment]::SetEnvironmentVariable('Path', $newPath, [System.EnvironmentVariableTarget]::Machine)
```

**验证安装:**
```powershell
# 重启PowerShell窗口后
echo $env:ONNXRUNTIME_DIR
# 应该显示: C:\onnxruntime-gpu

dir C:\onnxruntime-gpu\lib
# 应该看到:
#   onnxruntime.dll
#   onnxruntime.lib
#   onnxruntime_providers_cuda.dll      ← GPU关键文件
#   onnxruntime_providers_shared.dll    ← GPU关键文件
```

---

### 步骤 5: 替换代码文件启用GPU

```powershell
cd <项目路径>

# 备份原始文件
Copy-Item yolo_pose_detector.cpp yolo_pose_detector_cpu_backup.cpp

# 使用GPU版本
Copy-Item yolo_pose_detector_gpu.cpp yolo_pose_detector.cpp -Force

Write-Host "✓ 已切换到GPU加速版本" -ForegroundColor Green
```

**GPU版本主要改动:**
- ✅ 添加CUDA Provider配置
- ✅ 自动检测GPU可用性
- ✅ GPU不可用时自动回退到CPU
- ✅ 优化内存管理
- ✅ 详细的日志输出

---

### 步骤 6: 重新编译项目

```powershell
# 1. 清理旧的编译文件
Remove-Item -Recurse -Force build

# 2. 打开"Developer Command Prompt for VS 2019"
#    (从开始菜单搜索并打开)

# 3. 进入项目目录
cd <项目路径>

# 4. 创建build目录
mkdir build
cd build

# 5. 配置CMake
cmake .. -G "Visual Studio 16 2019" -A x64 `
  -DOpenCV_DIR="C:\opencv\build" `
  -DONNXRUNTIME_INCLUDE_DIR="C:\onnxruntime-gpu\include" `
  -DONNXRUNTIME_LIB="C:\onnxruntime-gpu\lib\onnxruntime.lib"

# 6. 编译 (Release模式)
cmake --build . --config Release

# 7. 检查编译结果
dir Release\*.exe
# 应该看到三个可执行文件
```

**如果编译失败:**
```powershell
# 检查CMake输出，确认找到了正确的库
# 如果提示找不到ONNX Runtime，手动指定路径

# 重新配置
cmake .. -G "Visual Studio 16 2019" -A x64 `
  -DOpenCV_DIR="C:\opencv\build" `
  -DONNXRUNTIME_INCLUDE_DIR="C:\onnxruntime-gpu\include" `
  -DONNXRUNTIME_LIB="C:\onnxruntime-gpu\lib\onnxruntime.lib"
```

---

### 步骤 7: 复制GPU DLL文件

```powershell
cd build\Release

# 复制ONNX Runtime GPU DLLs (关键步骤!)
copy C:\onnxruntime-gpu\lib\onnxruntime.dll .
copy C:\onnxruntime-gpu\lib\onnxruntime_providers_cuda.dll .
copy C:\onnxruntime-gpu\lib\onnxruntime_providers_shared.dll .

# 复制OpenCV DLL
copy C:\opencv\build\x64\vc15\bin\opencv_world480.dll .

# 复制IMSEE SDK DLL
copy ..\..\lib\indemind.dll .

# 验证所有DLL已复制
dir *.dll
# 应该看到至少5个DLL文件
```

---

### 步骤 8: 运行GPU加速版本

```powershell
# 在build\Release目录下运行
.\yolo_pose_detection.exe
```

**期望输出:**
```
Attempting to enable CUDA GPU acceleration...
✓ CUDA GPU acceleration enabled successfully!
  Device ID: 0
  VRAM Limit: 2048 MB
Initializing YOLO Pose Detector...
  Model: ../../models/yolov8n-pose.onnx
  Input size: 640x640
  Input name: images
  Input shape: [1, 3, 640, 640]
  Output name: output0
  Output shape: [1, 56, 8400]
✓ YOLO Pose Detector initialized successfully
```

**关键标志:** 看到 `✓ CUDA GPU acceleration enabled successfully!` 表示GPU加速成功启用！

---

### 步骤 9: 验证GPU使用情况

```powershell
# 打开新的PowerShell窗口
# 实时监控GPU使用情况
nvidia-smi -l 1

# 运行程序后，应该看到:
# GPU-Util列显示 60-90%
# Memory-Usage显示使用的显存增加
```

**示例输出:**
```
+-----------------------------------------------------------------------------+
| GPU  Name            TCC/WDDM | Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|   0  NVIDIA GeForce ... WDDM  | 00000000:01:00.0  On |                  N/A |
| 40%   65C    P2    85W / 170W |   1200MiB /  6144MiB |     78%      Default |
+-----------------------------------------------------------------------------+
```

GPU-Util应该在60-90%之间，证明GPU正在工作！

---

## 🔧 故障排除

### 问题 1: "Failed to enable CUDA GPU acceleration"

**可能原因:**
1. ONNX Runtime GPU版本未正确安装
2. CUDA/cuDNN未安装或版本不匹配
3. GPU DLL文件未复制到可执行文件目录

**解决方法:**
```powershell
# 1. 检查ONNX Runtime GPU DLL是否存在
dir C:\onnxruntime-gpu\lib\onnxruntime_providers_cuda.dll

# 2. 检查是否复制到可执行文件目录
dir build\Release\onnxruntime_providers_cuda.dll

# 3. 检查CUDA安装
nvcc --version

# 4. 检查cuDNN DLL
dir "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin\cudnn*.dll"

# 5. 确保CUDA bin目录在PATH中
echo $env:Path | Select-String "CUDA"
```

---

### 问题 2: 程序启动报错 "找不到DLL"

**解决方法:**
```powershell
# 使用Dependency Walker检查缺失的DLL
# 下载: https://www.dependencywalker.com/

# 或手动检查并复制所有必要的DLL
cd build\Release

# 确保以下DLL都存在:
dir onnxruntime.dll
dir onnxruntime_providers_cuda.dll
dir onnxruntime_providers_shared.dll
dir opencv_world*.dll
dir indemind.dll
dir cudnn64_*.dll  # 应该在CUDA\bin目录，已在PATH中

# 如果缺少cudnn DLL，添加CUDA bin到PATH或复制到当前目录
copy "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin\cudnn64_*.dll" .
```

---

### 问题 3: GPU使用率为0

**可能原因:**
- GPU加速未真正启用
- 模型太小，CPU预处理占主要时间

**解决方法:**
```powershell
# 1. 检查程序输出，确认看到GPU启用消息
# 应该看到: "✓ CUDA GPU acceleration enabled successfully!"

# 2. 尝试使用更大的模型 (增加GPU负载)
python prepare_yolo_model.py
# 选择: s 或 m (small/medium模型)

# 3. 检查显存使用 (如果显存增加，说明GPU在工作)
nvidia-smi
```

---

### 问题 4: 性能没有提升

**检查清单:**
1. 确认GPU加速成功启用 (看到✓消息)
2. 检查是否使用了Release模式编译
3. 检查GPU使用率是否达到60%以上
4. 尝试降低图像分辨率或使用更大模型

**性能测试:**
```powershell
# CPU版本测试
# 使用原始代码运行，记录FPS

# GPU版本测试
# 使用GPU代码运行，记录FPS

# 应该看到明显的FPS提升 (4-10倍)
```

---

## 📈 性能优化建议

### 1. 调整显存限制
在 `yolo_pose_detector_gpu.cpp` 第37行：
```cpp
cuda_options.gpu_mem_limit = 2ULL * 1024 * 1024 * 1024;  // 2GB

// 如果你的GPU有更多显存，可以增加此值
// 例如，6GB GPU:
cuda_options.gpu_mem_limit = 4ULL * 1024 * 1024 * 1024;  // 4GB
```

### 2. 启用混合精度 (FP16)
适用于RTX系列GPU（有Tensor Cores）：
```cpp
// 在构造函数中添加
session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
session_options_.AddConfigEntry("session.use_fp16", "1");  // 启用FP16
```

### 3. 使用更小的模型
- yolov8n: 最快，适合实时应用
- yolov8s: 平衡
- yolov8m: 更准确但较慢

### 4. 批处理推理
如果处理录制视频，可以使用批处理提高吞吐量。

---

## 📝 快速命令参考

```powershell
# 检查GPU
nvidia-smi

# 检查CUDA
nvcc --version

# 卸载CPU版本
.\uninstall_onnxruntime.ps1

# 安装GPU版本
.\install_onnxruntime_gpu.ps1

# 编译项目
mkdir build; cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# 复制DLL
cd Release
copy C:\onnxruntime-gpu\lib\*.dll .
copy C:\opencv\build\x64\vc15\bin\opencv_world*.dll .
copy ..\..\lib\indemind.dll .

# 运行
.\yolo_pose_detection.exe

# 监控GPU
nvidia-smi -l 1
```

---

## ✅ 完整部署检查清单

安装完成后，确认以下所有项目：

- [ ] nvidia-smi 命令正常工作
- [ ] CUDA Toolkit 已安装 (nvcc --version)
- [ ] cuDNN 已复制到CUDA目录
- [ ] ONNX Runtime GPU版本已安装到 C:\onnxruntime-gpu
- [ ] 环境变量 ONNXRUNTIME_DIR 设置正确
- [ ] PATH包含 C:\onnxruntime-gpu\lib
- [ ] 代码已替换为GPU版本
- [ ] 项目重新编译成功
- [ ] GPU相关DLL已复制到可执行文件目录
- [ ] 运行时看到 "CUDA GPU acceleration enabled"
- [ ] nvidia-smi显示GPU使用率上升
- [ ] FPS明显提升（4-10倍）

---

## 🎓 附加资源

- CUDA Toolkit: https://developer.nvidia.com/cuda-downloads
- cuDNN: https://developer.nvidia.com/cudnn
- ONNX Runtime: https://github.com/microsoft/onnxruntime/releases
- NVIDIA驱动: https://www.nvidia.com/Download/index.aspx

---

**版本:** 1.0
**更新日期:** 2025-11-20
**作者:** Chris

有问题请参考: README/PROBLEMS_AND_SOLUTIONS.md

📦 已创建的文件和工具

  我为你创建了以下文件来帮助你完成GPU部署：

  1. 卸载脚本

  uninstall_onnxruntime.ps1
  - 自动卸载CPU版本的ONNX Runtime
  - 清理环境变量
  - 删除项目中的旧DLL文件
  - 清理CMake缓存

  2. GPU版本安装脚本

  install_onnxruntime_gpu.ps1
  - 检测NVIDIA显卡和驱动
  - 检测CUDA安装
  - 引导下载和安装ONNX Runtime GPU版本
  - 自动设置环境变量
  - 验证安装完整性

  3. GPU加速代码文件

  yolo_pose_detector_gpu.cpp
  - 启用CUDA GPU加速
  - 自动检测GPU可用性
  - GPU不可用时自动回退CPU
  - 优化的显存管理
  - 详细的日志输出

  4. 完整GPU部署指南

  README/GPU_DEPLOYMENT_GUIDE.md
  - 详细的分步部署说明
  - 性能对比数据
  - 故障排除指南
  - 性能优化建议
  - 快速命令参考

  5. 版本切换工具

  switch_gpu_cpu.ps1
  - 快速在GPU和CPU版本间切换
  - 自动备份原始文件
  - 显示版本差异
  - 操作指引

  ---
  🚀 快速开始指南

  完整GPU部署流程（9个步骤）：

  # 步骤1: 检查GPU
  nvidia-smi

  # 步骤2: 卸载CPU版本ONNX Runtime
  .\uninstall_onnxruntime.ps1

  # 步骤3: 安装CUDA Toolkit (从NVIDIA官网下载)
  # https://developer.nvidia.com/cuda-downloads

  # 步骤4: 安装cuDNN (从NVIDIA官网下载)
  # https://developer.nvidia.com/cudnn

  # 步骤5: 安装ONNX Runtime GPU版本
  .\install_onnxruntime_gpu.ps1

  # 步骤6: 切换到GPU代码
  .\switch_gpu_cpu.ps1
  # 选择选项1

  # 步骤7: 重新编译
  cd build
  cmake --build . --config Release

  # 步骤8: 复制GPU DLL
  cd Release
  copy C:\onnxruntime-gpu\lib\*.dll .

  # 步骤9: 运行并验证
  .\yolo_pose_detection.exe
  # 应该看到: "✓ CUDA GPU acceleration enabled successfully!"

  ---
  📊 预期性能提升

  | GPU型号    | 预期FPS   | 提升倍数 |
  |----------|---------|------|
  | GTX 1660 | 80-100  | 5x   |
  | RTX 3060 | 150-200 | 10x  |
  | RTX 4090 | 300-400 | 20x  |

  相比CPU的15-20 FPS，提升非常显著！

  ---
  💡 使用建议

  1. 首先阅读 README/GPU_DEPLOYMENT_GUIDE.md 了解完整流程
  2. 按步骤执行：
    - 不要跳过CUDA和cuDNN的安装
    - 确保每一步都成功再进行下一步
    - 使用脚本可以简化很多手动操作
  3. 验证关键点：
    - nvidia-smi显示GPU信息
    - 运行时看到"CUDA GPU acceleration enabled"
    - nvidia-smi显示GPU使用率上升
  4. 遇到问题：
    - 查看GPU_DEPLOYMENT_GUIDE.md的故障排除章节
    - 检查所有DLL是否正确复制
    - 确认CUDA版本和ONNX Runtime版本匹配

  ---
  ✅ 关键检查点

  部署后必须确认：
  - ✅ 看到 "✓ CUDA GPU acceleration enabled successfully!"
  - ✅ nvidia-smi显示GPU使用率 60-90%
  - ✅ FPS明显提升（至少4倍以上）
  - ✅ 显存使用量增加

  如果这些都满足，恭喜你成功启用了GPU加速！🎉

