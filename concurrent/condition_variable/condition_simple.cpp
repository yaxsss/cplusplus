// ============================================================================
// Condition：按平台适配（pthread_cond / CONDITION_VARIABLE）
// Linux:   g++ -std=c++17 -Wall -pthread condition_simple.cpp -o condition_simple
// Windows: cl /EHsc /std:c++17 /I. condition_simple.cpp
// ============================================================================
#include "condition.h"

int main() {
    MutexLock mutex;
    Condition cond(mutex);
    {
        MutexLockGuard lock(mutex);
        mutex.assertLocked();
        cond.notify_one();
        cond.notify_all();
    }
    return 0;
}
