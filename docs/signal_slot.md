# SignalTrivial vs Event：两份现代 Observer 实现

对应源码：

- [`concurrent/signal_slot/signal_slot.cpp`](../concurrent/signal_slot/signal_slot.cpp) — `SignalTrivial`
- [`concurrent/observer/observer_modern.cpp`](../concurrent/observer/observer_modern.cpp) — `Event`

两者都用 `std::function` 多播回调，替代继承式 Observer。Subject 不再要求观察者继承接口，而是持有一组可调用对象；谁来听、听几次，都是运行期往容器里塞值。

差别在完整度：`SignalTrivial` 是最小内核，`Event` 按生产用法补上了句柄、退订和遍历安全。经典 Observer 的两个麻烦见 [`observer_problem.md`](observer_problem.md)。

---

## 核心模型（相同）

```text
继承式：观察者 “是” 一个 IXxxObserver（编译期、唯一、不可增减）
回调式：观察者 “持有” N 个 std::function 值（运行期、任意多、可增删）
```

`Clock::TimeChanged`、`Timer::Elapsed` 都是成员对象，不是「要求你继承的接口」。

---

## 1. 模板形状

| | `SignalTrivial` | `Event` |
|--|--|--|
| 签名 | `RET(ARGS...)`，允许有返回值 | `Args...`，回调固定 `void` |
| 存储 | `vector<std::function>` | `vector<pair<ID, Handler>>` |

```cpp
template<typename RET, typename... ARGS>
class SignalTrivial<RET(ARGS...)> { ... };

template <class... Args>
class Event {  // Handler = std::function<void(Args...)>
```

`SignalTrivial` 更接近通用信号槽；`Event` 更接近 C# 的 `event`：只通知、不收集返回值。

`Event` 用 `vector` 而不是 `map`，保证回调按订阅顺序触发。

---

## 2. 订阅 / 退订

`SignalTrivial` 用函数对象本身当钥匙：

```cpp
void connect(Functor&& func) {
    functors_.push_back(func);
}
void disconnect(Functor&& func) {
    functors_.erase(std::remove(functors_.begin(), functors_.end(), func),
                    functors_.end());
}
```

问题：

- `std::function` 的 `operator==` 对 lambda **基本不可用**（lambda 没有 `==`），最常见的订阅方式退订会失败
- 缺 `#include <algorithm>`，`std::remove` 本身也编译不过
- `connect` 形参是 `&&`，体内却 `push_back(func)`，没有 `std::move`

`Event` 发一个递增 `ID`，再包成 RAII `Subscription`：

```cpp
[[nodiscard]] Subscription Subscribe(Handler h) {
    handlers_.emplace_back(nextId_, std::move(h));
    return Subscription{this, nextId_++};
}
```

观察者不用保存「当初那份 `std::function`」。`Subscription` 析构或 `Unsubscribe()` 就按 id 删除。

`id_` 为什么存在、为什么默认是 `0`，见 [`event_subscription.md`](event_subscription.md)。

---

## 3. 触发时能不能改订阅列表

`SignalTrivial::call` 直接遍历活列表：

```cpp
void call(ARGS... args) {
    for (auto& functor : functors_) functor(args...);
}
```

回调里如果 `connect` / `disconnect`，迭代器可能失效，属于未定义行为。

`Event::Fire` 先拷贝快照再调：

```cpp
void Fire(Args... args) const {
    auto snapshot = handlers_;
    for (auto& [id, h] : snapshot) h(args...);
}
```

回调里增删订阅只影响下一次，这次走完快照。

---

## 4. 生命周期

`Event::Subscription` 是 move-only：

- 禁止拷贝，避免两个句柄抢着退订同一条
- 析构即 `Release()`，杜绝「观察者已死仍被回调」

`Dashboard` / `SystemMonitor` 把句柄放进成员。对象一析构就全部退订。`main` 里 `monitor` 销毁后再 `t1s.Fire()`，订阅数是 0，就是在验证这件事。

`SignalTrivial` 没有句柄。观察者死了，槽里的 `std::function` 可能还抓着悬空 `this`。

---

## 5. 用法完整度

`observer_modern.cpp` 把经典 Observer 的两个麻烦直接解掉：

- 听时间和温度：订两个不同 `Event`，不用多继承
- 同一 `Timer::Elapsed` 订两次：两个 lambda 各走各的
- 还能给同一个心跳再挂一个日志回调
- 匿名 lambda 也能当观察者，不必为它定义一个类

`signal_slot.cpp` 只有槽本身，没有业务场景，也没有 `main`。

---

## 对照

| | `SignalTrivial` | `Event` |
|--|--|--|
| 回调类型 | `std::function<RET(ARGS...)>` | `std::function<void(Args...)>` |
| 如何找到要退的那条 | 比较 `std::function`（对 lambda 失效） | 递增 `ID` |
| 退订句柄 | 无 | RAII `Subscription`（move-only） |
| 触发时改订阅 | 直接遍历，不安全 | 拷贝快照 |
| 观察者死亡 | 可能悬空回调 | 句柄析构即退订 |
| 演示场景 | 无 | 对标 classic 的两个麻烦 |

一句话：`SignalTrivial` 是「`vector<function>` + 遍历调用」；`Event` 在这之上加了 **id 句柄、RAII 退订、快照触发**。

要把 `SignalTrivial` 用到 `observer_modern` 那种场景，至少得补这三样，并且不要用 `std::function ==` 来退订。
