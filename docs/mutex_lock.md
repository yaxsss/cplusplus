# MutexLock 为什么还没到工业强度

对应源码：[`concurrent/mutex_lock/mutex_lock.cpp`](../concurrent/mutex_lock/mutex_lock.cpp)

当前实现能用，但有两处达不到工业强度：

1. `pthread_mutex_init(&mutex_, NULL)` 建出来的是 **DEFAULT**，不是我们预想的 **NORMAL**
2. 没有检查 pthread 返回值；`assert()` 在 release 里是空语句，挡不住 `ENOMEM`

---

## 1. DEFAULT 不是 NORMAL，要用 mutexattr 显式指定

`pthread_mutex_init(mutex, NULL)` 表示「用实现给的默认属性」。POSIX 规定这时互斥锁类型是 `PTHREAD_MUTEX_DEFAULT`，**语义由实现自己定**，不保证等于 `PTHREAD_MUTEX_NORMAL`。

| 类型 | 同一线程锁两次 | 没锁却 unlock | 典型用途 |
|------|----------------|---------------|----------|
| `NORMAL` | 死锁（卡死，方便暴露重入 bug） | 未定义 | 日常互斥，最符合「一把锁」的直觉 |
| `ERRORCHECK` | 返回 `EDEADLK` | 返回 `EPERM` | 调试 |
| `RECURSIVE` | 成功，计数 +1 | 未定义 | 允许重入，但会把「不该重入」的 bug 藏起来 |
| `DEFAULT` | **实现定义** | **实现定义** | `NULL` 属性走这条 |

Linux glibc 上 `DEFAULT` 和 `NORMAL` 数值常常一样，换到别的系统（或部分 Android / 嵌入式 libc）就可能变成 ERRORCHECK 甚至别的行为。工业代码不赌实现，用 `pthread_mutexattr_t` 写死类型：

```cpp
pthread_mutexattr_t attr;
CHECK(pthread_mutexattr_init(&attr) == 0);
CHECK(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) == 0);
CHECK(pthread_mutex_init(&mutex_, &attr) == 0);
CHECK(pthread_mutexattr_destroy(&attr) == 0);
```

`attr` 只在 init 时用，init 完就可以 destroy。之后这把锁的行为与平台默认值无关，始终是 NORMAL：同一线程再 `lock()` 会卡死，而不是悄悄重入或返回错误码。

---

## 2. 不能用 `assert()` 检查返回值，需要 non-debug 的 CHECK

`pthread_mutex_init` / `lock` / `unlock` / `destroy` 都有返回值。失败不只是「写错了参数」，还包括资源耗尽：

- `ENOMEM`：内存不够，锁根本没建起来
- `EAGAIN`：系统资源（比如锁的数量）不够

这类错误在 **release** 里同样会发生。一旦 init 失败还继续跑，后面的 `lock()` 是未定义行为，现场会烂得查不出来。正确反应是：**立刻打日志、清场、退出**。

`assert(ret == 0)` 做不到这一点。C 标准里 NDEBUG 一开，`assert` 就变成空语句：

```cpp
#ifdef NDEBUG
#define assert(e) ((void)0)   // release 里整句消失
#endif

assert(pthread_mutex_init(&mutex_, &attr) == 0);  // -O2 -DNDEBUG 时等于没写
```

所以「检查返回值」的意义不在 debug 里抓程序员笔误，而在 **任何构建类型下** 拦住 ENOMEM。这就是 non-debug assert：条件失败永远终止进程。

glog 的 `CHECK()` 就是这个思路——`CHECK` 不看 `NDEBUG`，失败走 `LOG(FATAL)`，进程 abort：

```cpp
CHECK(pthread_mutex_init(&mutex_, &attr) == 0);
CHECK_EQ(pthread_mutex_lock(&mutex_), 0);
```

没有 glog 时，自己写一个永远生效的宏即可（注意：这不是 `assert`，release 也不会被吃掉）：

```cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n",                  \
                   #cond, __FILE__, __LINE__);                            \
      std::abort();                                                       \
    }                                                                     \
  } while (0)

// 专门检查 pthread 返回值，失败时带上 strerror
#define CHECK_PTHREAD(call)                                               \
  do {                                                                    \
    int errnum = (call);                                                  \
    if (errnum != 0) {                                                    \
      std::fprintf(stderr, "CHECK_PTHREAD failed: %s = %d (%s) (%s:%d)\n",\
                   #call, errnum, std::strerror(errnum),                  \
                   __FILE__, __LINE__);                                   \
      std::abort();                                                       \
    }                                                                     \
  } while (0)
```

muduo 的 `MCHECK` 是同一类东西：直接调 glibc 的 `__assert_perror_fail`，不经过会被 NDEBUG 掏空的 `assert()` 宏。

`lock()` / `unlock()` 在 NORMAL 锁、且 init 已成功时，几乎只有「逻辑用错」（重复加锁、解别人的锁）才会失败；init / attr 失败才是 ENOMEM 主战场。工业写法两边都 CHECK：init 防资源不足，lock/unlock 防误用。

---

## 合在一起的工业写法示例

```cpp
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#define CHECK_PTHREAD(call)                                               \
  do {                                                                    \
    int errnum = (call);                                                  \
    if (errnum != 0) {                                                    \
      std::fprintf(stderr, "CHECK_PTHREAD failed: %s = %d (%s) (%s:%d)\n",\
                   #call, errnum, std::strerror(errnum),                  \
                   __FILE__, __LINE__);                                   \
      std::abort();                                                       \
    }                                                                     \
  } while (0)

class MutexLock {
public:
    MutexLock() : holder_(0) {
        pthread_mutexattr_t attr;
        CHECK_PTHREAD(pthread_mutexattr_init(&attr));
        CHECK_PTHREAD(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL));
        CHECK_PTHREAD(pthread_mutex_init(&mutex_, &attr));
        CHECK_PTHREAD(pthread_mutexattr_destroy(&attr));
    }

    ~MutexLock() {
        CHECK_PTHREAD(pthread_mutex_destroy(&mutex_));
    }

    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

    void lock() {
        CHECK_PTHREAD(pthread_mutex_lock(&mutex_));
        holder_ = pthread_self();
    }

    void unlock() {
        holder_ = 0;
        CHECK_PTHREAD(pthread_mutex_unlock(&mutex_));
    }

    pthread_mutex_t* getPthreadMutex() { return &mutex_; }

private:
    pthread_mutex_t mutex_;
    pid_t holder_;
};
```

对照当前教学版：

| | 现在的 `mutex_lock.cpp` | 工业写法 |
|--|--|--|
| 锁类型 | `init(..., NULL)` → DEFAULT，随 libc | attr 指定 `PTHREAD_MUTEX_NORMAL` |
| 返回值 | 完全忽略 | `CHECK_PTHREAD`，release 也生效 |
| `ENOMEM` | 继续跑，随后未定义 | 立刻 abort 清场 |
| `assert` | 只在 debug 有效 | 不能替代 CHECK |

两句话收束：

- **类型要写死**：NULL 属性是「听天由命」，mutexattr 才是「我要 NORMAL」。
- **失败要在任何构建里都看见**：`assert` 是调试器；`CHECK` / `MCHECK` 才是生产环境的保险丝。
