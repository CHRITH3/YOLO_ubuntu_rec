// ============================================
// 示例4: 命名空间和智能指针
// ============================================

#include <iostream>
#include <memory>  // 智能指针
#include <vector>  // vector容器

// ===== 1. 命名空间 (防止命名冲突) =====
namespace Company_A {
    void processImage() {
        std::cout << "A公司的图像处理算法" << std::endl;
    }
}

namespace Company_B {
    void processImage() {  // 同名函数不冲突!
        std::cout << "B公司的图像处理算法" << std::endl;
    }
}

// 项目中的命名空间
// 来自 get_pose_indemind_left.cpp:23
// using namespace indem;  // INDEMIND相机SDK的命名空间

void demo_namespace() {
    std::cout << "\n===== 命名空间 =====" << std::endl;

    // 需要指明使用哪个命名空间的函数
    Company_A::processImage();
    Company_B::processImage();

    // std::cout 中的 std 就是标准库的命名空间
    // std::string, std::vector 都在 std 命名空间中
}

// ===== 2. 智能指针 (自动管理内存) =====

class Camera {
public:
    Camera() {
        std::cout << "相机已连接" << std::endl;
    }

    ~Camera() {
        std::cout << "相机已断开" << std::endl;
    }

    void capture() {
        std::cout << "拍照!" << std::endl;
    }
};

// C语言方式 (容易内存泄漏)
void old_way_with_memory_leak() {
    std::cout << "\n===== 传统指针 (容易出错) =====" << std::endl;

    Camera* cam = new Camera();  // 动态分配内存
    cam->capture();

    // 如果忘记delete，内存泄漏!
    // 如果函数中途return，也会泄漏!
    delete cam;  // 必须手动释放
}

// C++方式 (自动管理，不会泄漏)
void modern_way_with_smart_pointer() {
    std::cout << "\n===== 智能指针 (自动释放) =====" << std::endl;

    {  // 作用域开始
        std::unique_ptr<Camera> cam(new Camera());  // 智能指针
        cam->capture();

        // 或者更现代的写法:
        // auto cam = std::make_unique<Camera>();

    }  // 作用域结束，自动调用delete！
    std::cout << "作用域结束，相机自动断开" << std::endl;
}

// 项目中的实际用法
// 来自 yolo_pose_detector.h:155
class YOLODetector_Example {
private:
    std::unique_ptr<int> session_;  // ONNX Runtime会话

public:
    void Init() {
        // 创建会话
        session_ = std::make_unique<int>(42);
        std::cout << "ONNX会话创建成功" << std::endl;
    }

    ~YOLODetector_Example() {
        // 不需要手动delete session_
        // unique_ptr析构时自动释放
        std::cout << "YOLODetector析构，session_自动释放" << std::endl;
    }
};

// ===== 3. auto 关键字 (自动类型推导) =====
void demo_auto() {
    std::cout << "\n===== auto 关键字 =====" << std::endl;

    // 传统方式 (类型名太长!)
    std::vector<int>::iterator it1 = std::vector<int>().begin();

    // 使用auto (编译器自动推导类型)
    auto it2 = std::vector<int>().begin();

    // 项目中的用法
    auto detector = std::make_unique<YOLODetector_Example>();
    detector->Init();

    // 循环中使用
    std::vector<int> nums = {1, 2, 3, 4, 5};
    for (auto num : nums) {  // auto自动推导为int
        std::cout << num << " ";
    }
    std::cout << std::endl;
}

int main() {
    demo_namespace();
    old_way_with_memory_leak();
    modern_way_with_smart_pointer();
    demo_auto();

    return 0;
}

/*
关键理解:

1. 命名空间 (namespace)
   - 避免命名冲突
   - std::cout 中 std 是标准库命名空间
   - using namespace std; 可以省略std::，但不推荐
   - 项目中: using namespace indem;

2. 智能指针 (Smart Pointer)
   - std::unique_ptr: 独占所有权，自动释放
   - 作用域结束自动delete
   - 不会内存泄漏
   - 项目中用于管理ONNX Runtime会话

3. auto 关键字
   - 让编译器自动推导类型
   - 简化代码
   - 项目中常用于迭代器、lambda等

4. 为什么用智能指针？
   - 自动内存管理
   - 异常安全
   - 代码更简洁
   - 不需要配对new/delete

记住:
- 优先用智能指针，不用new/delete
- 看到std::就是标准库
- auto让代码更简洁
*/
