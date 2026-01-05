/*
 * 08_opencv_drawing.cpp
 *
 * 学习目标：
 * 1. 绘制矩形、圆形、线条
 * 2. 绘制关键点（YOLO pose的17个关键点）
 * 3. 绘制骨架连接线
 * 4. 添加文字标注
 *
 * 这些都是项目中用来可视化检测结果的核心功能！
 */

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== OpenCV 绘图功能 ===" << std::endl;

    // 创建一个空白画布
    cv::Mat canvas(600, 800, CV_8UC3, cv::Scalar(255, 255, 255));  // 白色背景

    // ========== 1. 绘制矩形 ==========
    std::cout << "\n--- 绘制矩形 ---" << std::endl;

    // 方法1：左上角+右下角
    cv::rectangle(canvas,
                  cv::Point(50, 50),      // 左上角 (x, y)
                  cv::Point(200, 150),    // 右下角
                  cv::Scalar(0, 0, 255),  // 红色 BGR
                  2);                      // 线宽，-1表示填充

    // 方法2：使用cv::Rect
    cv::Rect bbox(250, 50, 150, 100);  // x, y, width, height
    cv::rectangle(canvas, bbox, cv::Scalar(0, 255, 0), 3);  // 绿色

    std::cout << "绘制了2个矩形（红色和绿色）" << std::endl;


    // ========== 2. 绘制圆形（用于关键点） ==========
    std::cout << "\n--- 绘制圆形（关键点） ---" << std::endl;

    // 在项目中，每个关键点都是一个小圆圈
    cv::circle(canvas,
               cv::Point(100, 250),         // 圆心
               5,                           // 半径
               cv::Scalar(255, 0, 0),       // 蓝色
               -1);                         // -1 = 填充

    cv::circle(canvas, cv::Point(150, 250), 5, cv::Scalar(255, 0, 0), -1);
    cv::circle(canvas, cv::Point(200, 280), 5, cv::Scalar(255, 0, 0), -1);

    std::cout << "绘制了3个关键点（蓝色圆点）" << std::endl;


    // ========== 3. 绘制线条（骨架连接） ==========
    std::cout << "\n--- 绘制线条（骨架） ---" << std::endl;

    // 连接关键点形成骨架
    cv::line(canvas,
             cv::Point(100, 250),       // 起点
             cv::Point(150, 250),       // 终点
             cv::Scalar(0, 255, 255),   // 黄色
             2);                        // 线宽

    cv::line(canvas, cv::Point(150, 250), cv::Point(200, 280),
             cv::Scalar(0, 255, 255), 2);

    std::cout << "绘制了骨架连接线（黄色）" << std::endl;


    // ========== 4. 添加文字 ==========
    std::cout << "\n--- 添加文字标注 ---" << std::endl;

    cv::putText(canvas,
                "Person: 0.95",              // 文本内容
                cv::Point(50, 30),           // 位置（左下角）
                cv::FONT_HERSHEY_SIMPLEX,    // 字体
                0.7,                         // 字体大小
                cv::Scalar(255, 0, 0),       // 蓝色
                2);                          // 粗细

    std::cout << "添加了置信度标注文字" << std::endl;


    // ========== 5. 模拟项目中的完整可视化 ==========
    std::cout << "\n=== 模拟YOLO Pose检测结果可视化 ===" << std::endl;

    // 创建新画布
    cv::Mat result(480, 640, CV_8UC3, cv::Scalar(50, 50, 50));  // 深灰色背景

    // 模拟检测到的人体框
    cv::Rect person_box(200, 100, 240, 350);
    cv::rectangle(result, person_box, cv::Scalar(0, 255, 0), 2);

    // 添加置信度标签
    cv::putText(result, "Person: 0.89",
                cv::Point(person_box.x, person_box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

    // 模拟17个COCO关键点（简化版，只画几个重要的）
    std::vector<cv::Point> keypoints = {
        cv::Point(320, 150),  // 0: 鼻子
        cv::Point(310, 140),  // 1: 左眼
        cv::Point(330, 140),  // 2: 右眼
        cv::Point(300, 150),  // 3: 左耳
        cv::Point(340, 150),  // 4: 右耳
        cv::Point(280, 220),  // 5: 左肩
        cv::Point(360, 220),  // 6: 右肩
        cv::Point(260, 300),  // 7: 左肘
        cv::Point(380, 300),  // 8: 右肘
        cv::Point(250, 350),  // 9: 左腕
        cv::Point(390, 350),  // 10: 右腕
    };

    // 绘制关键点
    for(const auto& pt : keypoints) {
        cv::circle(result, pt, 5, cv::Scalar(0, 0, 255), -1);  // 红色关键点
    }

    // 绘制骨架连接（COCO的标准骨架定义）
    std::vector<std::pair<int, int>> skeleton = {
        {0, 1}, {0, 2},     // 鼻子-眼睛
        {1, 3}, {2, 4},     // 眼睛-耳朵
        {5, 6},             // 左肩-右肩
        {5, 7}, {7, 9},     // 左臂
        {6, 8}, {8, 10},    // 右臂
    };

    for(const auto& bone : skeleton) {
        if(bone.first < keypoints.size() && bone.second < keypoints.size()) {
            cv::line(result, keypoints[bone.first], keypoints[bone.second],
                     cv::Scalar(0, 255, 255), 2);  // 黄色骨架线
        }
    }

    std::cout << "完成YOLO Pose结果可视化！" << std::endl;
    std::cout << "  - 绿色框: 人体检测框" << std::endl;
    std::cout << "  - 红色点: 17个关键点" << std::endl;
    std::cout << "  - 黄色线: 骨架连接" << std::endl;

    // 保存结果
    std::string output1 = "/home/chris/Desktop/YOLOv5 - 标定XZ轴/learning/drawing_demo.jpg";
    std::string output2 = "/home/chris/Desktop/YOLOv5 - 标定XZ轴/learning/pose_visualization.jpg";

    cv::imwrite(output1, canvas);
    cv::imwrite(output2, result);

    std::cout << "\n已保存可视化结果到:" << std::endl;
    std::cout << "  - " << output1 << std::endl;
    std::cout << "  - " << output2 << std::endl;


    // ========== 6. 项目中的实际代码对应 ==========
    std::cout << "\n=== 项目代码对应关系 ===" << std::endl;
    std::cout << "在 get_pose_indemind_left.cpp 中：" << std::endl;
    std::cout << "  第988行: cv::rectangle(vis_img, person.bbox, ...) - 画人体框" << std::endl;
    std::cout << "  第994行: cv::circle(vis_img, pt, 3, ...) - 画关键点" << std::endl;
    std::cout << "  第1001行: cv::line(vis_img, pt1, pt2, ...) - 画骨架" << std::endl;
    std::cout << "  第997行: cv::putText(...) - 添加文字标注" << std::endl;


    // ========== 7. 关键点总结 ==========
    std::cout << "\n=== 关键点总结 ===" << std::endl;
    std::cout << "1. cv::rectangle() - 绘制矩形（人体框）" << std::endl;
    std::cout << "2. cv::circle() - 绘制圆形（关键点）" << std::endl;
    std::cout << "3. cv::line() - 绘制线条（骨架连接）" << std::endl;
    std::cout << "4. cv::putText() - 添加文字（置信度等）" << std::endl;
    std::cout << "5. cv::Scalar(B, G, R) - 注意是BGR顺序！" << std::endl;
    std::cout << "6. 线宽-1表示填充，正数表示轮廓线宽度" << std::endl;

    return 0;
}
