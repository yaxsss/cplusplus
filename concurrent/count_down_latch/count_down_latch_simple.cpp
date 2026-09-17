#include <mutex>
#include <condition_variable>


class CountDownLatch  {
public:
    CountDownLatch(int count) : count_(count) {}
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return count_ == 0; });
    }
    void countDown() {
        std::unique_lock<std::mutex> lock(mutex_);
        --count_;
        if (count_ == 0) {
            condition_.notify_all();
        }
    }
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    int count_;
};

