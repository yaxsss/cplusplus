// ============================================================================
// POSIX pthread 版 MutexLock / MutexLockGuard
// 编译: g++ -std=c++17 -Wall -pthread main.cpp -o main && ./main
// ============================================================================
#ifdef __linux__
#include "mutex_lock.h"
#endif

int main() {
    MutexLock m;
    {
        MutexLockGuard lock(m);
        m.assertLocked();
    }
    return 0;
}