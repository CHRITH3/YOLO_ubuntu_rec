// ============================================
// 示例3: STL容器 (C++的数据结构库)
// ============================================

#include <iostream>
#include <vector>
#include <queue>
#include <string>

// ===== 1. vector - 动态数组 (最常用!) =====
void demo_vector() {
    std::cout << "\n===== std::vector (动态数组) =====" << std::endl;

    // 在C语言中，数组大小固定:
    // int arr[10];  // 只能存10个元素

    // C++的vector可以动态增长:
    std::vector<int> scores;  // 空vector

    // 添加元素
    scores.push_back(85);
    scores.push_back(90);
    scores.push_back(78);
    std::cout << "添加3个元素后，size = " << scores.size() << std::endl;

    // 访问元素 (和C数组一样)
    std::cout << "第一个分数: " << scores[0] << std::endl;
    std::cout << "最后一个分数: " << scores[scores.size()-1] << std::endl;

    // 遍历
    std::cout << "所有分数: ";
    for (int i = 0; i < scores.size(); i++) {
        std::cout << scores[i] << " ";
    }
    std::cout << std::endl;

    // C++11 范围for循环 (更简洁)
    std::cout << "所有分数(新方法): ";
    for (int score : scores) {  // 自动遍历
        std::cout << score << " ";
    }
    std::cout << std::endl;

    // 检查是否为空
    if (!scores.empty()) {
        std::cout << "Vector不为空" << std::endl;
    }

    // 清空
    scores.clear();
    std::cout << "清空后 size = " << scores.size() << std::endl;
}

// ===== 2. 项目中的实际用法 =====
// 来自 yolo_pose_detector.h:52
struct KeyPoint {
    float x, y, confidence;
};

struct PoseResult {
    std::vector<KeyPoint> keypoints;  // 17个关键点的动态数组

    PoseResult() {
        keypoints.resize(17);  // 预先分配17个空间
    }
};

void demo_project_vector() {
    std::cout << "\n===== 项目实例: 检测结果 =====" << std::endl;

    std::vector<PoseResult> all_poses;  // 可以存储多个人的检测结果

    // 检测到3个人
    PoseResult person1, person2, person3;
    all_poses.push_back(person1);
    all_poses.push_back(person2);
    all_poses.push_back(person3);

    std::cout << "检测到 " << all_poses.size() << " 个人" << std::endl;

    // 访问第一个人的数据
    std::cout << "第一个人有 " << all_poses[0].keypoints.size()
              << " 个关键点" << std::endl;
}

// ===== 3. queue - 队列 (先进先出) =====
void demo_queue() {
    std::cout << "\n===== std::queue (队列) =====" << std::endl;

    // 项目中用于缓存图像帧
    std::queue<int> frame_queue;

    // 入队 (相机采集的帧)
    frame_queue.push(1);  // 第1帧
    frame_queue.push(2);  // 第2帧
    frame_queue.push(3);  // 第3帧

    std::cout << "队列中有 " << frame_queue.size() << " 帧" << std::endl;

    // 出队 (处理帧)
    while (!frame_queue.empty()) {
        int frame = frame_queue.front();  // 获取队首
        std::cout << "处理第 " << frame << " 帧" << std::endl;
        frame_queue.pop();  // 移除队首
    }

    std::cout << "处理完毕，队列为空" << std::endl;
}

// ===== 4. string - 字符串 (比C的char*好用太多!) =====
void demo_string() {
    std::cout << "\n===== std::string (字符串) =====" << std::endl;

    // C语言方式 (麻烦):
    // char path[100];
    // strcpy(path, "models/");
    // strcat(path, "yolo.onnx");

    // C++方式 (简单):
    std::string model_dir = "models/";
    std::string model_name = "yolo.onnx";
    std::string full_path = model_dir + model_name;  // 直接用+拼接!

    std::cout << "完整路径: " << full_path << std::endl;

    // 其他常用操作
    std::cout << "路径长度: " << full_path.length() << std::endl;
    std::cout << "是否包含'yolo': " << (full_path.find("yolo") != std::string::npos) << std::endl;

    // 从C字符串转换
    const char* c_str = "Hello";
    std::string cpp_str(c_str);
    std::cout << "C++字符串: " << cpp_str << std::endl;

    // 转回C字符串 (有些老API需要)
    const char* back_to_c = cpp_str.c_str();
    std::cout << "C字符串: " << back_to_c << std::endl;
}

int main() {
    demo_vector();
    demo_project_vector();
    demo_queue();
    demo_string();

    return 0;
}

/*
关键理解:

1. std::vector<T> - 动态数组
   - push_back(x): 末尾添加
   - size(): 元素个数
   - [i]: 访问第i个元素
   - clear(): 清空
   - resize(n): 设置大小为n

2. std::queue<T> - 队列
   - push(x): 入队
   - front(): 获取队首
   - pop(): 出队
   - empty(): 是否为空

3. std::string - 字符串
   - 用+拼接
   - .c_str()转C字符串
   - 不需要手动管理内存!

4. 项目中的使用:
   - std::vector<PoseResult> poses;  // 存储多人检测结果
   - std::queue<cv::Mat> image_queue;  // 图像缓冲队列
   - std::string model_path;  // 模型路径

记住: STL容器会自动管理内存，不需要malloc/free!
*/
