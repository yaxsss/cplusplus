#include <iostream>
#include <string>
using namespace std;

class Case1 {
public:
    static const string description;
};
const string Case1::description = "无";

class Case2 {
public:
    Case2() {}
    static const string description;
};
const string Case2::description = "默认构造函数";

class Case3 {
public:
    Case3(int a) {}
    static const string description;
};
const string Case3::description = "带参数构造函数";

class Case4 {
public:
    Case4(const Case4& other) {}
    static const string description;
};
const string Case4::description = "拷贝构造函数";

class Case5 {
public:
    Case5(Case5&& other) {}
    static const string description;
};
const string Case5::description = "移动构造函数";

class Case6 {
public:
    Case6 operator=(const Case6& other) {}
    static const string description;
};
const string Case6::description = "拷贝赋值运算符";

class Case7 {
public:
    Case7 operator=(Case7&& other) {}
    static const string description;
};
const string Case7::description = "移动赋值运算符";

class Case8 {
public:
    ~Case8() {}
    static const string description;
};
const string Case8::description = "析构函数";


class Case9 {
public:
    Case9() = default;
    static const string description;
};
const string Case9::description = "默认构造函数";

class Case10{
public:
    Case10(const Case10&) = default;
    static const string description;
};
const string Case10::description = "默认拷贝构造函数";

class Case11{
public:
    Case11(Case11&&) = default;
    static const string description;
};
const string Case11::description = "默认移动构造函数";

class Case12 {
public:
    Case12& operator=(const Case12&) = default;
    static const string description;
};
const string Case12::description = "默认拷贝赋值运算符";

class Case13 {
public:
    Case13& operator=(Case13&&) = default;
    static const string description;    
};
const string Case13::description = "默认移动赋值运算符";

class Case14 {
public:
    ~Case14() = default;
    static const string description;
};
const string Case14::description = "默认析构函数";

template<typename T>
void print_default_function() {
    cout << boolalpha;
    cout << "描述: " << T::description << endl;
    cout << "默认构造函数: 是否存在: " << is_default_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_constructible_v<T> << endl;
    cout << "拷贝构造函数: 是否存在: " << is_copy_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_copy_constructible_v<T> << endl;
    cout << "移动构造函数: 是否存在: " << is_move_constructible_v<T> << ", 是否是平凡构造: " << is_trivially_move_constructible_v<T> << endl;
    cout << "拷贝赋值运算符: 是否存在: " << is_copy_assignable_v<T> << ", 是否是平凡赋值: " << is_trivially_copy_assignable_v<T> << endl;
    cout << "移动赋值运算符: 是否存在: " << is_move_assignable_v<T> << ", 是否是平凡赋值: " << is_trivially_move_assignable_v<T> << endl;
    cout << "析构函数: 是否存在: " << is_destructible_v<T> << ", 是否是平凡析构: " << is_trivially_destructible_v<T> << endl;
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