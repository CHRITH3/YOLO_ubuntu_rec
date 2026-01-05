// ============================================
// 示例1: 从C的struct到C++的class
// ============================================

#include <iostream>
#include <cmath>

// ===== C语言风格 (你熟悉的方式) =====
struct Point_C {
    float x;
    float y;
};

// C语言中需要单独写函数
float calculateDistance_C(Point_C p1, Point_C p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return sqrt(dx*dx + dy*dy);
}

// ===== C++风格 (数据+函数放在一起) =====
class Point_CPP {
public:  // public表示外部可以访问
    // 数据成员 (类似C的struct成员)
    float x;
    float y;

    // 构造函数: 创建对象时自动调用
    Point_CPP() {
        x = 0;
        y = 0;
        std::cout << "Point created at (0, 0)" << std::endl;
    }

    Point_CPP(float x_, float y_) {
        x = x_;
        y = y_;
        std::cout << "Point created at (" << x << ", " << y << ")" << std::endl;
    }

    // 成员函数: 操作自己的数据
    float distanceTo(Point_CPP other) {
        float dx = other.x - x;  // 直接访问自己的x
        float dy = other.y - y;
        return sqrt(dx*dx + dy*dy);
    }

    void print() {
        std::cout << "Point(" << x << ", " << y << ")" << std::endl;
    }
};

// ===== 项目中的真实例子 =====
// 来自 yolo_pose_detector.h:37
struct KeyPoint {
    float x;              // X坐标
    float y;              // Y坐标
    float confidence;     // 置信度

    // 默认构造函数
    KeyPoint() : x(0), y(0), confidence(0) {}

    // 带参数的构造函数
    KeyPoint(float x_, float y_, float conf_)
        : x(x_), y(y_), confidence(conf_) {}
};

int main() {
    std::cout << "\n===== C语言风格 =====" << std::endl;
    Point_C p1_c = {3.0, 4.0};
    Point_C p2_c = {0.0, 0.0};
    float dist_c = calculateDistance_C(p1_c, p2_c);
    std::cout << "Distance: " << dist_c << std::endl;

    std::cout << "\n===== C++风格 =====" << std::endl;
    Point_CPP p1_cpp(3.0, 4.0);  // 自动调用构造函数
    Point_CPP p2_cpp(0.0, 0.0);
    float dist_cpp = p1_cpp.distanceTo(p2_cpp);  // 对象调用自己的方法
    std::cout << "Distance: " << dist_cpp << std::endl;

    std::cout << "\n===== 项目实例 =====" << std::endl;
    KeyPoint nose(320.5, 240.3, 0.95);  // 鼻子关键点
    std::cout << "Nose: (" << nose.x << ", " << nose.y
              << ") confidence=" << nose.confidence << std::endl;

    return 0;
}

/*
编译运行:
g++ 01_class_basics.cpp -o test1
./test1

关键理解:
1. class = 数据 + 函数的组合
2. 构造函数: 对象创建时自动执行初始化
3. 成员函数: 可以直接访问自己的数据成员
4. public/private: 控制访问权限
*/
