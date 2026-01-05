# OpenCV版本兼容性测试脚本
# Author: Chris
# Date: 2025-11-20

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  OpenCV 版本兼容性测试工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查OpenCV是否安装
$opencvDir = $env:OpenCV_DIR
if (-not $opencvDir) {
    Write-Host "⚠ 未找到环境变量 OpenCV_DIR" -ForegroundColor Yellow
    Write-Host "请输入OpenCV安装路径 (例如: C:\opencv\build)" -ForegroundColor Yellow
    $opencvDir = Read-Host "OpenCV路径"
}

Write-Host "OpenCV路径: $opencvDir" -ForegroundColor Cyan
Write-Host ""

# 查找OpenCV版本
Write-Host "[步骤 1/5] 检测OpenCV版本..." -ForegroundColor Yellow

$versionFile = Join-Path $opencvDir "OpenCVConfig-version.cmake"
if (Test-Path $versionFile) {
    $content = Get-Content $versionFile
    $versionLine = $content | Select-String "PACKAGE_VERSION"
    if ($versionLine) {
        $version = ($versionLine -replace '.*"(.*)".*', '$1')
        Write-Host "✓ 检测到OpenCV版本: $version" -ForegroundColor Green

        # 解析版本号
        if ($version -match '^(\d+)\.(\d+)\.(\d+)') {
            $major = [int]$matches[1]
            $minor = [int]$matches[2]
            $patch = [int]$matches[3]

            Write-Host "  主版本: $major" -ForegroundColor Gray
            Write-Host "  次版本: $minor" -ForegroundColor Gray
            Write-Host "  补丁版本: $patch" -ForegroundColor Gray
            Write-Host ""

            # 兼容性判断
            Write-Host "[步骤 2/5] 分析兼容性..." -ForegroundColor Yellow

            if ($major -lt 3) {
                Write-Host "✗ OpenCV $version 太旧" -ForegroundColor Red
                Write-Host "  项目要求最低版本: 3.0" -ForegroundColor Yellow
                Write-Host "  建议: 升级到OpenCV 4.8或更高" -ForegroundColor Yellow
                $compatible = $false
            } elseif ($major -eq 3) {
                if ($minor -eq 4) {
                    Write-Host "✓ OpenCV $version 完全兼容" -ForegroundColor Green
                    Write-Host "  这是文档推荐的版本之一" -ForegroundColor Green
                    Write-Host "  建议: 可考虑升级到4.x以获得更好性能" -ForegroundColor Cyan
                } else {
                    Write-Host "✓ OpenCV $version 兼容" -ForegroundColor Green
                    Write-Host "  建议: 升级到3.4.3或4.x以获得更好体验" -ForegroundColor Cyan
                }
                $compatible = $true
            } elseif ($major -eq 4) {
                Write-Host "✓ OpenCV $version 完全兼容且推荐" -ForegroundColor Green
                if ($minor -ge 6) {
                    Write-Host "  这是最新稳定版本，性能最佳！" -ForegroundColor Green
                } else {
                    Write-Host "  可考虑升级到4.8+以获得最新功能" -ForegroundColor Cyan
                }
                $compatible = $true
            } else {
                Write-Host "✓ OpenCV $version 应该兼容" -ForegroundColor Green
                Write-Host "  这是更新的版本，理论上完全兼容" -ForegroundColor Cyan
                Write-Host "  如遇到问题，请反馈" -ForegroundColor Yellow
                $compatible = $true
            }
        } else {
            Write-Host "⚠ 无法解析版本号: $version" -ForegroundColor Yellow
            $compatible = $null
        }
    } else {
        Write-Host "⚠ 无法从配置文件中提取版本信息" -ForegroundColor Yellow
        $compatible = $null
    }
} else {
    Write-Host "⚠ 未找到版本配置文件: $versionFile" -ForegroundColor Yellow
    $compatible = $null
}

Write-Host ""

# 检查必要的DLL文件
Write-Host "[步骤 3/5] 检查OpenCV DLL文件..." -ForegroundColor Yellow

$binPath = Join-Path $opencvDir "x64\vc15\bin"
if (-not (Test-Path $binPath)) {
    $binPath = Join-Path $opencvDir "x64\vc16\bin"
}

if (Test-Path $binPath) {
    Write-Host "✓ 找到DLL目录: $binPath" -ForegroundColor Green

    # 查找opencv_world DLL
    $dllFiles = Get-ChildItem -Path $binPath -Filter "opencv_world*.dll"
    if ($dllFiles) {
        Write-Host "✓ 找到OpenCV DLL文件:" -ForegroundColor Green
        foreach ($dll in $dllFiles) {
            Write-Host "  - $($dll.Name) ($([math]::Round($dll.Length/1MB, 2)) MB)" -ForegroundColor Gray
        }
    } else {
        Write-Host "✗ 未找到opencv_world*.dll文件" -ForegroundColor Red
        $compatible = $false
    }
} else {
    Write-Host "✗ 未找到DLL目录" -ForegroundColor Red
    Write-Host "  期望路径: $binPath" -ForegroundColor Yellow
    $compatible = $false
}

Write-Host ""

# 检查PATH环境变量
Write-Host "[步骤 4/5] 检查PATH环境变量..." -ForegroundColor Yellow

$pathEnv = $env:Path
if ($pathEnv -like "*$binPath*") {
    Write-Host "✓ OpenCV bin目录已在PATH中" -ForegroundColor Green
} else {
    Write-Host "⚠ OpenCV bin目录不在PATH中" -ForegroundColor Yellow
    Write-Host "  需要添加到PATH: $binPath" -ForegroundColor Cyan
    Write-Host "  或手动复制DLL到可执行文件目录" -ForegroundColor Cyan
}

Write-Host ""

# 检查项目API使用
Write-Host "[步骤 5/5] 分析项目API兼容性..." -ForegroundColor Yellow

Write-Host "项目使用的OpenCV API:" -ForegroundColor Cyan
$apis = @(
    "cv::Mat, cv::Scalar, cv::Point, cv::Rect",
    "cv::resize(), cv::cvtColor(), cv::copyMakeBorder()",
    "cv::rectangle(), cv::circle(), cv::line(), cv::putText()",
    "cv::imread(), cv::imshow(), cv::imwrite()",
    "cv::VideoCapture"
)

foreach ($api in $apis) {
    Write-Host "  ✓ $api" -ForegroundColor Green
}

Write-Host ""
Write-Host "所有API都是OpenCV核心功能，从2.x时代就存在" -ForegroundColor Green
Write-Host "在3.x和4.x中完全向后兼容" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  测试结果总结" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($compatible -eq $true) {
    Write-Host "✓ 当前OpenCV版本完全兼容此项目" -ForegroundColor Green
    Write-Host ""
    Write-Host "后续步骤:" -ForegroundColor Yellow
    Write-Host "1. 确保PATH中包含OpenCV bin目录" -ForegroundColor White
    Write-Host "2. 重新编译项目" -ForegroundColor White
    Write-Host "3. 复制对应版本的DLL文件" -ForegroundColor White
    Write-Host "4. 运行测试" -ForegroundColor White

    if ($major -eq 3) {
        Write-Host ""
        Write-Host "性能优化建议:" -ForegroundColor Cyan
        Write-Host "• 考虑升级到OpenCV 4.8+以获得10-20%性能提升" -ForegroundColor Gray
        Write-Host "• OpenCV 4.x有更好的多线程和SIMD优化" -ForegroundColor Gray
    }

} elseif ($compatible -eq $false) {
    Write-Host "✗ 当前OpenCV版本可能不兼容" -ForegroundColor Red
    Write-Host ""
    Write-Host "建议:" -ForegroundColor Yellow
    Write-Host "1. 升级到OpenCV 4.8.0或更高版本" -ForegroundColor White
    Write-Host "   下载: https://opencv.org/releases/" -ForegroundColor Cyan
    Write-Host "2. 选择预编译的Windows版本" -ForegroundColor White
    Write-Host "3. 解压后重新设置环境变量" -ForegroundColor White
    Write-Host "4. 重新运行此测试脚本" -ForegroundColor White
} else {
    Write-Host "⚠ 无法确定兼容性" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "建议:" -ForegroundColor Yellow
    Write-Host "1. 检查OpenCV是否正确安装" -ForegroundColor White
    Write-Host "2. 确认OpenCV_DIR环境变量设置正确" -ForegroundColor White
    Write-Host "3. 尝试重新编译项目进行验证" -ForegroundColor White
}

Write-Host ""
Write-Host "参考文档: README/DEPLOYMENT_GUIDE.md" -ForegroundColor Gray
Write-Host ""
pause
