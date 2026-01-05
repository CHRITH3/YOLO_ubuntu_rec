# GPU/CPU版本切换工具
# Author: Chris
# Date: 2025-11-20

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  YOLO Pose Detector 版本切换工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查文件是否存在
$currentFile = "yolo_pose_detector.cpp"
$gpuFile = "yolo_pose_detector_gpu.cpp"
$cpuBackup = "yolo_pose_detector_cpu_backup.cpp"

if (-not (Test-Path $gpuFile)) {
    Write-Host "✗ 找不到GPU版本文件: $gpuFile" -ForegroundColor Red
    Write-Host "请确保项目完整" -ForegroundColor Yellow
    pause
    exit 1
}

# 检测当前使用的版本
$currentContent = Get-Content $currentFile -Raw
if ($currentContent -match "GPU Accelerated Version") {
    $currentVersion = "GPU"
} else {
    $currentVersion = "CPU"
}

Write-Host "当前版本: " -NoNewline
if ($currentVersion -eq "GPU") {
    Write-Host "GPU加速版本" -ForegroundColor Green
} else {
    Write-Host "CPU版本" -ForegroundColor Yellow
}
Write-Host ""

# 显示菜单
Write-Host "请选择操作:" -ForegroundColor Yellow
Write-Host "  [1] 切换到GPU加速版本 (推荐，需要NVIDIA显卡)" -ForegroundColor White
Write-Host "  [2] 切换到CPU版本 (兼容性最佳)" -ForegroundColor White
Write-Host "  [3] 显示版本差异" -ForegroundColor White
Write-Host "  [0] 退出" -ForegroundColor Gray
Write-Host ""

$choice = Read-Host "请输入选项 (0-3)"

switch ($choice) {
    "1" {
        Write-Host ""
        Write-Host "切换到GPU版本..." -ForegroundColor Yellow

        # 备份当前文件（如果还没有备份）
        if (-not (Test-Path $cpuBackup)) {
            Copy-Item $currentFile $cpuBackup -Force
            Write-Host "✓ 已备份原始CPU版本到: $cpuBackup" -ForegroundColor Green
        }

        # 切换到GPU版本
        Copy-Item $gpuFile $currentFile -Force
        Write-Host "✓ 已切换到GPU加速版本" -ForegroundColor Green
        Write-Host ""
        Write-Host "后续步骤:" -ForegroundColor Yellow
        Write-Host "1. 确保已安装ONNX Runtime GPU版本" -ForegroundColor White
        Write-Host "2. 重新编译项目:" -ForegroundColor White
        Write-Host "   cd build" -ForegroundColor Cyan
        Write-Host "   cmake --build . --config Release" -ForegroundColor Cyan
        Write-Host "3. 复制GPU DLL到build\Release目录" -ForegroundColor White
        Write-Host "4. 运行程序，检查是否显示'CUDA GPU acceleration enabled'" -ForegroundColor White
        Write-Host ""
        Write-Host "详细说明: README/GPU_DEPLOYMENT_GUIDE.md" -ForegroundColor Gray
    }

    "2" {
        Write-Host ""
        Write-Host "切换到CPU版本..." -ForegroundColor Yellow

        if (Test-Path $cpuBackup) {
            # 恢复备份的CPU版本
            Copy-Item $cpuBackup $currentFile -Force
            Write-Host "✓ 已恢复到CPU版本" -ForegroundColor Green
        } else {
            Write-Host "⚠ 未找到CPU备份文件" -ForegroundColor Yellow
            Write-Host "当前文件可能已经是CPU版本，或者需要从Git恢复" -ForegroundColor Yellow
        }

        Write-Host ""
        Write-Host "后续步骤:" -ForegroundColor Yellow
        Write-Host "1. 重新编译项目:" -ForegroundColor White
        Write-Host "   cd build" -ForegroundColor Cyan
        Write-Host "   cmake --build . --config Release" -ForegroundColor Cyan
        Write-Host "2. 运行程序" -ForegroundColor White
    }

    "3" {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "  版本差异说明" -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host ""

        Write-Host "GPU版本 (yolo_pose_detector_gpu.cpp):" -ForegroundColor Green
        Write-Host "  ✓ 启用NVIDIA CUDA GPU加速" -ForegroundColor White
        Write-Host "  ✓ 性能提升4-20倍" -ForegroundColor White
        Write-Host "  ✓ 自动检测GPU可用性" -ForegroundColor White
        Write-Host "  ✓ GPU不可用时自动回退到CPU" -ForegroundColor White
        Write-Host "  ✓ 优化显存管理" -ForegroundColor White
        Write-Host "  ✗ 需要NVIDIA显卡" -ForegroundColor Red
        Write-Host "  ✗ 需要安装CUDA和cuDNN" -ForegroundColor Red
        Write-Host "  ✗ 需要ONNX Runtime GPU版本" -ForegroundColor Red
        Write-Host ""

        Write-Host "CPU版本 (原始文件):" -ForegroundColor Yellow
        Write-Host "  ✓ 兼容所有系统" -ForegroundColor White
        Write-Host "  ✓ 无需额外依赖" -ForegroundColor White
        Write-Host "  ✓ 部署简单" -ForegroundColor White
        Write-Host "  ✗ 性能较低 (15-20 FPS)" -ForegroundColor Red
        Write-Host ""

        Write-Host "性能对比 (YOLOv8n, 640x640):" -ForegroundColor Cyan
        Write-Host "  CPU (Intel i7):     15-20 FPS" -ForegroundColor Gray
        Write-Host "  GPU (GTX 1660):    80-100 FPS  (+4-5x)" -ForegroundColor Yellow
        Write-Host "  GPU (RTX 3060):   150-200 FPS  (+8-10x)" -ForegroundColor Green
        Write-Host "  GPU (RTX 4090):   300-400 FPS  (+15-20x)" -ForegroundColor Magenta
        Write-Host ""

        Write-Host "代码主要差异:" -ForegroundColor Cyan
        Write-Host "  构造函数中添加了CUDA Provider配置" -ForegroundColor Gray
        Write-Host "  添加了GPU检测和错误处理逻辑" -ForegroundColor Gray
        Write-Host "  添加了详细的日志输出" -ForegroundColor Gray
        Write-Host ""

        pause
        return
    }

    "0" {
        Write-Host "已退出" -ForegroundColor Gray
        return
    }

    default {
        Write-Host "✗ 无效选项" -ForegroundColor Red
        pause
        return
    }
}

Write-Host ""
pause
