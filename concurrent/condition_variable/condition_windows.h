#ifndef CONDITION_WINDOWS_H
#define CONDITION_WINDOWS_H
#include "../mutex_lock/mutex_lock.h"
#include <windows.h>

class Condition {
public:
    explicit Condition(MutexLock& mutex) : mutex_(mutex) {
        InitializeConditionVariable(&cv_);
    }
    // CONDITION_VARIABLE 没有 Destroy API
    ~Condition() = default;
    Condition(const Condition&) = delete;
    Condition& operator=(const Condition&) = delete;

    // 调用前必须已持有 mutex_（SleepConditionVariableCS 会原子地释放并重新获得 CRITICAL_SECTION）
    void wait() {
        SleepConditionVariableCS(&cv_, mutex_.getHandle(), INFINITE);
    }
    void notify_one() {
        WakeConditionVariable(&cv_);
    }
    void notify_all() {
        WakeAllConditionVariable(&cv_);
    }

private:
    MutexLock& mutex_;
    CONDITION_VARIABLE cv_;
};

#endif
