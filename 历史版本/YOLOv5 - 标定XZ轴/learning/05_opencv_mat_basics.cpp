/*
 * 05_opencv_mat_basics.cpp
 *
 * 学习目标：
 * 1. 理解 cv::Mat 是什么（OpenCV的核心数据结构）
 * 2. 学会创建、访问、复制 cv::Mat
 * 3. 理解图像的存储方式（行、列、通道）
 */

#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::cout << "=== OpenCV cv::Mat 基础 ===" << std::endl;

    // 1. 创建一个 3x3 的矩阵，类型为 CV_8UC1（8位无符号整数，1通道）
    // 就像C语言的 unsigned char array[3][3]
    cv::Mat mat1(3, 3, CV_8UC1);

    // 填充数据
    for(int row = 0; row < mat1.rows; row++) {
        for(int col = 0; col < mat1.cols; col++) {
            mat1.at<unsigned char>(row, col) = row * 10 + col;
        }
    }

    std::cout << "3x3 单通道矩阵：" << std::endl;
    std::cout << mat1 << std::endl << std::endl;


    // 2. 创建彩色图像矩阵（3通道：BGR）
    // CV_8UC3 = 8位无符号整数，3通道（Blue, Green, Red）
    cv::Mat colorImg(2, 2, CV_8UC3);

    // 设置第一个像素为红色（注意：OpenCV用BGR顺序，不是RGB！）
    colorImg.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 255);  // BGR = (0,0,255) = 红色
    colorImg.at<cv::Vec3b>(0, 1) = cv::Vec3b(255, 0, 0);  // BGR = (255,0,0) = 蓝色
    colorImg.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 255, 0);  // BGR = (0,255,0) = 绿色
    colorImg.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 255, 255); // 白色

    std::cout << "2x2 彩色图像（BGR格式）：" << std::endl;
    std::cout << colorImg << std::endl << std::endl;


    // 3. Mat的浅拷贝 vs 深拷贝（非常重要！）
    cv::Mat mat2 = mat1;           // 浅拷贝：mat2和mat1共享同一块内存
    cv::Mat mat3 = mat1.clone();   // 深拷贝：mat3拥有独立的内存

    // 修改mat2会影响mat1，但不影响mat3
    mat2.at<unsigned char>(0, 0) = 99;

    std::cout << "浅拷贝测试：" << std::endl;
    std::cout << "mat1(0,0) = " << (int)mat1.at<unsigned char>(0, 0) << " (被修改了！)" << std::endl;
    std::cout << "mat2(0,0) = " << (int)mat2.at<unsigned char>(0, 0) << std::endl;
    std::cout << "mat3(0,0) = " << (int)mat3.at<unsigned char>(0, 0) << " (未被修改)" << std::endl << std::endl;


    // 4. 项目中常见的Mat属性
    std::cout << "Mat的常用属性：" << std::endl;
    std::cout << "  rows（高度）: " << colorImg.rows << std::endl;
    std::cout << "  cols（宽度）: " << colorImg.cols << std::endl;
    std::cout << "  channels（通道数）: " << colorImg.channels() << std::endl;
    std::cout << "  type（数据类型）: " << colorImg.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;
    std::cout << "  empty（是否为空）: " << (colorImg.empty() ? "是" : "否") << std::endl;


    // 5. 创建全0、全1、随机矩阵
    cv::Mat zeros = cv::Mat::zeros(3, 3, CV_8UC1);     // 全0矩阵
    cv::Mat ones = cv::Mat::ones(3, 3, CV_8UC1);       // 全1矩阵
    cv::Mat random = cv::Mat(3, 3, CV_8UC1);
    cv::randu(random, 0, 256);  // 随机值 [0, 256)

    std::cout << "\n全0矩阵：\n" << zeros << std::endl;
    std::cout << "\n全1矩阵：\n" << ones << std::endl;
    std::cout << "\n随机矩阵：\n" << random << std::endl;


    std::cout << "\n=== 关键点总结 ===" << std::endl;
    std::cout << "1. cv::Mat 是OpenCV存储图像/矩阵的核心结构" << std::endl;
    std::cout << "2. BGR顺序，不是RGB！(项目中很重要)" << std::endl;
    std::cout << "3. 默认是浅拷贝，需要深拷贝用 .clone()" << std::endl;
    std::cout << "4. 用 .at<type>(row,col) 访问单个元素" << std::endl;

    return 0;
}
