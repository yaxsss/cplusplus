count_down_latch（倒计时）的作用：

对应源码：[`concurrent/count_down_latch/count_down_latch_simple.cpp`](../concurrent/count_down_latch/count_down_latch_simple.cpp)

* 主线程发起多个子线程，等这些子线程各自都完成一定的任务之后，主线程才继续执行。通常用于主线程等待多个子线程完成初始化

```cpp
const int N = 3;
CountDownLatch ready(N);          // 倒计数 = 子线程数
std::vector<std::thread> threads;

for (int i = 0; i < N; ++i) {
    threads.emplace_back([i, &ready] {
        // 各自做初始化……
        ready.countDown();        // 我准备好了
    });
}

ready.wait();                     // 主线程：等人到齐再继续
// 此时 N 个子线程的初始化都已完成
for (auto& t : threads) t.join();
```

* 主线程发起多个子线程，子线程都等待主线程，主线程完成其他一些任务之后，通知所有子线程开始执行。通常用于多个子线程等待主线程发起“起跑”命令

```cpp
const int N = 3;
CountDownLatch start(1);          // 倒计数 = 1，一扇门
std::vector<std::thread> threads;

for (int i = 0; i < N; ++i) {
    threads.emplace_back([i, &start] {
        start.wait();             // 所有子线程堵在起跑线
        // 接到命令后才真正开始干活
    });
}

// 主线程先做自己的准备（加载配置、打开日志……）
start.countDown();                // 门开，notify_all，所有人一起跑
for (auto& t : threads) t.join();
```

两种可以叠在一起：先用 `ready(N)` 等初始化，再用 `start(1)` 统一发令。
`count == 0` 时为什么必须 `notify_all`，见 [`notify_one_vs_all.md`](notify_one_vs_all.md)。
