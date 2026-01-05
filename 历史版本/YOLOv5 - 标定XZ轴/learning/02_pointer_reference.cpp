// ============================================
// 示例2: 指针和引用 (从单片机C过渡到C++)
// ============================================

#include <iostream>
#include <vector>

// ===== 单片机C语言中你熟悉的指针 =====
void updateValue_pointer(int* ptr) {
    *ptr = 100;  // 通过指针修改原变量
}

// ===== C++中的引用 (更安全、更方便) =====
void updateValue_reference(int& ref) {
    ref = 200;  // 直接修改，不需要*号
}

// ===== 项目中的实际例子(伪代码说明) =====
// 来自 pose_utils.cpp:44
// void MapPoseTo3D(
//     std::vector<PoseResult>& poses,  // 引用传递，会修改原数据
//     const cv::Mat& depth,            // const引用：只读，不修改
//     const cv::Mat& camera_matrix) {
//
//     // 函数内部直接操作poses，外部变量会被修改
//     // const表示depth和camera_matrix只读
// }

class Rectangle {
public:
    int width;
    int height;

    Rectangle(int w, int h) : width(w), height(h) {}

    int area() {
        return width * height;
    }
};

// 按值传递: 会复制整个对象（慢，浪费内存）
int getArea_byValue(Rectangle rect) {
    return rect.area();
}

// 按引用传递: 不复制，直接操作原对象（快）
int getArea_byReference(Rectangle& rect) {
    return rect.area();
}

// 按const引用传递: 既快，又安全（不会被修改）
int getArea_byConstReference(const Rectangle& rect) {
    // rect.width = 10;  // 错误！const不能修改
    return rect.width * rect.height;  // 可以读取
}

int main() {
    std::cout << "\n===== 指针 vs 引用 =====" << std::endl;

    // 指针方式
    int value1 = 10;
    int* ptr = &value1;  // 取地址
    std::cout << "原值: " << value1 << std::endl;
    updateValue_pointer(ptr);
    std::cout << "指针修改后: " << value1 << std::endl;

    // 引用方式
    int value2 = 10;
    std::cout << "原值: " << value2 << std::endl;
    updateValue_reference(value2);  // 不需要&，自动传引用
    std::cout << "引用修改后: " << value2 << std::endl;

    std::cout << "\n===== 传递方式对比 =====" << std::endl;
    Rectangle rect(10, 20);

    std::cout << "按值传递: " << getArea_byValue(rect) << std::endl;
    std::cout << "按引用传递: " << getArea_byReference(rect) << std::endl;
    std::cout << "按const引用: " << getArea_byConstReference(rect) << std::endl;

    return 0;
}

/*
关键理解:

1. 指针 (你熟悉的):
   int* ptr = &value;  // 取地址
   *ptr = 100;         // 解引用修改

2. 引用 (C++新增):
   int& ref = value;   // 创建引用
   ref = 100;          // 直接修改，不需要*

3. const引用 (项目常用):
   const cv::Mat& depth  // 只读，不复制，高效

4. 为什么用引用？
   - 避免复制大对象（图像数据很大！）
   - 代码更简洁
   - 编译器检查，更安全

5. 项目中的使用场景:
   - 函数参数用const引用：不修改，高效
   - 函数参数用引用：需要修改原数据
   - 返回值用引用：避免复制
*/
