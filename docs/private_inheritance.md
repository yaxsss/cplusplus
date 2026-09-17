私有继承
私有继承的主要作用：

1. 实现“组合”而非"继承", 原因是因为阻止了派生类转换为基类对象。还是可以调用基类的public和protected方法。
```cpp
Derived derived;
Base* base = &derived; // 错误：私有继承阻止隐私转换
```

2. 复用基类的实现, 是实现继承，而不是接口继承
与公有继承的区别：
* 公有继承，派生类是基类的一种。
* 私有继承，implemented-in-terms-of（用..实现），派生类复用基类的实现。

3. 实现“非虚借口（NVI）“模式
通过将虚函数设为private或protected，并在公有接口中调用虚函数，来控制派生类的行为。
私有继承可以与 NVI 模式结合使用，进一步隐藏基类的实现细节。
```cpp
class Base {
public:
    void execute() {
        doExecute(); // 调用私有虚函数
    }

private:
    virtual void doExecute() {
        std::cout << "Base::doExecute" << std::endl;
    }
};

class Derived : private Base {
private:
    void doExecute() override {
        std::cout << "Derived::doExecute" << std::endl;
    }

public:
    void callExecute() {
        execute(); // 调用基类的 NVI 接口
    }
};
```
