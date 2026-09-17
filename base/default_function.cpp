// ============================================================================
// 编译器何时合成六大特殊成员函数，以及「手写」和「= default」的差别
//
// 规则（C++11）：
//   - 默认构造：只要声明了任意构造函数，就不再隐式生成
//   - 拷贝构造/拷贝赋值：声明了移动构造或移动赋值，则被删除
//   - 移动构造/移动赋值：一旦声明了拷贝、移动或析构中的任意一个，就不再生成
//   - 析构：未声明则隐式生成
//   - 用户提供的函数体（即便是空的）不是 trivial；= default 才可能是 trivial
//
// Case1      什么都不写 → 六个都会生成，且都 trivial
// Case2–8    各手写一个特殊成员（空函数体）→ 看其余五个还在不在、还是否 trivial
// Case9–14   对应的 = default → 函数在，且对空类仍是 trivial
//
// 编译: g++ -std=c++17 -Wall default_function.cpp && ./a.out
// ============================================================================
#include <iostream>
#include <string>
using namespace std;

class Case1 {
public:
    static const string description;
};
const string Case1::description = "case1:什么都不写";

class Case2 {
public:
    Case2() {}
    static const string description;
};
const string Case2::description = "case2:默认构造函数";

class Case3 {
public:
    Case3(int a) {}
    static const string description;
};
const string Case3::description = "case3:带参数构造函数";

class Case4 {
public:
    Case4(const Case4& other) {}
    static const string description;
};
const string Case4::description = "case4:拷贝构造函数";

class Case5 {
public:
    Case5(Case5&& other) {}
    static const string description;
};
const string Case5::description = "case5:移动构造函数";

class Case6 {
public:
    Case6 operator=(const Case6& other) {}
    static const string description;
};
const string Case6::description = "case6:拷贝赋值运算符";

class Case7 {
public:
    Case7 operator=(Case7&& other) {}
    static const string description;
};
const string Case7::description = "case7:移动赋值运算符";

class Case8 {
public:
    ~Case8() {}
    static const string description;
};
const string Case8::description = "case8:析构函数";


class Case9 {
public:
    Case9() = default;
    static const string description;
};
const string Case9::description = "case9:默认构造函数";

class Case10{
public:
    Case10(const Case10&) = default;
    static const string description;
};
const string Case10::description = "case10:默认拷贝构造函数";

class Case11{
public:
    Case11(Case11&&) = default;
    static const string description;
};
const string Case11::description = "case11:默认移动构造函数";

class Case12 {
public:
    Case12& operator=(const Case12&) = default;
    static const string description;
};
const string Case12::description = "case12:默认拷贝赋值运算符";

class Case13 {
public:
    Case13& operator=(Case13&&) = default;
    static const string description;    
};
const string Case13::description = "case13:默认移动赋值运算符";

class Case14 {
public:
    ~Case14() = default;
    static const string description;
};
const string Case14::description = "case14:默认析构函数";

template<typename T>
void print_default_function() {
    cout << boolalpha;
    cout << "===================" << T::description << "===================" << endl;
    cout << "默认构造函数: 是否存在: " << is_default_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_constructible_v<T> << endl;
    cout << "拷贝构造函数: 是否存在: " << is_copy_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_copy_constructible_v<T> << endl;
    cout << "移动构造函数: 是否存在: " << is_move_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_move_constructible_v<T> << endl;
    cout << "拷贝赋值运算符: 是否存在: " << is_copy_assignable_v<T> << ", 是否是平凡赋值: " << is_trivially_copy_assignable_v<T> << endl;
    cout << "移动赋值运算符: 是否存在: " << is_move_assignable_v<T> << ", 是否是平凡赋值: " << is_trivially_move_assignable_v<T> << endl;
    cout << "析构函数: 是否存在: " << is_destructible_v<T> << ", 是否是平凡析构: " << is_trivially_destructible_v<T> << endl;
    cout << "是否是聚合类型: " << is_aggregate_v<T> << endl;
    cout << "是否是标准布局类型: " << is_standard_layout_v<T> << endl;
    cout << "是否是POD类型: " << is_pod_v<T> << endl;
    cout << "是否是Trivial类型: " << is_trivial_v<T> << endl;
}

int main() {
    print_default_function<Case1>();
    print_default_function<Case2>();
    print_default_function<Case3>();
    print_default_function<Case4>();
    print_default_function<Case5>();
    print_default_function<Case6>();
    print_default_function<Case7>();
    print_default_function<Case8>();
    print_default_function<Case9>();
    print_default_function<Case10>();
    print_default_function<Case11>();
    print_default_function<Case12>();   
    print_default_function<Case13>();
    print_default_function<Case14>();
}