#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class BlockingQueue {
public:
    BlockingQueue(size_t maxSize) : maxSize_(maxSize) {}
    void push(const T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return queue_.size() < maxSize_; });
        queue_.push(value);
        condition_.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty(); });
        T value = queue_.front();
        queue_.pop();
        condition_.notify_one();
        return value;
    }
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<T> queue_;
    size_t maxSize_;
};