#include <exception>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "thread_pool.hpp"

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": CHECK failed: " #expression << '\n';             \
            return false;                                                     \
        }                                                                     \
    } while (false)

bool testZeroWorkersAreRejected() {
    bool rejected = false;
    try {
        ThreadPool pool(0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    return true;
}

bool testTaskExecutionReturnsResult() {
    ThreadPool pool(1);
    std::future<int> result = pool.submit([] { return 42; });
    CHECK(result.get() == 42);
    pool.shutdown();
    return true;
}

bool testGracefulShutdownCompletesAcceptedTasks() {
    ThreadPool pool(1);
    std::future<int> first = pool.submit([] { return 1; });
    std::future<int> second = pool.submit([] { return 2; });

    pool.shutdown();

    CHECK(first.get() == 1);
    CHECK(second.get() == 2);
    return true;
}

bool testSubmissionAfterShutdownIsRejected() {
    ThreadPool pool(1);
    pool.shutdown();

    bool rejected = false;
    try {
        std::future<int> result = pool.submit([] { return 1; });
        result.get();
    } catch (const std::exception&) {
        rejected = true;
    }
    CHECK(rejected);
    return true;
}

bool testTaskExceptionPropagatesThroughFuture() {
    ThreadPool pool(1);
    std::future<int> result = pool.submit([]() -> int {
        throw std::runtime_error("task failed");
    });

    bool propagated = false;
    try {
        result.get();
    } catch (const std::runtime_error& error) {
        propagated = error.what() == std::string("task failed");
    } catch (...) {
        propagated = false;
    }
    pool.shutdown();
    CHECK(propagated);
    return true;
}

bool testWorkerContinuesAfterTaskException() {
    ThreadPool pool(1);
    std::future<void> failed = pool.submit([] {
        throw std::runtime_error("task failed");
    });
    std::future<int> succeeding = pool.submit([] { return 7; });

    bool exception_propagated = false;
    try {
        failed.get();
    } catch (const std::runtime_error&) {
        exception_propagated = true;
    }
    CHECK(exception_propagated);
    CHECK(succeeding.get() == 7);
    pool.shutdown();
    return true;
}

bool testShutdownCanBeCalledTwice() {
    ThreadPool pool(1);
    pool.shutdown();

    bool threw = false;
    try {
        pool.shutdown();
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);
    return true;
}

bool testMultipleWorkersExecuteTasksConcurrently() {
    ThreadPool pool(2);

    std::mutex mutex;
    std::condition_variable condition;
    int started = 0;
    bool release = false;

    auto task = [&]() {
        std::unique_lock<std::mutex> lock(mutex);
        ++started;
        condition.notify_all();

        condition.wait(lock, [&]() {
            return release;
        });

        return true;
    };

    std::future<bool> first = pool.submit(task);
    std::future<bool> second = pool.submit(task);

    bool both_started = false;

    {
        std::unique_lock<std::mutex> lock(mutex);
        both_started = condition.wait_for(
            lock,
            std::chrono::seconds(2),
            [&]() { return started == 2; }
        );

        release = true;
    }

    condition.notify_all();

    CHECK(first.get());
    CHECK(second.get());
    CHECK(both_started);

    pool.shutdown();
    return true;
}

struct TestCase {
    const char* name;
    bool (*run)();
};

int main(int argc, char* argv[]) {
    const TestCase tests[] = {
        {"zero workers are rejected", testZeroWorkersAreRejected},
        {"task execution returns result", testTaskExecutionReturnsResult},
        {"graceful shutdown completes accepted tasks", testGracefulShutdownCompletesAcceptedTasks},
        {"submission after shutdown is rejected", testSubmissionAfterShutdownIsRejected},
        {"task exception propagates through future", testTaskExceptionPropagatesThroughFuture},
        {"worker continues after task exception", testWorkerContinuesAfterTaskException},
        {"shutdown can be called twice", testShutdownCanBeCalledTwice},
        {"multiple workers execute tasks concurrently", testMultipleWorkersExecuteTasksConcurrently},
    };

    if (argc > 2) {
        std::cerr << "Usage: thread_pool_test [test name]\n";
        return 2;
    }

    if (argc == 2) {
        for (const TestCase& test : tests) {
            if (std::string(argv[1]) == test.name) {
                if (test.run()) {
                    std::cout << "PASS: " << test.name << '\n';
                    return 0;
                }
                std::cerr << "FAIL: " << test.name << '\n';
                return 1;
            }
        }

        std::cerr << "Unknown test: " << argv[1] << '\n';
        return 2;
    }

    bool all_passed = true;
    for (const TestCase& test : tests) {
        if (test.run()) {
            std::cout << "PASS: " << test.name << '\n';
        } else {
            std::cerr << "FAIL: " << test.name << '\n';
            all_passed = false;
        }
    }

    return all_passed ? 0 : 1;
}
