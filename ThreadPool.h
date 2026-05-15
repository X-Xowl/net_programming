//
// Created by MY on 2026/5/15.
//

#ifndef UNTITLED10_THREADPOOL_H
#define UNTITLED10_THREADPOOL_H
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <vector>
#include <functional>
class ThreadPool
{
public:
    explicit ThreadPool(size_t thread_count)
    {
        for (size_t i = 0;i < thread_count;++i)
        {
            workers_.emplace_back([this]{worker_loop();});
        }
    }
    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker:workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }
    bool enqueue(std::function<void()> task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_)
        {
            return false;
        }
        tasks_.push(std::move(task));
        cv_.notify_one();
        return true;
    }

private:
    void worker_loop()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock,[this]{return stop_ || !tasks_.empty();});
                if (stop_ || tasks_.empty())
                {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};
#endif //UNTITLED10_THREADPOOL_H
