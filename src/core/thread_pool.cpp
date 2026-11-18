#include "thread_pool.hpp"

#include <stdexcept>

ThreadPool::ThreadPool(std::size_t worker_count)
    : stopping(false) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker_count must be at least 1");
    }

    workers.reserve(worker_count);

    try {
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers.emplace_back(&ThreadPool::workerLoop, this);
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stopping = true;
        }
        task_available.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        throw;
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            task_available.wait(lock, [this] {
                return stopping || !tasks.empty();
            });

            if (stopping && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop_front();
        }

        try {
            task();
        } catch (...) {
            // Submitted tasks use packaged_task, which stores their exceptions
            // in the associated future. Keep workers alive for any other error.
        }
    }
}

void ThreadPool::shutdown() {
    std::lock_guard<std::mutex> shutdown_lock(shutdown_mutex);
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stopping = true;
    }

    task_available.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
