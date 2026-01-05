# ONNX Runtime GPU版本安装辅助脚本
# Author: Chris
# Date: 2025-11-20

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  ONNX Runtime GPU版本安装向导" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查管理员权限
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "⚠ 警告: 需要管理员权限来设置环境变量" -ForegroundColor Yellow
    Write-Host "建议: 右键点击PowerShell -> 以管理员身份运行" -ForegroundColor Yellow
    Write-Host ""
}

# 步骤1: 检查NVIDIA显卡
Write-Host "[步骤 1/6] 检查NVIDIA显卡..." -ForegroundColor Yellow
try {
    $nvidiaCheck = nvidia-smi 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ 检测到NVIDIA显卡" -ForegroundColor Green
        nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader
    } else {
        Write-Host "✗ 未检测到NVIDIA显卡或驱动" -ForegroundColor Red
        Write-Host "请先安装NVIDIA显卡驱动: https://www.nvidia.com/Download/index.aspx" -ForegroundColor Yellow
        exit 1
    }
} catch {
    Write-Host "✗ nvidia-smi命令不可用" -ForegroundColor Red
    Write-Host "请先安装NVIDIA显卡驱动" -ForegroundColor Yellow
    exit 1
}

Write-Host ""

# 步骤2: 检查CUDA
Write-Host "[步骤 2/6] 检查CUDA Toolkit..." -ForegroundColor Yellow
try {
    $cudaVersion = nvcc --version 2>&1 | Select-String "release"
    if ($cudaVersion) {
        Write-Host "✓ CUDA已安装: $cudaVersion" -ForegroundColor Green
    } else {
        Write-Host "⚠ 未检测到CUDA Toolkit" -ForegroundColor Yellow
        Write-Host "下载地址: https://developer.nvidia.com/cuda-downloads" -ForegroundColor Cyan
        $continue = Read-Host "是否继续? (Y/N)"
        if ($continue -ne 'Y' -and $continue -ne 'y') {
            exit 0
        }
    }
} catch {
    Write-Host "⚠ 未检测到CUDA Toolkit" -ForegroundColor Yellow
    Write-Host "下载地址: https://developer.nvidia.com/cuda-downloads" -ForegroundColor Cyan
}

Write-Host ""

# 步骤3: 选择安装路径
Write-Host "[步骤 3/6] 选择安装路径..." -ForegroundColor Yellow
$defaultPath = "C:\onnxruntime-gpu"
Write-Host "默认路径: $defaultPath" -ForegroundColor Gray
$customPath = Read-Host "请输入安装路径 (直接回车使用默认路径)"
if ([string]::IsNullOrWhiteSpace($customPath)) {
    $installPath = $defaultPath
} else {
    $installPath = $customPath
}

Write-Host "安装路径: $installPath" -ForegroundColor Cyan

# 检查路径是否已存在
if (Test-Path $installPath) {
    Write-Host "⚠ 路径已存在" -ForegroundColor Yellow
    $overwrite = Read-Host "是否覆盖? (Y/N)"
    if ($overwrite -ne 'Y' -and $overwrite -ne 'y') {
        Write-Host "安装已取消" -ForegroundColor Gray
        exit 0
    }
}

Write-Host ""

# 步骤4: 下载提示
Write-Host "[步骤 4/6] 下载ONNX Runtime GPU版本..." -ForegroundColor Yellow
Write-Host "请手动下载以下文件:" -ForegroundColor Cyan
Write-Host ""
Write-Host "下载地址: https://github.com/microsoft/onnxruntime/releases" -ForegroundColor White
Write-Host "文件名: onnxruntime-win-x64-gpu-1.17.0.zip" -ForegroundColor White
Write-Host "        (或最新版本的GPU版本)" -ForegroundColor Gray
Write-Host ""

$zipPath = Read-Host "请输入下载的ZIP文件完整路径 (或拖拽文件到此窗口)"
$zipPath = $zipPath.Trim('"')

if (-not (Test-Path $zipPath)) {
    Write-Host "✗ 文件不存在: $zipPath" -ForegroundColor Red
    exit 1
}

Write-Host "✓ 找到文件: $zipPath" -ForegroundColor Green
Write-Host ""

# 步骤5: 解压文件
Write-Host "[步骤 5/6] 解压文件..." -ForegroundColor Yellow

# 创建临时目录
$tempDir = Join-Path $env:TEMP "onnxruntime_temp"
if (Test-Path $tempDir) {
    Remove-Item -Recurse -Force $tempDir
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

# 解压
Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force

# 查找解压后的目录
$extractedDir = Get-ChildItem -Path $tempDir -Directory | Select-Object -First 1

if (-not $extractedDir) {
    Write-Host "✗ 解压失败" -ForegroundColor Red
    exit 1
}

# 移动到安装目录
if (Test-Path $installPath) {
    Remove-Item -Recurse -Force $installPath
}
Move-Item -Path $extractedDir.FullName -Destination $installPath

# 清理临时目录
Remove-Item -Recurse -Force $tempDir

Write-Host "✓ 已解压到: $installPath" -ForegroundColor Green

# 验证关键文件
$requiredFiles = @(
    "lib\onnxruntime.dll",
    "lib\onnxruntime.lib",
    "lib\onnxruntime_providers_cuda.dll",
    "lib\onnxruntime_providers_shared.dll",
    "include\onnxruntime_cxx_api.h"
)

Write-Host "验证文件..." -ForegroundColor Gray
$allFilesExist = $true
foreach ($file in $requiredFiles) {
    $fullPath = Join-Path $installPath $file
    if (Test-Path $fullPath) {
        Write-Host "  ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file (未找到)" -ForegroundColor Red
        $allFilesExist = $false
    }
}

if (-not $allFilesExist) {
    Write-Host "⚠ 某些文件缺失，请检查下载的文件是否正确" -ForegroundColor Yellow
}

Write-Host ""

# 步骤6: 设置环境变量
Write-Host "[步骤 6/6] 设置环境变量..." -ForegroundColor Yellow

if ($isAdmin) {
    # 设置ONNXRUNTIME_DIR
    [System.Environment]::SetEnvironmentVariable('ONNXRUNTIME_DIR', $installPath, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "✓ 已设置 ONNXRUNTIME_DIR = $installPath" -ForegroundColor Green

    # 添加到PATH
    $libPath = Join-Path $installPath "lib"
    $path = [System.Environment]::GetEnvironmentVariable('Path', [System.EnvironmentVariableTarget]::Machine)

    # 移除旧的onnxruntime路径
    $newPath = ($path.Split(';') | Where-Object { $_ -notlike '*onnxruntime*' }) -join ';'

    # 添加新路径
    if ($newPath -notlike "*$libPath*") {
        $newPath = "$newPath;$libPath"
        [System.Environment]::SetEnvironmentVariable('Path', $newPath, [System.EnvironmentVariableTarget]::Machine)
        Write-Host "✓ 已添加到PATH: $libPath" -ForegroundColor Green
    }

    # 刷新当前会话的环境变量
    $env:ONNXRUNTIME_DIR = $installPath
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")

} else {
    Write-Host "⚠ 需要管理员权限，请手动设置环境变量:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "1. 按 Win+R，输入 sysdm.cpl，按回车" -ForegroundColor White
    Write-Host "2. 高级 -> 环境变量" -ForegroundColor White
    Write-Host "3. 新建系统变量:" -ForegroundColor White
    Write-Host "   变量名: ONNXRUNTIME_DIR" -ForegroundColor Cyan
    Write-Host "   变量值: $installPath" -ForegroundColor Cyan
    Write-Host "4. 编辑PATH，添加:" -ForegroundColor White
    Write-Host "   $installPath\lib" -ForegroundColor Cyan
    Write-Host ""
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  安装完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "下一步操作:" -ForegroundColor Yellow
Write-Host "1. 重启PowerShell/CMD窗口" -ForegroundColor White
Write-Host "2. 验证环境变量: " -NoNewline -ForegroundColor White
Write-Host "echo `$env:ONNXRUNTIME_DIR" -ForegroundColor Cyan
Write-Host "3. 修改代码启用GPU (使用 yolo_pose_detector_gpu.cpp)" -ForegroundColor White
Write-Host "4. 重新编译项目" -ForegroundColor White
Write-Host "5. 复制GPU DLL到可执行文件目录" -ForegroundColor White
Write-Host ""
Write-Host "详细说明请查看: README/GPU_DEPLOYMENT_GUIDE.md" -ForegroundColor Gray
Write-Host ""
