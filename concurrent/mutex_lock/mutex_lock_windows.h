#ifndef MUTEX_LOCK_WINDOWS_H
#define MUTEX_LOCK_WINDOWS_H

#include <windows.h>
#include <cassert>

class MutexLock {
public:
    MutexLock() : holder_(0) {
        InitializeCriticalSection(&cs_);
    }
    ~MutexLock() {
        DeleteCriticalSection(&cs_);
    }
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

    bool isLockedByThisThread() {
        return holder_ == GetCurrentThreadId();
    }
    void assertLocked() {
        assert(isLockedByThisThread());
    }
    void lock() {
        EnterCriticalSection(&cs_);
        holder_ = GetCurrentThreadId();
    }
    void unlock() {
        holder_ = 0;
        LeaveCriticalSection(&cs_);
    }
    // 对标 Linux 的 getPthreadMutex()：条件变量等需要 CRITICAL_SECTION*
    CRITICAL_SECTION* getHandle() {
        return &cs_;
    }

private:
    CRITICAL_SECTION cs_;
    DWORD holder_;
};

#endif // MUTEX_LOCK_WINDOWS_H
