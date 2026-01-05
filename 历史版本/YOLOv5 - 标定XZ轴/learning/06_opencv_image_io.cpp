/*
 * 06_opencv_image_io.cpp
 *
 * 学习目标：
 * 1. 读取图像文件（imread）
 * 2. 显示图像（imshow, waitKey）
 * 3. 保存图像（imwrite）
 * 4. 理解图像窗口操作
 */

#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::cout << "=== OpenCV 图像读取、显示、保存 ===" << std::endl;

    // 1. 创建一个简单的测试图像（因为我们可能没有真实图片文件）
    // 创建一个 300x400 的彩色图像（BGR）
    cv::Mat testImage(300, 400, CV_8UC3);

    // 绘制一些内容
    testImage.setTo(cv::Scalar(200, 200, 200));  // 灰色背景 (BGR都是200)

    // 画一个红色矩形
    cv::rectangle(testImage,
                  cv::Point(50, 50),      // 左上角
                  cv::Point(350, 250),    // 右下角
                  cv::Scalar(0, 0, 255),  // 红色 (BGR)
                  3);                      // 线宽

    // 画一个绿色圆形
    cv::circle(testImage,
               cv::Point(200, 150),        // 圆心
               50,                         // 半径
               cv::Scalar(0, 255, 0),      // 绿色
               -1);                        // -1表示填充

    // 添加文字
    cv::putText(testImage,
                "OpenCV Test",              // 文本内容
                cv::Point(120, 180),        // 位置
                cv::FONT_HERSHEY_SIMPLEX,   // 字体
                1.0,                        // 字体大小
                cv::Scalar(255, 255, 255),  // 白色
                2);                         // 粗细

    std::cout << "创建测试图像: " << testImage.cols << "x" << testImage.rows
              << ", " << testImage.channels() << "通道" << std::endl;


    // 2. 保存图像到文件
    std::string filename = "/home/chris/Desktop/YOLOv5 - 标定XZ轴/learning/test_image.jpg";
    bool success = cv::imwrite(filename, testImage);

    if(success) {
        std::cout << "图像已保存到: " << filename << std::endl;
    } else {
        std::cout << "保存失败！" << std::endl;
    }


    // 3. 从文件读取图像
    cv::Mat loadedImage = cv::imread(filename);

    if(loadedImage.empty()) {
        std::cout << "错误：无法读取图像文件！" << std::endl;
    } else {
        std::cout << "成功读取图像: " << loadedImage.cols << "x" << loadedImage.rows << std::endl;
    }


    // 4. 显示图像（需要GUI环境）
    std::cout << "\n尝试显示图像窗口..." << std::endl;
    std::cout << "（如果你在SSH或无GUI环境中，会看到错误信息，这是正常的）" << std::endl;

    try {
        // 创建窗口
        cv::namedWindow("Test Image", cv::WINDOW_NORMAL);  // WINDOW_NORMAL = 可调整大小

        // 显示图像
        cv::imshow("Test Image", testImage);

        std::cout << "按任意键关闭窗口..." << std::endl;
        cv::waitKey(0);  // 等待按键，0表示无限等待

        // 关闭所有窗口
        cv::destroyAllWindows();

    } catch(const cv::Exception& e) {
        std::cout << "无法显示窗口（可能是headless环境）: " << e.what() << std::endl;
    }


    std::cout << "\n=== 项目中的实际用法示例 ===" << std::endl;
    std::cout << "在 get_pose_indemind_left.cpp 中：" << std::endl;
    std::cout << "  - 使用 slam.GetImageDataLeft() 获取摄像头图像" << std::endl;
    std::cout << "  - 图像已经是 cv::Mat 格式" << std::endl;
    std::cout << "  - 检查 img.empty() 确保图像有效" << std::endl;
    std::cout << "  - 使用 cv::imshow() 显示检测结果" << std::endl;


    std::cout << "\n=== 关键点总结 ===" << std::endl;
    std::cout << "1. imread()  - 读取图像文件" << std::endl;
    std::cout << "2. imwrite() - 保存图像到文件" << std::endl;
    std::cout << "3. imshow()  - 显示图像（需要GUI）" << std::endl;
    std::cout << "4. waitKey() - 等待按键，参数为等待时间(ms)，0=无限等待" << std::endl;
    std::cout << "5. 总是检查 .empty() 确保图像读取成功！" << std::endl;

    return 0;
}
