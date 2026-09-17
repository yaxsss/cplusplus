# Event::Subscription 里 `id_` 的作用

对应源码：[`concurrent/observer/observer_modern.cpp`](../concurrent/observer/observer_modern.cpp)

`id_` 是这条订阅在 `Event` 里的**编号**，用来精确退订，不靠比较 `std::function`。

---

## 为什么需要编号

同一个 `Event` 上可以挂多份回调。心跳、自检、日志是三个不同的 lambda，`std::function` 对 lambda **几乎不能 `==`**。如果像 `SignalTrivial::disconnect` 那样用函数对象当钥匙，最常见的订阅方式根本退不掉。

所以 `Event` 不存「当初那份 function 长什么样」，而是给每次 `Subscribe` 发一个递增号：

```cpp
std::vector<std::pair<ID, Handler>> handlers_;
ID nextId_ = 1;   // 有效 id 从 1 起
```

---

## 发号与退订

`Subscribe` 一边把 `(id, handler)` 放进列表，一边把同一个 id 写进返回的句柄：

```cpp
[[nodiscard]] Subscription Subscribe(Handler h) {
    handlers_.emplace_back(nextId_, std::move(h));
    return Subscription{this, nextId_++};
}
```

句柄里有两样东西：

| 成员 | 作用 |
|------|------|
| `owner_` | 指向哪个 `Event`；`nullptr` 表示已经退订 / 空句柄 |
| `id_` | 要删 `handlers_` 里的哪一条 |

退订链路：

```text
Subscription 析构 / Unsubscribe()
    → Release()
        → owner_->Unsubscribe(id_)
            → 删掉 handlers_ 里 first == id_ 的那一项
        → owner_ = nullptr
```

```cpp
void Release() {
    if (owner_) { owner_->Unsubscribe(id_); owner_ = nullptr; }
}

void Unsubscribe(ID id) {
    handlers_.erase(
        std::remove_if(handlers_.begin(), handlers_.end(),
                       [id](const auto& p) { return p.first == id; }),
        handlers_.end());
}
```

三条订阅可以各退各的，不会误删别人。

---

## 为什么默认是 `id_ = 0`

```cpp
Event* owner_ = nullptr;
ID id_ = 0;
```

`0` 是空句柄的哨兵：默认构造、已经 `Release()` 过的 `Subscription` 不指向任何订阅。真正有效的 id 从 `nextId_ = 1` 起，所以 `0` 表示「没有东西可退」。

是否真去退订，看的是 `owner_ == nullptr`，不是 `id_ == 0`。`id_` 只在 `owner_` 有效时才有意义。

---

## 和 move 的关系

`Subscription` 禁止拷贝，只允许移动，避免两个句柄拿着同一个 `id_` 抢着退订：

```cpp
Subscription(Subscription&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)), id_(other.id_) {}
```

`std::exchange` 把来源的 `owner_` 置空，来源变成空句柄，析构时不会再 `Unsubscribe` 一次。`id_` 跟着句柄走，始终对应列表里那一条。

---

## 对照

| 做法 | 怎么找到要退的那条 | 对 lambda |
|------|-------------------|-----------|
| `SignalTrivial::disconnect(func)` | `std::function ==` | 基本失效 |
| `Event` 的 `id_` | 递增整数当钥匙 | 每条订阅一个号，精确删除 |

更完整的两份实现对比见 [`signal_slot.md`](signal_slot.md)。
