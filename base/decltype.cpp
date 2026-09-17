// ============================================================================
// decltype：未加括号看「声明类型」，加括号看「表达式值类别」
//
// 规则1  decltype(id)     id 是未加括号的名字 → 得到声明时的类型（含引用和 const）
// 规则2  decltype((expr)) 当成表达式：
//          左值 → T&     将亡值 → T&&     纯右值 → T
//
// 对照：
//   x 是 int            decltype(x)  → int      decltype((x)) → int&
//   y 是 int&           decltype(y)  → int&     decltype((y)) → int&
//   z 是 const int&     decltype(z)  → const int&
//                       decltype((z)) → const int&   // 左值，保留 const
//   42 纯右值           decltype(42) / decltype((42)) → int
//   std::move(x) 将亡值 decltype(std::move(x)) → int&&
//   ++x 返回左值        decltype(++x) → int&
//
// 编译: g++ -std=c++17 -Wall decltype.cpp -o decltype && ./decltype
// ============================================================================
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
