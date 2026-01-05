# ONNX Runtime 卸载脚本
# 用于卸载CPU版本，准备安装GPU版本
# Author: Chris
# Date: 2025-11-20

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  ONNX Runtime 卸载工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
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
    } else {
        Write-Host "  已跳过" -ForegroundColor Gray
    }
} else {
    Write-Host "  未找到ONNX Runtime目录" -ForegroundColor Gray
}

# 2. 清理环境变量
Write-Host ""
Write-Host "[2/4] 清理环境变量..." -ForegroundColor Yellow

# 检查是否有管理员权限
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($isAdmin) {
    # 删除ONNXRUNTIME_DIR
    [System.Environment]::SetEnvironmentVariable('ONNXRUNTIME_DIR', $null, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "  ✓ 已删除 ONNXRUNTIME_DIR" -ForegroundColor Green

    # 从PATH中移除
    $path = [System.Environment]::GetEnvironmentVariable('Path', [System.EnvironmentVariableTarget]::Machine)
    $newPath = ($path.Split(';') | Where-Object { $_ -notlike '*onnxruntime*' }) -join ';'
    [System.Environment]::SetEnvironmentVariable('Path', $newPath, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "  ✓ 已从PATH中移除" -ForegroundColor Green
} else {
    Write-Host "  ⚠ 需要管理员权限来清理环境变量" -ForegroundColor Red
    Write-Host "  请以管理员身份运行此脚本，或手动清理环境变量" -ForegroundColor Yellow
}

# 3. 清理项目DLL
Write-Host ""
Write-Host "[3/4] 清理项目中的DLL..." -ForegroundColor Yellow
$projectPath = Get-Location
$dllPatterns = @("onnxruntime.dll", "onnxruntime_providers_*.dll")
$deletedCount = 0

foreach ($pattern in $dllPatterns) {
    $files = Get-ChildItem -Path $projectPath -Filter $pattern -Recurse -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        Remove-Item $file.FullName -Force
        Write-Host "  ✓ 已删除: $($file.Name) ($($file.DirectoryName))" -ForegroundColor Green
        $deletedCount++
    }
}

if ($deletedCount -eq 0) {
    Write-Host "  未找到需要删除的DLL文件" -ForegroundColor Gray
}

# 4. 清理CMake缓存
Write-Host ""
Write-Host "[4/4] 清理CMake缓存..." -ForegroundColor Yellow
if (Test-Path "build") {
    $confirm = Read-Host "  是否删除build目录? (Y/N)"
    if ($confirm -eq 'Y' -or $confirm -eq 'y') {
        Remove-Item -Recurse -Force build
        Write-Host "  ✓ 已删除build目录" -ForegroundColor Green
    } else {
        Write-Host "  已跳过" -ForegroundColor Gray
    }
} else {
    Write-Host "  未找到build目录" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  卸载完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "后续步骤:" -ForegroundColor Yellow
Write-Host "1. 重启PowerShell/CMD窗口以刷新环境变量" -ForegroundColor White
Write-Host "2. 验证卸载: " -NoNewline -ForegroundColor White
Write-Host "echo `$env:ONNXRUNTIME_DIR" -ForegroundColor Cyan
Write-Host "   (应该返回空白)" -ForegroundColor Gray
Write-Host "3. 下载ONNX Runtime GPU版本:" -ForegroundColor White
Write-Host "   https://github.com/microsoft/onnxruntime/releases" -ForegroundColor Cyan
Write-Host "   文件: onnxruntime-win-x64-gpu-1.17.0.zip" -ForegroundColor Cyan
Write-Host "4. 解压到: C:\onnxruntime-gpu" -ForegroundColor White
Write-Host "5. 设置环境变量:" -ForegroundColor White
Write-Host "   ONNXRUNTIME_DIR = C:\onnxruntime-gpu" -ForegroundColor Cyan
Write-Host "   PATH 添加: C:\onnxruntime-gpu\lib" -ForegroundColor Cyan
Write-Host ""

if (-not $isAdmin) {
    Write-Host "⚠ 提醒: 由于没有管理员权限，请手动清理环境变量" -ForegroundColor Yellow
    Write-Host "   1. 按 Win+R，输入 sysdm.cpl" -ForegroundColor Gray
    Write-Host "   2. 高级 -> 环境变量" -ForegroundColor Gray
    Write-Host "   3. 删除 ONNXRUNTIME_DIR" -ForegroundColor Gray
    Write-Host "   4. 从PATH中移除包含onnxruntime的路径" -ForegroundColor Gray
    Write-Host ""
}
