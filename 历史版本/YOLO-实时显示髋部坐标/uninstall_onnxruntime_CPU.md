 🗑️ ONNX Runtime CPU版本完整卸载步骤

  第一步：删除ONNX Runtime文件夹

  # 1. 删除之前解压的ONNX Runtime目录
  # 假设之前安装在 C:\onnxruntime

  Remove-Item -Recurse -Force C:\onnxruntime

  # 或者手动删除：
  # 打开文件资源管理器，找到 C:\onnxruntime 文件夹
  # 右键 -> 删除

  第二步：清理环境变量

  # 方式A: 使用图形界面（推荐）
  # 1. 按 Win + R，输入 sysdm.cpl，按回车
  # 2. 切换到"高级"选项卡
  # 3. 点击"环境变量"

  # 4. 在"系统变量"中找到并删除：
  #    变量名: ONNXRUNTIME_DIR
  #    变量值: C:\onnxruntime
  #    -> 选中后点击"删除"

  # 5. 在"系统变量"中找到 Path
  #    -> 双击打开
  #    -> 找到并删除: C:\onnxruntime\lib
  #    -> 点击"确定"

  # 6. 一路点击"确定"保存更改

  # 方式B: 使用PowerShell（管理员权限）
  # ⚠️ 需要以管理员身份运行PowerShell

  # 删除ONNXRUNTIME_DIR环境变量
  [System.Environment]::SetEnvironmentVariable('ONNXRUNTIME_DIR', $null,
  [System.EnvironmentVariableTarget]::Machine)

  # 从PATH中删除ONNX Runtime路径
  $path = [System.Environment]::GetEnvironmentVariable('Path',
  [System.EnvironmentVariableTarget]::Machine)
  $newPath = ($path.Split(';') | Where-Object { $_ -notlike '*onnxruntime*'
  }) -join ';'
  [System.Environment]::SetEnvironmentVariable('Path', $newPath,
  [System.EnvironmentVariableTarget]::Machine)

  # 刷新环境变量
  $env:Path = [System.Environment]::GetEnvironmentVariable("Path",
  "Machine")

  第三步：清理项目中的DLL文件

  # 进入项目目录
  cd D:\YOLO-实时显示髋部坐标

  # 删除之前复制的ONNX Runtime DLL
  Remove-Item -Force build\Release\onnxruntime.dll -ErrorAction
  SilentlyContinue
  Remove-Item -Force build\Release\onnxruntime_providers_*.dll -ErrorAction
  SilentlyContinue

  # 或者手动删除 build\Release\ 目录下的:
  # - onnxruntime.dll
  # - onnxruntime_providers_shared.dll (如果有)
  # - onnxruntime_providers_cuda.dll (如果有)

  第四步：清理CMake缓存

  # 删除构建目录（这会清除CMake缓存）
  cd D:\YOLO-实时显示髋部坐标
  Remove-Item -Recurse -Force build

  # 或者只删除CMake缓存文件
  Remove-Item -Force build\CMakeCache.txt -ErrorAction SilentlyContinue
  Remove-Item -Recurse -Force build\CMakeFiles -ErrorAction SilentlyContinue

  第五步：验证卸载完成

  # 1. 检查目录是否已删除
  Test-Path C:\onnxruntime
  # 应该返回: False

  # 2. 检查环境变量
  echo $env:ONNXRUNTIME_DIR
  # 应该返回: (空白)

  # 3. 检查PATH中是否还有onnxruntime
  echo $env:Path | Select-String "onnxruntime"
  # 应该没有任何输出

  # 4. 重启PowerShell/CMD窗口
  # 以确保环境变量更新生效

  ---
  ✅ 卸载完成后，安装GPU版本

  卸载完成后，按照之前的说明安装GPU版本：

  # 1. 下载ONNX Runtime GPU版本
  # https://github.com/microsoft/onnxruntime/releases
  # 下载: onnxruntime-win-x64-gpu-1.17.0.zip

  # 2. 解压到新目录
  # 例如: C:\onnxruntime-gpu

  # 3. 设置新的环境变量
  # ONNXRUNTIME_DIR = C:\onnxruntime-gpu
  # PATH 添加: C:\onnxruntime-gpu\lib

  # 4. 验证新安装
  dir C:\onnxruntime-gpu\lib

  # 应该看到:
  # onnxruntime.dll
  # onnxruntime.lib
  # onnxruntime_providers_cuda.dll      # GPU版本特有
  # onnxruntime_providers_shared.dll    # GPU版本特有

  ---
  🔍 快速卸载脚本

  为了方便，我可以创建一个自动卸载脚本：

  创建文件：uninstall_onnxruntime.ps1

  # ONNX Runtime 卸载脚本
  Write-Host "========================================" -ForegroundColor
  Cyan
  Write-Host "  ONNX Runtime 卸载工具" -ForegroundColor Cyan
  Write-Host "========================================" -ForegroundColor
  Cyan
  Write-Host ""

  # 1. 查找ONNX Runtime安装位置
  $onnxPath = $env:ONNXRUNTIME_DIR
  if (-not $onnxPath) {
      $onnxPath = "C:\onnxruntime"
  }

  Write-Host "[1/4] 检查安装位置..." -ForegroundColor Yellow
  if (Test-Path $onnxPath) {
      Write-Host "  找到: $onnxPath" -ForegroundColor Green
      $confirm = Read-Host "  是否删除此目录? (Y/N)"
      if ($confirm -eq 'Y' -or $confirm -eq 'y') {
          Remove-Item -Recurse -Force $onnxPath
          Write-Host "  ✓ 已删除" -ForegroundColor Green
      }
  } else {
      Write-Host "  未找到ONNX Runtime目录" -ForegroundColor Gray
  }

  # 2. 清理环境变量
  Write-Host ""
  Write-Host "[2/4] 清理环境变量..." -ForegroundColor Yellow

  # 检查是否有管理员权限
  $isAdmin = ([Security.Principal.WindowsPrincipal]
  [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Pri
  ncipal.WindowsBuiltInRole]::Administrator)

  if ($isAdmin) {
      # 删除ONNXRUNTIME_DIR
      [System.Environment]::SetEnvironmentVariable('ONNXRUNTIME_DIR', $null,
   [System.EnvironmentVariableTarget]::Machine)
      Write-Host "  ✓ 已删除 ONNXRUNTIME_DIR" -ForegroundColor Green

      # 从PATH中移除
      $path = [System.Environment]::GetEnvironmentVariable('Path',
  [System.EnvironmentVariableTarget]::Machine)
      $newPath = ($path.Split(';') | Where-Object { $_ -notlike
  '*onnxruntime*' }) -join ';'
      [System.Environment]::SetEnvironmentVariable('Path', $newPath,
  [System.EnvironmentVariableTarget]::Machine)
      Write-Host "  ✓ 已从PATH中移除" -ForegroundColor Green
  } else {
      Write-Host "  ⚠ 需要管理员权限来清理环境变量" -ForegroundColor Red
      Write-Host "  请以管理员身份运行此脚本" -ForegroundColor Red
  }

  # 3. 清理项目DLL
  Write-Host ""
  Write-Host "[3/4] 清理项目中的DLL..." -ForegroundColor Yellow
  $projectPath = Get-Location
  $dllPatterns = @("onnxruntime.dll", "onnxruntime_providers_*.dll")

  foreach ($pattern in $dllPatterns) {
      $files = Get-ChildItem -Path $projectPath -Filter $pattern -Recurse
  -ErrorAction SilentlyContinue
      foreach ($file in $files) {
          Remove-Item $file.FullName -Force
          Write-Host "  ✓ 已删除: $($file.FullName)" -ForegroundColor Green
      }
  }

  # 4. 清理CMake缓存
  Write-Host ""
  Write-Host "[4/4] 清理CMake缓存..." -ForegroundColor Yellow
  if (Test-Path "build") {
      Remove-Item -Recurse -Force build
      Write-Host "  ✓ 已删除build目录" -ForegroundColor Green
  }

  Write-Host ""
  Write-Host "========================================" -ForegroundColor
  Cyan
  Write-Host "  卸载完成！" -ForegroundColor Green
  Write-Host "========================================" -ForegroundColor
  Cyan
  Write-Host ""
  Write-Host "建议:" -ForegroundColor Yellow
  Write-Host "1. 重启PowerShell/CMD窗口" -ForegroundColor Gray
  Write-Host "2. 验证: echo `$env:ONNXRUNTIME_DIR (应该为空)"
  -ForegroundColor Gray
  Write-Host "3. 现在可以安装GPU版本" -ForegroundColor Gray
  Write-Host ""

  使用方法：

  # 1. 保存上面的脚本为 uninstall_onnxruntime.ps1

  # 2. 以管理员身份运行PowerShell
  # 右键点击PowerShell -> 以管理员身份运行

  # 3. 进入项目目录
  cd D:\YOLO-实时显示髋部坐标

  # 4. 运行脚本
  .\uninstall_onnxruntime.ps1

  # 5. 按照提示操作

  ---
  📋 卸载检查清单

  完成后请验证：
  - C:\onnxruntime 目录已删除
  - 环境变量 ONNXRUNTIME_DIR 已删除
  - PATH 中没有 onnxruntime 路径
  - 项目中没有 onnxruntime.dll
  - build 目录已清理
  - 重启了命令行窗口

