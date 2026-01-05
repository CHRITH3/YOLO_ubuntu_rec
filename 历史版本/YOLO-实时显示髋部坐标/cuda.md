🚀 性能提升分析

  预期性能对比

  | 硬件配置            | YOLOv8n 推理速度 | 提升倍数   |
  |-----------------|--------------|--------|
  | CPU (Intel i7)  | 15-20 FPS    | 基准     |
  | NVIDIA GTX 1660 | 80-100 FPS   | 4-5x   |
  | NVIDIA RTX 3060 | 150-200 FPS  | 8-10x  |
  | NVIDIA RTX 4090 | 300-400 FPS  | 15-20x |

  结论：使用GPU后性能提升至少4-20倍！ 特别适合实时姿态检测场景。

  ---
  🎯 GPU部署完整方案

  方案一：使用ONNX Runtime CUDA版本（推荐）

  第一步：检查硬件和驱动

  # 1. 检查NVIDIA显卡
  nvidia-smi

  # 应该显示类似:
  # +-----------------------------------------------------------------------
  ------+
  # | NVIDIA-SMI 535.xx       Driver Version: 535.xx       CUDA Version: 
  12.2     |
  # +-----------------------------------------------------------------------
  ------+
  # |   0  NVIDIA GeForce RTX 3060    On   | 00000000:01:00.0  Off |        
    N/A |
  # +-----------------------------------------------------------------------
  ------+

  # 2. 记录CUDA版本（例如：12.2）

  第二步：安装CUDA Toolkit

  # 访问NVIDIA官网下载CUDA Toolkit
  # https://developer.nvidia.com/cuda-downloads

  下载对应版本:
  - Windows 10/11
  - x86_64架构
  - CUDA Toolkit 12.x (根据nvidia-smi显示的版本)

  # 安装步骤:
  1. 运行安装程序 (cuda_12.x.x_windows.exe)
  2. 选择"自定义安装"
  3. 至少勾选:
     ✓ CUDA Toolkit
     ✓ CUDA Development
     ✓ CUDA Runtime
  4. 安装完成后，验证:

  nvcc --version
  # 应该显示: Cuda compilation tools, release 12.x

  第三步：安装cuDNN

  # 1. 访问NVIDIA官网下载cuDNN
  # https://developer.nvidia.com/cudnn
  # (需要注册NVIDIA开发者账号，免费)

  下载对应版本:
  - cuDNN for CUDA 12.x
  - Windows版本

  # 2. 解压下载的压缩包 (例如: 
  cudnn-windows-x86_64-9.x.x.x_cuda12-archive.zip)

  # 3. 复制文件到CUDA安装目录
  # 假设CUDA安装在: C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.x

  复制文件:
  cudnn-xxx\bin\*.dll      -> C:\Program Files\NVIDIA GPU Computing
  Toolkit\CUDA\v12.x\bin
  cudnn-xxx\include\*.h    -> C:\Program Files\NVIDIA GPU Computing
  Toolkit\CUDA\v12.x\include
  cudnn-xxx\lib\*.lib      -> C:\Program Files\NVIDIA GPU Computing
  Toolkit\CUDA\v12.x\lib\x64

  # 4. 将CUDA bin目录添加到系统PATH (如果还没有)
  C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.x\bin

  第四步：安装ONNX Runtime GPU版本

  # ⚠️ 重要：卸载之前安装的CPU版本

  # 1. 下载ONNX Runtime GPU版本
  # 访问: https://github.com/microsoft/onnxruntime/releases

  下载文件 (根据CUDA版本选择):
  - CUDA 12.x: onnxruntime-win-x64-gpu-1.17.0.zip
  - CUDA 11.x: onnxruntime-win-x64-gpu-1.17.0-cu11.zip

  # 2. 解压到目录 (例如: C:\onnxruntime-gpu)
  目录结构:
  C:\onnxruntime-gpu\
    ├── include\
    │   └── onnxruntime_cxx_api.h
    └── lib\
        ├── onnxruntime.lib
        ├── onnxruntime.dll
        ├── onnxruntime_providers_cuda.dll      # GPU关键DLL
        └── onnxruntime_providers_shared.dll    # GPU关键DLL

  # 3. 更新环境变量
  # 修改之前的ONNXRUNTIME_DIR:
  变量名: ONNXRUNTIME_DIR
  变量值: C:\onnxruntime-gpu

  # 修改PATH，替换为:
  C:\onnxruntime-gpu\lib

  第五步：修改代码以启用GPU

  创建修改后的文件：

● Read(yolo_pose_detector.cpp)
  ⎿  Read 50 lines

● 需要修改的代码位置：

  在 yolo_pose_detector.cpp 文件中，修改构造函数：

  // 原代码 (第9-27行)
  YOLOPoseDetector::YOLOPoseDetector(
      const std::string& model_path,
      int input_size,
      float conf_threshold,
      float iou_threshold)
      : model_path_(model_path),
        input_size_(input_size),
        conf_threshold_(conf_threshold),
        iou_threshold_(iou_threshold),
        env_(ORT_LOGGING_LEVEL_WARNING, "YOLOPose"),
        scale_factor_(1.0f),
        pad_w_(0),
        pad_h_(0),
        initialized_(false) {

      session_options_.SetIntraOpNumThreads(4);
      session_options_.SetGraphOptimizationLevel(
          GraphOptimizationLevel::ORT_ENABLE_ALL);
  }

  修改为（启用GPU）：

  YOLOPoseDetector::YOLOPoseDetector(
      const std::string& model_path,
      int input_size,
      float conf_threshold,
      float iou_threshold)
      : model_path_(model_path),
        input_size_(input_size),
        conf_threshold_(conf_threshold),
        iou_threshold_(iou_threshold),
        env_(ORT_LOGGING_LEVEL_WARNING, "YOLOPose"),
        scale_factor_(1.0f),
        pad_w_(0),
        pad_h_(0),
        initialized_(false) {

      // 🚀 启用CUDA GPU加速
      OrtCUDAProviderOptions cuda_options;
      cuda_options.device_id = 0;  // 使用第一块GPU
      cuda_options.arena_extend_strategy = 0;
      cuda_options.gpu_mem_limit = 2 * 1024 * 1024 * 1024ULL;  // 
  2GB显存限制
      cuda_options.cudnn_conv_algo_search =
  OrtCudnnConvAlgoSearchExhaustive;
      cuda_options.do_copy_in_default_stream = 1;

      try {
          session_options_.AppendExecutionProvider_CUDA(cuda_options);
          std::cout << "✓ CUDA GPU acceleration enabled" << std::endl;
      } catch (const Ort::Exception& e) {
          std::cerr << "⚠ Failed to enable CUDA, falling back to CPU: "
                    << e.what() << std::endl;
      }

      session_options_.SetIntraOpNumThreads(4);
      session_options_.SetGraphOptimizationLevel(
          GraphOptimizationLevel::ORT_ENABLE_ALL);
  }

  第六步：重新编译项目

  # 1. 清理旧的编译文件
  cd D:\YOLO-实时显示髋部坐标
  Remove-Item -Recurse -Force build

  # 2. 打开"Developer Command Prompt for VS 2019"

  # 3. 重新编译
  mkdir build
  cd build

  cmake .. -G "Visual Studio 16 2019" -A x64 `
    -DOpenCV_DIR="C:\opencv\build" `
    -DONNXRUNTIME_INCLUDE_DIR="C:\onnxruntime-gpu\include" `
    -DONNXRUNTIME_LIB="C:\onnxruntime-gpu\lib\onnxruntime.lib"

  cmake --build . --config Release

  # 4. 复制GPU相关DLL到可执行文件目录
  cd Release

  # 复制ONNX Runtime GPU DLLs
  copy C:\onnxruntime-gpu\lib\onnxruntime.dll .
  copy C:\onnxruntime-gpu\lib\onnxruntime_providers_cuda.dll .
  copy C:\onnxruntime-gpu\lib\onnxruntime_providers_shared.dll .

  # 复制其他必要的DLL (如之前)
  copy C:\opencv\build\x64\vc15\bin\opencv_world480.dll .
  copy ..\..\lib\indemind.dll .

  第七步：验证GPU加速

  # 运行程序
  .\yolo_pose_detection.exe

  # 检查输出，应该看到:
  # Initializing YOLO Pose Detector...
  # ✓ CUDA GPU acceleration enabled      <-- 关键信息
  #   Model: ../../models/yolov8n-pose.onnx
  #   Input size: 640x640
  # ✓ YOLO Pose Detector initialized successfully

  # 在另一个CMD窗口监控GPU使用情况:
  nvidia-smi -l 1

  # 应该看到GPU使用率上升到70-90%

  ---
  🔥 方案二：使用TensorRT（终极性能）

  如果你想要极致性能（比CUDA再快1.5-2倍），可以使用TensorRT：

  TensorRT性能对比

  | 后端       | RTX 3060速度 | 相对提升  |
  |----------|------------|-------|
  | CPU      | 20 FPS     | 1x    |
  | CUDA     | 150 FPS    | 7.5x  |
  | TensorRT | 250 FPS    | 12.5x |

  TensorRT部署步骤

  # 1. 下载安装TensorRT
  # https://developer.nvidia.com/tensorrt
  # 选择对应CUDA版本的TensorRT 8.x

  # 2. 下载ONNX Runtime TensorRT版本
  # https://github.com/microsoft/onnxruntime/releases
  # onnxruntime-win-x64-tensorrt-1.17.0.zip

  # 3. 修改代码，使用TensorRT Provider
  OrtTensorRTProviderOptions trt_options;
  trt_options.device_id = 0;
  trt_options.trt_max_workspace_size = 2147483648;  // 2GB
  trt_options.trt_fp16_enable = 1;  // 启用FP16混合精度 (更快)

  session_options_.AppendExecutionProvider_TensorRT(trt_options);

  # 4. 重新编译和运行

  ---
  📊 快速对比总结

  | 配置       | 部署难度     | 性能提升  | 推荐场景     |
  |----------|----------|-------|----------|
  | CPU Only | ⭐ 简单     | 基准    | 测试/开发    |
  | CUDA     | ⭐⭐⭐ 中等   | 4-10x | ✅ 推荐生产环境 |
  | TensorRT | ⭐⭐⭐⭐⭐ 复杂 | 8-15x | 追求极致性能   |

  ---
  ✅ 完整部署检查清单（GPU版本）

  部署前确认：
  - NVIDIA显卡 (GTX 1050及以上)
  - 显卡驱动已更新 (nvidia-smi正常)
  - CUDA Toolkit已安装 (nvcc --version)
  - cuDNN已安装并复制到CUDA目录
  - ONNX Runtime GPU版本已下载
  - 环境变量已更新 (ONNXRUNTIME_DIR, PATH)
  - 代码已修改 (添加CUDA Provider)
  - 项目重新编译
  - GPU相关DLL已复制到可执行文件目录
  - 运行时看到"CUDA GPU acceleration enabled"
  - nvidia-smi显示GPU使用率上升

  ---
  🎯 我的建议

  推荐方案：ONNX Runtime + CUDA
  - ✅ 性能提升显著（4-10倍）
  - ✅ 部署相对简单
  - ✅ 稳定性好
  - ✅ 支持多种GPU型号

  如果你确定要启用GPU加速，我可以帮你：
  1. 修改代码文件以启用CUDA
  2. 提供详细的编译脚本
  3. 创建GPU版本的自动部署脚本

