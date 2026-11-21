
#pragma once

#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace Hallazgo_api {

struct ExpansionResult {
    std::vector<std::string> tokens;
    bool used = false;
    std::size_t added_terms = 0;
    std::string error;
};

class OllamaClient {
public:
    OllamaClient(const std::string& host = "127.0.0.1", int port = 11434);
    ~OllamaClient();

    OllamaClient(const OllamaClient&) = delete;
    OllamaClient& operator=(const OllamaClient&) = delete;

    ExpansionResult expandQuery(
        const std::string& query,
        const std::vector<std::string>& original_tokens
    );

    bool isAvailable();

private:
    struct Request {
        std::string query;
        std::vector<std::string> original_tokens;
        std::promise<ExpansionResult> result;
    };

    void processQueue();
    ExpansionResult generateExpansion(
        const std::string& query,
        const std::vector<std::string>& original_tokens
    );

    std::string host;
    int port;

    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    std::queue<std::shared_ptr<Request>> request_queue;

    bool stopping = false;
    std::thread worker_thread;
};

}
