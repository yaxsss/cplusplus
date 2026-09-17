// ============================================================================
// 现代写法：std::function / lambda 版 Observer —— 零继承
// 编译: g++ -std=c++17 -Wall observer_modern.cpp -o modern && ./modern
//
// 关键变化：订阅关系从“类型层面”降到了“值层面”。
//   继承式：SystemMonitor “是”一个 ITimerObserver（编译期、唯一、不可增减）
//   回调式：SystemMonitor “持有” N 个 std::function 值（运行期、任意多、可增删）
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// 通用、类型安全的多播事件槽（C++ 版的 C# event）
// 用 vector 而非 map：保证回调按订阅顺序触发
// ---------------------------------------------------------------------------
template <class... Args>
class Event {
public:
    using Handler = std::function<void(Args...)>;
    using ID = std::size_t;

    // RAII 订阅句柄：析构即自动退订，杜绝“观察者已死仍被回调”
    class Subscription {
    public:
        Subscription() = default;
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), id_(other.id_) {}

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                Release();
                owner_ = std::exchange(other.owner_, nullptr);
                id_ = other.id_;
            }
            return *this;
        }

        ~Subscription() { Release(); }

        void Unsubscribe() { Release(); }

    private:
        friend class Event;
        Subscription(Event* o, ID id) : owner_(o), id_(id) {}
        void Release() {
            if (owner_) { owner_->Unsubscribe(id_); owner_ = nullptr; }
        }
        Event* owner_ = nullptr;
        ID id_ = 0;
    };

    [[nodiscard]] Subscription Subscribe(Handler h) {
        handlers_.emplace_back(nextId_, std::move(h));
        return Subscription{this, nextId_++};
    }

    void Unsubscribe(ID id) {
        handlers_.erase(
            std::remove_if(handlers_.begin(), handlers_.end(),
                           [id](const auto& p) { return p.first == id; }),
            handlers_.end());
    }

    std::size_t SubscriberCount() const { return handlers_.size(); }

    // 拷贝快照再遍历：允许回调内部增删订阅
    void Fire(Args... args) const {
        auto snapshot = handlers_;
        for (auto& [id, h] : snapshot) h(args...);
    }

private:
    std::vector<std::pair<ID, Handler>> handlers_;
    ID nextId_ = 1;
};

// ---------------------------------------------------------------------------
// 被观察者：只声明“我有哪些事件”，完全不关心谁来听
// ---------------------------------------------------------------------------
class Clock {
public:
    Event<int, int> TimeChanged;   // 事件是“成员对象”，不是“要求你继承的接口”
    void Tick() {
        if (++minute_ == 60) { minute_ = 0; hour_ = (hour_ + 1) % 24; }
        TimeChanged.Fire(hour_, minute_);
    }
private:
    int hour_ = 9, minute_ = 0;
};

class TemperatureSensor {
public:
    Event<double> TemperatureChanged;  // 参数直接是 double，无需 Subject* 再 downcast
    void Sample() { c_ += 0.5; TemperatureChanged.Fire(c_); }
private:
    double c_ = 26.0;
};

class Timer {
public:
    Event<> Elapsed;  // 无参事件
    void Fire() { Elapsed.Fire(); }
};

// ---------------------------------------------------------------------------
// 场景一：一个类同时听“时间”和“温度” —— 不用多继承，也不用改类声明
// ---------------------------------------------------------------------------
class Dashboard {
public:
    Dashboard(Clock& c, TemperatureSensor& s) {
        timeSubs_.push_back(c.TimeChanged.Subscribe([this](int h, int m) { OnTimeChanged(h, m); }));
        tempSubs_.push_back(s.TemperatureChanged.Subscribe([this](double v) { OnTemperatureChanged(v); }));
    }

private:
    void OnTimeChanged(int h, int m) {
        std::cout << "  [Dashboard] 时间 -> " << h << ":" << (m < 10 ? "0" : "") << m << "\n";
    }
    void OnTemperatureChanged(double c) {
        std::cout << "  [Dashboard] 温度 -> " << c << " C\n";
    }
    std::vector<Event<int, int>::Subscription> timeSubs_;
    std::vector<Event<double>::Subscription>  tempSubs_;
};

// ---------------------------------------------------------------------------
// 场景二：同一“类型”的事件听两次 —— 注册两个不同回调即可，天然区分
// ---------------------------------------------------------------------------
class SystemMonitor {
public:
    SystemMonitor(Timer& heartbeat, Timer& selfCheck) {
        subs_.push_back(heartbeat.Elapsed.Subscribe([this] { OnHeartbeat(); }));
        subs_.push_back(selfCheck.Elapsed.Subscribe([this] { OnSelfCheck(); }));
        // 对“同一个”定时器再挂一个回调（比如打日志）——继承式做不到，回调式随手就来
        subs_.push_back(heartbeat.Elapsed.Subscribe([this] { OnHeartbeatLog(); }));

        std::cout << "  [Monitor] 心跳定时器当前订阅数 = " << heartbeat.Elapsed.SubscriberCount() << "\n";
    }

private:
    void OnHeartbeat()    { std::cout << "  [Monitor] 心跳 tick\n"; }
    void OnSelfCheck()    { std::cout << "  [Monitor] 30s 自检\n"; }
    void OnHeartbeatLog() { std::cout << "  [Monitor] 心跳(附加日志回调)\n"; }

    std::vector<Event<>::Subscription> subs_;  // RAII：Monitor 析构即全部退订
};

// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== 场景一：听两种不同类型的事件（无多继承）===\n";
    Clock clock;
    TemperatureSensor sensor;
    {
        Dashboard dash(clock, sensor);
        clock.Tick();
        sensor.Sample();
    }  // dash 析构 -> 自动退订

    std::cout << "\n=== 场景二：同一类型事件听多次（1s 心跳 / 30s 自检 / 附加日志）===\n";
    Timer t1s, t30s;
    {
        SystemMonitor monitor(t1s, t30s);
        std::cout << "  -- 触发 1s 定时器 --\n";
        t1s.Fire();
        std::cout << "  -- 触发 30s 定时器 --\n";
        t30s.Fire();
    }  // monitor 析构 -> 自动退订

    std::cout << "\n=== 验证自动退订：再触发一次，应当没有任何输出 ===\n";
    t1s.Fire();
    t30s.Fire();
    std::cout << "  [OK] 1s 剩余订阅数 = " << t1s.Elapsed.SubscriberCount()
              << "，30s 剩余订阅数 = " << t30s.Elapsed.SubscriberCount() << "\n";

    std::cout << "\n=== 额外福利：匿名 / 临时观察者，不必为它定义一个类 ===\n";
    int count = 0;
    auto once = t1s.Elapsed.Subscribe([&count] { ++count; std::cout << "  [lambda] 捕获局部变量的匿名观察者\n"; });
    t1s.Fire();
    std::cout << "  count = " << count << "\n";
    once.Unsubscribe();
    t1s.Fire();
    std::cout << "  退订后再触发，count 仍为 " << count << "\n";

    return 0;
}
