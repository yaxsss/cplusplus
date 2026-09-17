# 经典 GoF Observer：两个麻烦

对应源码：[`concurrent/observer/observer_classic.cpp`](../concurrent/observer/observer_classic.cpp)

```sh
# 编译运行
cl /EHsc /std:c++17 /utf-8 concurrent/observer/observer_classic.cpp
# 或
g++ -std=c++17 -Wall concurrent/observer/observer_classic.cpp -o classic && ./classic
```

经典 Observer 用**继承接口**表达订阅关系：Subject 只认识一个抽象基类，事件一到就调虚函数。这套模型有两个结构性麻烦。

---

## 麻烦一：每种事件都是一个接口类型

`Clock` 只接受 `IClockObserver*`，`TemperatureSensor` 只接受 `ITemperatureObserver*`。`Dashboard` 想同时听时间和温度，就只能多继承：

```cpp
class Dashboard : public IClockObserver, public ITemperatureObserver { ... };
```

代价：

- 继承是稀缺资源：同一个基类只能继承一次
- 关系在编译期焊死，运行时加不了也拆不掉
- 每多一种事件，就多一个基类、一份虚函数表槽

```text
Clock.Tick()                 -> dash.OnTimeChanged()
TemperatureSensor.Sample()   -> dash.OnTemperatureChanged()
```

两种**不同类型**的事件，多继承还能撑住。真正崩的是下面这种情况。

---

## 麻烦二：同一种事件要订两次

需求很平常：

- 1 秒定时器响 → 心跳 `OnHeartbeat()`
- 30 秒定时器响 → 自检 `OnSelfCheck()`

但接口只有一个无参虚函数：

```cpp
class ITimerObserver {
    virtual void OnTimer() = 0;  // 回调里无从知道是谁触发的
};
```

一个类对 `ITimerObserver` 只能继承一次。两个 `Timer` 实例触发的都是同一个 `OnTimer()`，`SystemMonitor` **没法直接同时挂到两个 Timer 上**。

```text
Timer 1s  --OnTimer()-->  SystemMonitor   // 分不清该心跳还是自检
Timer 30s --OnTimer()-->  SystemMonitor
```

后面三段都是绕这个坑，不是在推荐生产写法。

---

## Workaround A：转发壳类（trampoline / sink）

不把 `SystemMonitor` 自己挂上去，而是养两个小观察者，每个只转发到一个业务函数：

```cpp
class HeartbeatSink : public ITimerObserver {
    SystemMonitor* owner_;
    void OnTimer() override;   // -> owner_->OnHeartbeat()
};

class SelfCheckSink : public ITimerObserver {
    SystemMonitor* owner_;
    void OnTimer() override;   // -> owner_->OnSelfCheck()
};

class SystemMonitor {
    HeartbeatSink heartbeat_{this};
    SelfCheckSink selfcheck_{this};
};
```

`main` 里挂的是壳，不是 `m` 本身：

```cpp
t1s.Attach(&m.heartbeat_);
t30s.Attach(&m.selfcheck_);
```

调用链：

```text
t1s.Fire()  → heartbeat_.OnTimer()  → m.OnHeartbeat()
t30s.Fire() → selfcheck_.OnTimer()  → m.OnSelfCheck()
```

身份信息藏在「挂了哪个对象」里，所以回调可以没有参数。

代价：每多一条订阅关系，就要再手写一个小类，样板代码爆炸。

---

## Workaround B：sender + if/else 分派

只挂一个观察者，让定时器自己报名：

```cpp
class TimerWithSender {
    std::string name_;
    void Fire() { for (auto* o : obs_) o->OnTimer(name_); }
};

class MonitorDispatcher : public ITimerObserverWithSender {
    void OnTimer(const std::string& source) override {
        if (source == m_.heartbeatSource_)      m_.OnHeartbeat();
        else if (source == m_.selfCheckSource_) m_.OnSelfCheck();
    }
};
```

调用链：

```text
w1s.Fire()  → disp.OnTimer("1s")  → OnHeartbeat()
w30s.Fire() → disp.OnTimer("30s") → OnSelfCheck()
```

壳类不用写了，但「谁触发的」泄漏进了业务逻辑：

- 每加一个定时器，都要回来改 `if/else`（违反开闭原则）
- 字符串写错会静默分派失败
- Subject 的实现细节渗进 Observer

---

## 彩蛋：用模板复制接口也救不了

有人会想：把接口做成 `ITimerObserverT<1>` 和 `ITimerObserverT<2>`，类型不同，就可以同时继承，各写一份 `OnTimer()`。

C++ **不按「来自哪个基类」区分虚函数，只看名字 + 参数**。两个基类的 `OnTimer()` 签名一样，会被合并成一个槽：

```cpp
class TaggedMonitor : public ITimerObserverT<1>, public ITimerObserverT<2> {
    void OnTimer() override;  // 同时覆盖两个基类，无法分别实现
};
```

`main` 里两次 `static_cast` 打出来的是同一句话，就是在证明这一点。

---

## 对照

| 方案 | 怎么区分 1s / 30s | 代价 |
|------|-------------------|------|
| 直接继承 `ITimerObserver` | 分不了 | 一个类只能占一个槽 |
| A 壳类 | 两个不同对象 | 每条订阅一个小类 |
| B sender 字符串 | `if (source == ...)` | 运行时分派，改一处漏一处 |
| 模板 Tag | 以为是两个类型 | 同签名虚函数被合并 |

现代做法一般是 `std::function` / 信号槽：同一对象可以对同一事件注册两个不同 lambda，根本不需要继承接口。这份示例的目的，是把「为什么要换掉继承式 Observer」讲清楚。

两份现代实现的对比见 [`signal_slot.md`](signal_slot.md)。
