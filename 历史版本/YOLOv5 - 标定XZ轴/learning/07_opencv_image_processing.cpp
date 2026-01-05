/*
 * 07_opencv_image_processing.cpp
 *
 * 学习目标：
 * 1. 图像缩放（resize）
 * 2. 颜色空间转换（cvtColor）
 * 3. ROI（感兴趣区域）操作
 * 4. 理解项目中的图像预处理流程
 */

#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::cout << "=== OpenCV 图像处理基础 ===" << std::endl;

    // 创建一个测试图像 640x480 BGR
    cv::Mat srcImage(480, 640, CV_8UC3);
    srcImage.setTo(cv::Scalar(100, 150, 200));  // 填充颜色

    // 画一些特征方便观察
    cv::rectangle(srcImage, cv::Point(100, 100), cv::Point(540, 380),
                  cv::Scalar(0, 255, 0), 5);
    cv::circle(srcImage, cv::Point(320, 240), 80, cv::Scalar(0, 0, 255), -1);

    std::cout << "原始图像尺寸: " << srcImage.cols << "x" << srcImage.rows << std::endl;


    // ========== 1. 图像缩放 (resize) ==========
    std::cout << "\n--- 图像缩放 ---" << std::endl;

    // 方法1：指定目标尺寸
    cv::Mat resized1;
    cv::resize(srcImage, resized1, cv::Size(320, 240));  // 缩小到320x240
    std::cout << "缩放后尺寸: " << resized1.cols << "x" << resized1.rows << std::endl;

    // 方法2：按比例缩放
    cv::Mat resized2;
    cv::resize(srcImage, resized2, cv::Size(), 0.5, 0.5);  // 缩小到50%
    std::cout << "按比例缩放: " << resized2.cols << "x" << resized2.rows << std::endl;

    // 项目中的实际用法：缩放到YOLO输入尺寸 640x640
    cv::Mat yoloSize;
    cv::resize(srcImage, yoloSize, cv::Size(640, 640));
    std::cout << "YOLO输入尺寸: " << yoloSize.cols << "x" << yoloSize.rows << std::endl;


    // ========== 2. 颜色空间转换 (cvtColor) ==========
    std::cout << "\n--- 颜色空间转换 ---" << std::endl;

    // BGR -> RGB（YOLO模型需要RGB格式）
    cv::Mat rgbImage;
    cv::cvtColor(srcImage, rgbImage, cv::COLOR_BGR2RGB);
    std::cout << "BGR转RGB完成，通道数: " << rgbImage.channels() << std::endl;

    // BGR -> 灰度图
    cv::Mat grayImage;
    cv::cvtColor(srcImage, grayImage, cv::COLOR_BGR2GRAY);
    std::cout << "BGR转灰度完成，通道数: " << grayImage.channels() << std::endl;

    // BGR -> HSV（色调、饱和度、明度，用于颜色检测）
    cv::Mat hsvImage;
    cv::cvtColor(srcImage, hsvImage, cv::COLOR_BGR2HSV);
    std::cout << "BGR转HSV完成" << std::endl;


    // ========== 3. ROI（Region of Interest）操作 ==========
    std::cout << "\n--- ROI 感兴趣区域 ---" << std::endl;

    // 提取图像的一部分（例如：人体检测框内的区域）
    cv::Rect roi(100, 100, 200, 150);  // x, y, width, height
    cv::Mat roiImage = srcImage(roi);   // 浅拷贝，共享内存！

    std::cout << "ROI尺寸: " << roiImage.cols << "x" << roiImage.rows << std::endl;

    // 修改ROI会影响原图
    roiImage.setTo(cv::Scalar(255, 0, 255));  // 设置为紫色
    std::cout << "ROI修改会影响原图（因为是浅拷贝）" << std::endl;

    // 如果不想影响原图，使用clone()
    cv::Mat roiCopy = srcImage(roi).clone();  // 深拷贝


    // ========== 4. 项目中的图像预处理流程 ==========
    std::cout << "\n=== 项目中YOLO预处理的完整流程 ===" << std::endl;

    cv::Mat originalImg(480, 640, CV_8UC3);  // 模拟摄像头图像
    originalImg.setTo(cv::Scalar(100, 100, 100));

    std::cout << "步骤1: 原始图像 " << originalImg.cols << "x" << originalImg.rows << std::endl;

    // 步骤2: Letterbox resize（保持宽高比的缩放 + 填充）
    // 这是YOLO中最重要的预处理步骤！
    int target_w = 640, target_h = 640;
    float scale = std::min(target_w / (float)originalImg.cols,
                           target_h / (float)originalImg.rows);

    int new_w = originalImg.cols * scale;
    int new_h = originalImg.rows * scale;

    cv::Mat resized;
    cv::resize(originalImg, resized, cv::Size(new_w, new_h));

    // 创建640x640的黑色画布
    cv::Mat letterboxed = cv::Mat::zeros(640, 640, CV_8UC3);

    // 计算居中位置
    int top = (640 - new_h) / 2;
    int left = (640 - new_w) / 2;

    // 将缩放后的图像放到画布中央
    resized.copyTo(letterboxed(cv::Rect(left, top, new_w, new_h)));

    std::cout << "步骤2: Letterbox后 " << letterboxed.cols << "x" << letterboxed.rows
              << " (scale=" << scale << ", padding=" << top << "," << left << ")" << std::endl;

    // 步骤3: BGR -> RGB
    cv::Mat rgb;
    cv::cvtColor(letterboxed, rgb, cv::COLOR_BGR2RGB);
    std::cout << "步骤3: BGR->RGB转换完成" << std::endl;

    // 步骤4: 归一化到 [0, 1]（在实际代码中是转换为float后除以255）
    cv::Mat normalized;
    rgb.convertTo(normalized, CV_32F, 1.0 / 255.0);
    std::cout << "步骤4: 归一化到[0,1]，数据类型: CV_32F" << std::endl;

    // 步骤5: HWC -> CHW（Height,Width,Channel -> Channel,Height,Width）
    // 这一步在实际代码中通过数据重排实现，OpenCV没有直接API
    std::cout << "步骤5: HWC->CHW转换（需要手动重排数据）" << std::endl;

    std::cout << "\n最终输入YOLO的张量形状: [1, 3, 640, 640]" << std::endl;


    // ========== 5. 关键点总结 ==========
    std::cout << "\n=== 关键点总结 ===" << std::endl;
    std::cout << "1. cv::resize(src, dst, size) - 图像缩放" << std::endl;
    std::cout << "2. cv::cvtColor(src, dst, code) - 颜色空间转换" << std::endl;
    std::cout << "   - COLOR_BGR2RGB (YOLO需要)" << std::endl;
    std::cout << "   - COLOR_BGR2GRAY (灰度图)" << std::endl;
    std::cout << "3. cv::Rect + 索引操作 - 提取ROI" << std::endl;
    std::cout << "4. Letterbox预处理保持宽高比很重要！" << std::endl;
    std::cout << "5. 预处理顺序: Letterbox -> BGR2RGB -> 归一化 -> HWC2CHW" << std::endl;

    return 0;
}
