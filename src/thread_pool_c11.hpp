#include <thread>
#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <future>
#include <memory>


class ThreadPoll
{
public:
    ThreadPoll(std::size_t thread_size = (std::thread::hardware_concurrency() == 0 ? 1 : std::thread::hardware_concurrency()))
    {
        printf("cpu cores:%lu\n", thread_size);
        // create workers
        for (size_t i = 0; i < thread_size; ++i) {
            auto worker = [this] {
                //1. wait
                //2. get task
                //3. excute until stop and all tasks done
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->mu_);
                        this->cond_.wait(lock, [this]{ return this->stop_ || ! this->tasks_.empty(); });

                        if (this->tasks_.empty() && this->stop_)
                            return;

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    task();
                }
            };

            this->workers_.emplace_back(worker);
        }
    }

    // commit
    template<typename F, typename ...Args>
    auto commit(F &&function, Args&& ...args) -> std::future<typename std::result_of<F(Args...)>::type>
    {
        // package function
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(function), std::forward<Args>(args)...));
        std::future<return_type> result = task->get_future();

        // insert function to tasks queue
        {
            std::lock_guard<std::mutex> lock(this->mu_);
            if (this->stop_)
                throw std::runtime_error("commit on stopped thread poll!");
            this->tasks_.emplace([task]{ (*task)(); });
        }

        // notify worker to do tasks
        this->cond_.notify_one();
        return result;
    }

    // destory
    ~ThreadPoll()
    {
        // stop workers
        {
            std::lock_guard<std::mutex> lock(this->mu_);
            this->stop_ = true;
        }
        // wakeup all worker
        this->cond_.notify_all();

        // join all tasks
        for (auto &it : this->workers_)
            if (it.joinable()) it.join();

    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mu_;
    std::condition_variable cond_;
    bool stop_ { false };
};
