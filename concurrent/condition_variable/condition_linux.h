#ifndef CONDITION_LINUX_H
#define CONDITION_LINUX_H
#include "../mutex_lock/mutex_lock.h"
#ifdef __linux__
#include <pthread.h>
#include <cstddef>

class Condition {
public:
    explicit Condition(MutexLock& mutex) : mutex_(mutex) {
        pthread_cond_init(&cond_, NULL);
    }
    ~Condition() {
        pthread_cond_destroy(&cond_);
    }
    Condition(const Condition&) = delete;
    Condition& operator=(const Condition&) = delete;

    // 调用前必须已持有 mutex_（与 pthread_cond_wait 相同）
    void wait() {
        pthread_cond_wait(&cond_, mutex_.getPthreadMutex());
    }
    void notify_one() {
        pthread_cond_signal(&cond_);
    }
    void notify_all() {
        pthread_cond_broadcast(&cond_);
    }

private:
    MutexLock& mutex_;
    pthread_cond_t cond_;
};

#endif // __linux__
#endif 
