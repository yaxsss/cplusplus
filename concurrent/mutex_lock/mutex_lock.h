#ifndef MUTEX_LOCK_H
#define MUTEX_LOCK_H

#if defined(_WIN32)
#include "mutex_lock_windows.h"
#elif defined(__linux__)
#include "mutex_lock_linux.h"
#else
#error "MutexLock: unsupported platform"
#endif

class MutexLockGuard {
public:
    explicit MutexLockGuard(MutexLock& mutex) : mutex_(mutex) {
        mutex_.lock();
    }
    ~MutexLockGuard() {
        mutex_.unlock();
    }
    MutexLockGuard(const MutexLockGuard&) = delete;
    MutexLockGuard& operator=(const MutexLockGuard&) = delete;

private:
    MutexLock& mutex_;
};

// 禁止 MutexLockGuard(mutex); 这种写法：临时对象语句结束就析构，等于没锁住。
// 函数宏只匹配「名字后面紧跟括号」，所以 MutexLockGuard lock(m); 不受影响。
// 不要用 static_assert(false)：有的编译器/clangd 在解析宏体时就会报断言失败。
#define MutexLockGuard(x) error "Missing guard variable name"

#endif
