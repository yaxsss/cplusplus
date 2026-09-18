// ============================================================================
// memory_order_relaxed：不保证「本线程里先写的，别人一定先看到」
//
// 旧写法两个线程对同一个 a store 1..9，那只是写者交错，seq_cst 一样会交错。
// relaxed 真正放开的是：不同原子变量之间没有 happens-before，可见顺序可以
// 和代码顺序相反。
//
// 实验 A（消息传递）：T1 先 store x 再 store y；T2 先 load y 再 load x。
//   若 y==1 且 x==0：两个 store 被看到的顺序反了。
//   x86 TSO 禁止 store-store 重排，这台机器上 A 几乎打不出来；ARM/POWER 会。
//
// 实验 B（store buffer）：T1 写 x 再读 y；T2 写 y 再读 x。
//   两边都读到 0：各自先看到自己的写、还没看到对方的写。
//   relaxed 允许；若改成 seq_cst 则禁止。x86 上就能打出来。
//
// 编译: g++ -std=c++17 -O2 -pthread relaxed.cpp -o relaxed && ./relaxed
// ============================================================================
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> x{0}, y{0};
int r1 = 0, r2 = 0;

// y 已经是 1，x 还是 0 → 后写的 y 先被看到
void test_message_passing() {
  x.store(0, std::memory_order_relaxed);
  y.store(0, std::memory_order_relaxed);
  r1 = r2 = 0;

  std::thread t1([] {
    x.store(1, std::memory_order_relaxed);
    y.store(1, std::memory_order_relaxed);
  });
  std::thread t2([] {
    r1 = y.load(std::memory_order_relaxed);
    r2 = x.load(std::memory_order_relaxed);
  });
  t1.join();
  t2.join();
}

// 两边都读到 0 → 两个 store 都还没让对方看见
void test_store_buffer() {
  x.store(0, std::memory_order_relaxed);
  y.store(0, std::memory_order_relaxed);
  r1 = r2 = 0;

  std::thread t1([] {
    x.store(1, std::memory_order_relaxed);
    r1 = y.load(std::memory_order_relaxed);
  });
  std::thread t2([] {
    y.store(1, std::memory_order_relaxed);
    r2 = x.load(std::memory_order_relaxed);
  });
  t1.join();
  t2.join();
}

int main() {
  int mp = 0, sb = 0;
  constexpr int kIters = 100000;

  for (int i = 0; i < kIters; ++i) {
    test_message_passing();
    if (r1 == 1 && r2 == 0) {
      ++mp;
    }
  }
  for (int i = 0; i < kIters; ++i) {
    test_store_buffer();
    if (r1 == 0 && r2 == 0) {
      ++sb;
    }
  }

  std::cout << "A 消息传递  y==1 && x==0 : " << mp << " / " << kIters
            << "  (x86 上通常为 0)\n";
  std::cout << "B store buffer 两边都读到 0 : " << sb << " / " << kIters
            << "  (relaxed 允许，seq_cst 不允许)\n";
}
