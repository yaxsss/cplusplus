#ifdef __linux__
#include <pthread.h>
#include <unistd.h>
#include <cassert>
#include <cstddef>

class MutexLock {
public:
    MutexLock(): holder_(0) {
        pthread_mutex_init(&mutex_, NULL);
    }
    ~MutexLock() {
        pthread_mutex_destroy(&mutex_);
    }
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;
    bool isLockedByThisThread() {
        return holder_ == pthread_self();
    }
    void assertLocked() {
        assert(isLockedByThisThread());
    }
    void lock() {
        pthread_mutex_lock(&mutex_);
        holder_ = pthread_self();
    }
    void unlock() {
        holder_ = 0;
        pthread_mutex_unlock(&mutex_);
    }
    pthread_mutex_t* getPthreadMutex() {
        return &mutex_;
    }

private:
    pthread_mutex_t mutex_;
    pid_t holder_;
};


#endif