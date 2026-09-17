// ============================================================================
// 经典 GoF Observer（继承式实现）—— 以及这段话里说的两个麻烦
// 编译: g++ -std=c++17 -Wall observer_classic.cpp -o classic && ./classic
// ============================================================================
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 麻烦一：每“一种”事件，就是一个抽象基类（一个接口类型）
// ---------------------------------------------------------------------------
class IClockObserver {
public:
    virtual void OnTimeChanged(int hour, int minute) = 0;
    virtual ~IClockObserver() = default;
};

class ITemperatureObserver {
public:
    virtual void OnTemperatureChanged(double celsius) = 0;
    virtual ~ITemperatureObserver() = default;
};

class Clock {  // Subject: 时钟
    std::vector<IClockObserver*> obs_;
    int hour_ = 9, minute_ = 0;

public:
    void Attach(IClockObserver* o) { obs_.push_back(o); }
    void Tick() {
        if (++minute_ == 60) { minute_ = 0; hour_ = (hour_ + 1) % 24; }
        for (auto* o : obs_) o->OnTimeChanged(hour_, minute_);
    }
};

class TemperatureSensor {  // Subject: 温度传感器
    std::vector<ITemperatureObserver*> obs_;
    double c_ = 26.0;

public:
    void Attach(ITemperatureObserver* o) { obs_.push_back(o); }
    void Sample() {
        c_ += 0.5;
        for (auto* o : obs_) o->OnTemperatureChanged(c_);
    }
};

// >>> 代价：想同时关心“时间”和“温度”，就只能 multi-inheritance。
//     继承是稀缺资源：一个类对“同一个基类”只能继承一次，而且继承关系
//     在编译期就焊死，运行时既加不了也拆不掉。
class Dashboard : public IClockObserver, public ITemperatureObserver {
public:
    void OnTimeChanged(int h, int m) override {
        std::cout << "  [Dashboard] 时间更新 -> " << h << ":" << (m < 10 ? "0" : "") << m << "\n";
    }
    void OnTemperatureChanged(double c) override {
        std::cout << "  [Dashboard] 温度更新 -> " << c << " C\n";
    }
};

// ---------------------------------------------------------------------------
// 麻烦二：同一“类型”的事件要观察两次（1 秒心跳 + 30 秒自检）
// ---------------------------------------------------------------------------
class ITimerObserver {
public:
    virtual void OnTimer() = 0;  // 注意：无参数，回调里无从知道是谁触发的
    virtual ~ITimerObserver() = default;
};

class Timer {  // Subject: 定时器（两个实例 = 两个不同频率的定时器）
    std::vector<ITimerObserver*> obs_;

public:
    void Attach(ITimerObserver* o) { obs_.push_back(o); }
    void Fire() { for (auto* o : obs_) o->OnTimer(); }
};

class SystemMonitor;  // 前向声明

// --- Workaround A：转发壳类（trampoline / sink）-------------------------
// 把“一个接口只能占一个槽”变成“每个槽一个对象”。
// 代价：每多一条订阅关系，就要手写一个小类 -> 样板代码爆炸。
class HeartbeatSink : public ITimerObserver {
    SystemMonitor* owner_;
public:
    explicit HeartbeatSink(SystemMonitor* o) : owner_(o) {}
    void OnTimer() override;  // SystemMonitor 定义之后再实现
};

class SelfCheckSink : public ITimerObserver {
    SystemMonitor* owner_;
public:
    explicit SelfCheckSink(SystemMonitor* o) : owner_(o) {}
    void OnTimer() override;
};

// --- Workaround B：把 sender 塞进参数，靠 if/else 手动分派 ---------------
class ITimerObserverWithSender {
public:
    virtual void OnTimer(const std::string& source) = 0;
    virtual ~ITimerObserverWithSender() = default;
};

class TimerWithSender {
    std::vector<ITimerObserverWithSender*> obs_;
    std::string name_;
public:
    explicit TimerWithSender(std::string n) : name_(std::move(n)) {}
    void Attach(ITimerObserverWithSender* o) { obs_.push_back(o); }
    void Fire() { for (auto* o : obs_) o->OnTimer(name_); }
};

class SystemMonitor {
public:
    void OnHeartbeat() { std::cout << "  [Monitor] 心跳 tick\n"; }
    void OnSelfCheck() { std::cout << "  [Monitor] 30s 自检\n"; }

    // Workaround A：把两个 sink 作为成员对象养着
    HeartbeatSink  heartbeat_{this};
    SelfCheckSink  selfcheck_{this};

    // Workaround B：订阅关系的“身份”退化成字符串，靠运行时比对
    std::string heartbeatSource_ = "1s";
    std::string selfCheckSource_ = "30s";
};

void HeartbeatSink::OnTimer()  { owner_->OnHeartbeat(); }
void SelfCheckSink::OnTimer()  { owner_->OnSelfCheck(); }

// Workaround B 的观察者：认人的活儿全堆在一个函数里
class MonitorDispatcher : public ITimerObserverWithSender {
    SystemMonitor& m_;
public:
    explicit MonitorDispatcher(SystemMonitor& m) : m_(m) {}
    void OnTimer(const std::string& source) override {
        if (source == m_.heartbeatSource_)      m_.OnHeartbeat();
        else if (source == m_.selfCheckSource_) m_.OnSelfCheck();
        // 每加一个定时器，就得回来改这里 —— 违反开闭原则
        // 而且“谁发来的”这个信息泄漏进了业务逻辑
    }
};

// ---------------------------------------------------------------------------
// 彩蛋：连“用模板把同一个接口复制成不同类型”这种技巧都会翻车
// ---------------------------------------------------------------------------
template <int Tag>
class ITimerObserverT {
public:
    virtual void OnTimer() = 0;
    virtual ~ITimerObserverT() = default;
};

// 看起来 ITimerObserverT<1> 和 ITimerObserverT<2> 是两个不同类型，可以同时继承？
class TaggedMonitor : public ITimerObserverT<1>, public ITimerObserverT<2> {
public:
    // 坑：下面这一个 OnTimer 会“同时”覆盖两个基类的虚函数！
    // 因为两个基类里的函数同名同签名，C++ 视为同一个虚函数的两次声明。
    // 你没法给 Tag=1 和 Tag=2 各写一份不同实现。
    void OnTimer() override { std::cout << "  [TaggedMonitor] 两个基类的 OnTimer 被同一个实现覆盖了\n"; }
};

// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== 麻烦一：观察两种类型 -> 必须多继承 ===\n";
    Clock clock;
    TemperatureSensor sensor;
    Dashboard dash;  // 一个类被迫同时“是”时钟观察者“又是”温度观察者
    clock.Attach(&dash);
    sensor.Attach(&dash);
    clock.Tick();
    sensor.Sample();

    std::cout << "\n=== 麻烦二之 Workaround A：转发壳类 ===\n";
    Timer t1s, t30s;
    SystemMonitor m;
    t1s.Attach(&m.heartbeat_);   // 挂的是壳对象，不是 m 本身
    t30s.Attach(&m.selfcheck_);
    t1s.Fire();
    t30s.Fire();

    std::cout << "\n=== 麻烦二之 Workaround B：sender + if/else 分派 ===\n";
    TimerWithSender w1s("1s"), w30s("30s");
    MonitorDispatcher disp(m);
    w1s.Attach(&disp);
    w30s.Attach(&disp);
    w1s.Fire();
    w30s.Fire();

    std::cout << "\n=== 彩蛋：模板复制接口也救不了 ===\n";
    TaggedMonitor tm;
    static_cast<ITimerObserverT<1>&>(tm).OnTimer();
    static_cast<ITimerObserverT<2>&>(tm).OnTimer();  // 打出来的还是那句话
    std::cout << "  -> 说明：签名相同的虚函数在多继承下会被“合并”，无法按基类分别实现\n";

    return 0;
}
