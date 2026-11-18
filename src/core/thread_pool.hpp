#pragma once

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    template <typename Callable>
    std::future<typename std::result_of<Callable()>::type> submit(Callable&& task) {
        typedef typename std::result_of<Callable()>::type Result;
        std::shared_ptr<std::packaged_task<Result()> > packaged_task(
            new std::packaged_task<Result()>(std::forward<Callable>(task))
        );
        std::future<Result> result = packaged_task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stopping) {
                throw std::runtime_error("cannot submit a task after shutdown");
            }
            tasks.push_back([packaged_task]() { (*packaged_task)(); });
        }

        task_available.notify_one();
        return result;
    }

    void shutdown();

private:
    void workerLoop();

    std::deque<std::function<void()> > tasks;
    std::vector<std::thread> workers;
    std::mutex queue_mutex;
    std::mutex shutdown_mutex;
    std::condition_variable task_available;
    bool stopping;
};
