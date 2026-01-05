  📁 项目代码结构

  YOLO-实时显示髋部坐标/
  ├── 核心源代码 (C++)
  │   ├── yolo_pose_detector.h        # YOLO推理引擎头文件
  │   ├── yolo_pose_detector.cpp      # YOLO推理引擎实现 (536行)
  │   ├── pose_utils.h                # 姿态处理工具头文件
  │   ├── pose_utils.cpp              # 姿态处理工具实现 (410行)
  │   ├── get_pose_with_depth.cpp     # RGB+深度相机版本 (425行)
  │   ├── get_pose_indemind_left.cpp  # RGB单目相机版本 (805行)
  │   └── get_pose_rgb_only.cpp       # 通用摄像头版本 (447行)
  │
  ├── 构建配置
  │   ├── CMakeLists.txt              # 跨平台构建配置 (283行)
  │   ├── build_linux.sh              # Linux构建脚本
  │   └── build_windows.bat           # Windows构建脚本
  │
  ├── SDK和依赖
  │   ├── include/                    # IMSEE SDK头文件
  │   │   ├── imrsdk.h
  │   │   ├── imrdata.h
  │   │   └── types.h
  │   └── lib/                        # IMSEE SDK库文件
  │       ├── libindemind.so          # Linux库
  │       └── indemind.dll/lib        # Windows库
  │
  ├── 模型文件
  │   └── models/
  │       └── yolov8n-pose.onnx       # YOLOv8 Nano姿态模型 (13.5MB)
  │
  ├── 工具脚本
  │   ├── install_onnxruntime.sh      # ONNX Runtime安装脚本
  │   ├── prepare_yolo_model.py       # 模型准备脚本 (155行)
  │   └── quickstart_yolo_pose.sh     # 快速启动脚本
  │
  ├── 编译输出
  │   └── build/                      # CMake构建输出目录
  │       ├── yolo_pose_detection.exe
  │       ├── yolo_pose_indemind_left.exe
  │       └── yolo_pose_rgb_only.exe
  │
  └── 文档
      └── README/
          ├── README.md
          ├── DEPLOYMENT_GUIDE.md     # 部署指南 (760行)
          ├── PROBLEMS_AND_SOLUTIONS.md  # 问题解决 (1,393行)
          └── 其他文档...

  ---
  🪟 Windows环境完整部署说明

  第一步：安装开发工具

  1.1 安装Visual Studio 2019或更高版本

  下载地址: https://visualstudio.microsoft.com/zh-hans/downloads/

  安装步骤:
  1. 运行安装程序
  2. 选择"使用C++的桌面开发"工作负载
  3. 确保包含以下组件:
     ✓ MSVC v142编译器
     ✓ Windows 10 SDK
     ✓ CMake工具

  1.2 安装CMake

  下载地址: https://cmake.org/download/

  安装步骤:
  1. 下载Windows x64 Installer
  2. 安装时勾选"Add CMake to system PATH for all users"
  3. 验证安装: 打开CMD运行
     cmake --version

  1.3 安装Python (用于模型准备)

  下载地址: https://www.python.org/downloads/

  安装步骤:
  1. 下载Python 3.8+
  2. 安装时勾选"Add Python to PATH"
  3. 验证安装:
     python --version

  ---
  第二步:安装依赖库

  2.1 安装OpenCV (推荐使用预编译包)

  方法A: 使用官方预编译包 ⭐推荐
  # 1. 下载OpenCV
  访问: https://opencv.org/releases/
  下载: opencv-4.8.0-windows.exe (或最新版本)

  # 2. 解压到C盘
  解压路径示例: C:\opencv

  # 3. 添加环境变量
  # 按 Win+R，输入 sysdm.cpl，打开系统属性
  # 高级 -> 环境变量 -> 系统变量

  添加到Path:
  C:\opencv\build\x64\vc15\bin

  新建变量:
  变量名: OpenCV_DIR
  变量值: C:\opencv\build

  方法B: 使用vcpkg
  # 1. 安装vcpkg
  git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
  cd C:\vcpkg
  .\bootstrap-vcpkg.bat

  # 2. 安装OpenCV
  .\vcpkg install opencv:x64-windows

  # 3. 设置环境变量
  $env:OpenCV_DIR="C:\vcpkg\installed\x64-windows"

  2.2 安装ONNX Runtime

  # 1. 下载ONNX Runtime
  访问: https://github.com/microsoft/onnxruntime/releases
  下载: onnxruntime-win-x64-1.17.0.zip (或最新版本)

  # 2. 解压到指定目录
  解压路径示例: C:\onnxruntime

  目录结构:
  C:\onnxruntime\
    ├── include\
    │   └── onnxruntime_cxx_api.h
    └── lib\
        ├── onnxruntime.lib
        └── onnxruntime.dll

  # 3. 添加环境变量
  添加到Path:
  C:\onnxruntime\lib

  新建变量:
  变量名: ONNXRUNTIME_DIR
  变量值: C:\onnxruntime

  2.3 安装Python依赖 (用于模型准备)

  # 打开CMD或PowerShell
  pip install torch ultralytics

  ---
  第三步:准备项目文件

  # 1. 将项目文件夹复制到合适位置 (例如桌面或D盘)
  示例路径: D:\YOLO-实时显示髋部坐标

  # 2. 进入项目目录
  cd D:\YOLO-实时显示髋部坐标

  # 3. 检查目录结构
  dir
  应该看到:
    ├── get_pose_with_depth.cpp
    ├── yolo_pose_detector.cpp
    ├── yolo_pose_detector.h
    ├── pose_utils.cpp
    ├── pose_utils.h
    ├── CMakeLists.txt
    ├── include\
    ├── lib\
    └── models\

  # 4. 确认Windows库文件存在
  dir lib
  应该看到:
    indemind.dll
    indemind.lib

  ---
  第四步:准备YOLO模型

  # 运行模型准备脚本
  python prepare_yolo_model.py

  # 按提示操作:
  # 1. 选择模型大小: 输入 n (nano,最快) 按回车
  # 2. 输入图像大小: 直接按回车使用默认值640

  # 验证模型文件
  dir models\yolov8n-pose.onnx
  应该看到约13.5MB的文件

  ---
  第五步:编译项目

  5.1 使用提供的批处理脚本 ⭐推荐

  # 打开"Developer Command Prompt for VS 2019"
  # (从开始菜单搜索)

  # 进入项目目录
  cd D:\YOLO-实时显示髋部坐标

  # 运行构建脚本
  build_windows.bat

  # 脚本会自动:
  # - 检查依赖
  # - 创建build目录
  # - 配置CMake
  # - 编译项目

  5.2 手动编译

  # 1. 打开"Developer Command Prompt for VS 2019"

  # 2. 进入项目目录
  cd D:\YOLO-实时显示髋部坐标

  # 3. 创建构建目录
  mkdir build
  cd build

  # 4. 配置CMake
  cmake .. -G "Visual Studio 16 2019" -A x64

  # 如果CMake找不到依赖,手动指定路径:
  cmake .. -G "Visual Studio 16 2019" -A x64 `
    -DOpenCV_DIR="C:\opencv\build" `
    -DONNXRUNTIME_INCLUDE_DIR="C:\onnxruntime\include" `
    -DONNXRUNTIME_LIB="C:\onnxruntime\lib\onnxruntime.lib"

  # 5. 编译 (Release模式)
  cmake --build . --config Release

  # 6. 检查编译结果
  dir Release
  应该看到三个可执行文件:
    yolo_pose_detection.exe         (RGB+深度版本)
    yolo_pose_indemind_left.exe     (RGB单目版本)
    yolo_pose_rgb_only.exe          (通用摄像头版本)

  ---
  第六步:准备运行环境

  # 1. 复制必要的DLL文件到可执行文件目录
  cd D:\YOLO-实时显示髋部坐标\build\Release

  # 复制ONNX Runtime DLL
  copy C:\onnxruntime\lib\onnxruntime.dll .

  # 复制OpenCV DLL
  copy C:\opencv\build\x64\vc15\bin\opencv_world480.dll .
  # (文件名根据你的OpenCV版本,可能是opencv_world4100.dll等)

  # 复制IMSEE SDK DLL
  copy ..\..\lib\indemind.dll .

  # 2. 验证所有DLL已复制
  dir *.dll
  应该看到:
    onnxruntime.dll
    opencv_world*.dll
    indemind.dll

  ---
  第七步:运行程序

  # 方式1: 使用IMSEE双目相机 (RGB+深度)
  .\yolo_pose_detection.exe

  # 方式2: 使用IMSEE单目相机 (仅RGB)
  .\yolo_pose_indemind_left.exe

  # 方式3: 使用普通摄像头/视频文件
  .\yolo_pose_rgb_only.exe --camera 0        # 使用摄像头
  .\yolo_pose_rgb_only.exe --video test.mp4  # 使用视频文件
  .\yolo_pose_rgb_only.exe --image test.jpg  # 使用图片

  # 程序快捷键:
  # q 或 ESC : 退出程序
  # b        : 切换边界框显示
  # k        : 切换关键点显示
  # s        : 切换骨架显示
  # i        : 切换信息叠加
  # SPACE    : 保存当前帧

  ---
  🔧 常见问题排查

  问题1: 提示找不到DLL

  错误: 找不到 onnxruntime.dll / opencv_world*.dll

  解决方法:
  1. 将DLL文件复制到可执行文件所在目录
  2. 或将DLL所在目录添加到系统PATH环境变量

  问题2: CMake找不到OpenCV

  错误: Could not find OpenCV

  解决方法:
  # 使用 -DOpenCV_DIR 手动指定路径
  cmake .. -G "Visual Studio 16 2019" -A x64 -DOpenCV_DIR="C:\opencv\build"

  问题3: 链接错误

  错误: LNK2001: unresolved external symbol

  解决方法:
  1. 确保使用相同的编译模式 (Release vs Release)
  2. 确保所有库都是x64架构
  3. 检查Visual Studio版本和OpenCV预编译版本是否匹配

  问题4: 相机无法打开

  错误: Failed to open camera

  解决方法:
  1. 检查IMSEE相机是否连接
  2. 检查驱动是否安装
  3. 在设备管理器中确认相机设备正常
  4. 尝试管理员权限运行程序

  ---
  📋 快速检查清单

  部署前请确认:
  - Visual Studio 2019+ 已安装
  - CMake 3.10+ 已安装并加入PATH
  - OpenCV已安装并设置环境变量
  - ONNX Runtime已安装并设置环境变量
  - Python 3.8+ 已安装
  - 项目文件完整 (include/, lib/, models/)
  - YOLO模型文件已准备 (models/yolov8n-pose.onnx)
  - 所有DLL文件已复制到可执行文件目录

  ---

