#include <iostream>

int main() {
    int x = 42;
    int& y = x;
    const int& z = x;

    // 规则1：未加括号的标识符
    std::cout << (std::is_same_v<decltype(x), int>) << std::endl;           // ✅
    std::cout << (std::is_same_v<decltype(y), int&>) << std::endl;          // ✅
    std::cout << (std::is_same_v<decltype(z), int&>) << std::endl;          // ✅

    // 规则2：加括号的表达式
    std::cout << (std::is_same_v<decltype((x)), int&>) << std::endl;        // ✅
    std::cout << (std::is_same_v<decltype((y)), int&>) << std::endl;        // ✅
    std::cout << (std::is_same_v<decltype((z)), const int&>) << std::endl;  // ✅

    // 纯右值和将亡值
    std::cout << (std::is_same_v<decltype(42), int>) << std::endl;          // ✅
    std::cout << (std::is_same_v<decltype((42)), int>) << std::endl;        // ✅

    std::cout << std::is_same_v<decltype(std::move(x)), int&&> << std::endl;  // true
    std::cout << std::is_same_v<decltype(static_cast<int&&>(x)), int&&> << std::endl;  // true

    std::cout << std::is_same_v<decltype(++x), int&> << std::endl;
    std::cout << x << std::endl;
}
