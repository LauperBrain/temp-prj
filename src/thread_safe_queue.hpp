#ifndef _THREAD_SAFE_THREAD_SAFE_QUEUE_HPP_
#define _THREAD_SAFE_THREAD_SAFE_QUEUE_HPP_


#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue {
private:
    using const_type_ref = const T &;
    using type_ref = T &;

public:
    ThreadSafeQueue() { }
    ThreadSafeQueue(const ThreadSafeQueue &other) = delete;
    void operator=(const ThreadSafeQueue &other) = delete;
    ~ThreadSafeQueue() { }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void enqueue(const_type_ref t)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(t);
    }

    bool dequeue(type_ref t)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;

        t = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
};

#endif //_THREAD_SAFE_THREAD_SAFE_QUEUE_HPP_
