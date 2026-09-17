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