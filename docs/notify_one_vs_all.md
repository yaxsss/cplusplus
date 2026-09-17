# notify_one vs notify_all：队列一次叫醒一个，Latch 一次叫醒全部

对应源码：

- [`concurrent/block_queue/blocking_queue_simple.cpp`](../concurrent/block_queue/blocking_queue_simple.cpp)
- [`concurrent/count_down_latch/count_down_latch_simple.cpp`](../concurrent/count_down_latch/count_down_latch_simple.cpp)

关键差别：**一次状态变化，能让几个等待者的条件变成真。**

---

## BlockingQueue：1 对 1，用 `notify_one()`

```cpp
void push(const T& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return queue_.size() < maxSize_; });
    queue_.push(value);
    condition_.notify_one();
}
T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty(); });
    T value = queue_.front();
    queue_.pop();
    condition_.notify_one();
    return value;
}
```

`push` 多了一个元素，只够 **一个** `pop` 拿走；`pop` 腾出一个空位，只够 **一个** `push` 放进去。

```text
push 之后：empty → 非空    唤醒 1 个正在等数据的消费者
pop  之后：full  → 有空位  唤醒 1 个正在等空位的生产者
```

有界队列里，同一时刻的等待者还是同一类人：

- 空了：只会有消费者在等
- 满了：只会有生产者在等

所以叫醒一个就够，多叫是浪费。

谓词会再检查一遍（`queue_.size() < maxSize_` / `!queue_.empty()`）。万一碰巧叫醒了不该醒的人，他会继续睡。但因为等着的人类型一致，`notify_one` 通常就是对的那个。

---

## CountDownLatch：1 对 N，用 `notify_all()`

```cpp
void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return count_ == 0; });
}
void countDown() {
    std::unique_lock<std::mutex> lock(mutex_);
    --count_;
    if (count_ == 0) {
        condition_.notify_all();
    }
}
```

`wait()` 的条件是 `count_ == 0`。可以有很多线程同时卡在这里，等的是**同一扇门**。

`count_ != 0` 时谁也不该醒（谓词仍是假）。只有减到 0 那一次，**所有** `wait()` 的条件同时变真：

```text
count:  3 → 2 → 1 → 0
                    └── 门开了，所有 wait() 都该走
```

如果这里写 `notify_one()`，只有一条线程被放行，其余的会永远睡死——没有下一次 `count == 0` 的通知了。

所以只在 `count_ == 0` 时通知，而且必须 `notify_all()`。

---

## 对照

| | BlockingQueue | CountDownLatch |
|--|--|--|
| 等待条件 | 有空位 / 有数据（互斥的两类） | 大家都等 `count == 0` |
| 一次操作满足几个人 | 正好 1 个 | 全部 |
| 何时通知 | 每次 `push` / `pop` | 仅当 `count == 0` |
| 通知 | `notify_one()` | `notify_all()` |

记忆：资源是「一份给一人」就 `notify_one`；是「门开了大家一起走」就 `notify_all`。
