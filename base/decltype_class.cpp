// ============================================================================
// decltype 用在成员、下标、三元表达式上；以及返回局部对象时的构造
//
// 未加括号的成员访问：得到成员的声明类型，不随对象是不是 const 变
//   decltype(s.value) / decltype(cs.value) 都是 int（value 声明为 int）
// 函数调用、下标、三元：按表达式的值类别走
//   getRef() 返回 int&  → decltype 是 int&
//   getVal() 返回 int   → decltype 是 int（纯右值）
//   arr[0] 是左值       → decltype(arr[0]) 是 int&
//
// make_x() 返回局部 X：拷贝/移动是否发生取决于是否 NRVO。
// 本目标在 CMake 里加了 -fno-elide-constructors，用来观察构造/析构次数。
//
// 编译: g++ -std=c++17 -Wall -fno-elide-constructors decltype_class.cpp -o decltype_class
// ============================================================================
#include <iostream>
#include <vector>
#include <type_traits>

class X {
    public:
    X() {std::cout << "X constructor" << std::endl;}
    X(const X&) {std::cout << "X copy constructor" << std::endl;}
    X operator=(const X&) {std::cout << "X copy assignment" << std::endl; return *this;}
    ~X() {std::cout << "X destructor" << std::endl;}
};

X make_x() {
    X x1;
    return x1;
}

struct MyStruct {
    int value;
    int& getRef() { return value; }
    int getVal() const { return value; }
};

int main() {
    MyStruct s{42};
    const MyStruct cs{100};
    
    // 成员访问
    std::cout << std::is_same_v<decltype(s.value), int> << std::endl;      // int
    std::cout << std::is_same_v<decltype(cs.value), int> << std::endl;     // int（对象是 const）
    
    // 成员函数
    std::cout << std::is_same_v<decltype(s.getRef()), int&> << std::endl;     // int&
    std::cout << std::is_same_v<decltype(s.getVal()), int> << std::endl;     // int
    std::cout << std::is_same_v<decltype(cs.getVal()), int> << std::endl;    // int（const 方法）
    
    // 指针
    int arr[5] = {1, 2, 3, 4, 5};
    using ArrElem = decltype(arr[0]);           // int&
    using PtrType = decltype(&arr[0]);          // int*
    

    // 三元表达式
    bool flag = true;
    using TernaryType = decltype(flag ? 1 : 2); // int

    X x2 = make_x(); // 调用 make_x()，返回值类型为 X
    
    return 0;
}