#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <functional> 
#include <future> 
#include <mutex> 
#include <queue> 
#include <thread> 
#include <utility> 
#include <vector>
#include "thread_safe_queue.hpp"

class ThreadPool
{
public:
    ThreadPool(const int max_number_of_threads) : threads_(std::vector<std::thread>(max_number_of_threads)) { }

    void initialize()
    {
        for (size_t i = 0; i < threads_.size(); ++i) {
            threads_.at(i) = std::thread(Worker(this, i));
        } 
    }

    void shutdown()
    {
        shutdown_ = true; 
        conditional_lock_.notify_all(); // Wakeup all worker.
        for (size_t i = 0; i < threads_.size(); ++i) {
            if (threads_.at(i).joinable())
                threads_.at(i).join();
        }
    }

    template<typename FUNCTION, typename...Args>
    auto submit(FUNCTION &&f, Args&&... args) -> std::future<decltype(f(args...))>
    {
        // Create a function with bounded parameters ready to execute.
        std::function<decltype(f(args...))()> func = std::bind(std::forward<FUNCTION>(f), std::forward<Args>(args)...);

        // Encapsulate it into a shared ptr in order to be able to copy construct
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args ...))()>>(func);

        // Wrap packaged task into void function
        std::function<void()> wrapper_func = [task_ptr]() { (*task_ptr)(); };

        queue_.enqueue(wrapper_func);
        
        // Weakup a thread whitch was waitting.
        conditional_lock_.notify_one();

        return task_ptr->get_future();
    }

private:
    bool shutdown_ = false;
    ThreadSafeQueue<std::function<void()>> queue_;
    std::vector<std::thread> threads_;
    std::mutex conditional_mutex_;
    std::condition_variable conditional_lock_;

private:
    class Worker
    {
        public:
            Worker(ThreadPool *pool, const int id):id_(id), pool_(pool) { }

            //overload operator '()'
            void operator()()
            {
                std::function<void()> func;    
                bool dequeued;

                // If the thread pool is not shutdown, repeat get task.
                while (!pool_->shutdown_) {
                    {
                        std::unique_lock<std::mutex> lock(pool_->conditional_mutex_);
                        if (pool_->queue_.empty()) {
                            // Waitting for pool notify me to ready to dequeue and run task.
                            pool_->conditional_lock_.wait(lock);
                        }
                        dequeued = pool_->queue_.dequeue(func);
                    }
                    if (dequeued)
                        func();
                }
            }

        private:
            int id_;
            ThreadPool *pool_;
    };
};



#endif //_THREAD_POOL_H_
