
#include "ollama_client.hpp"

#include "tokenizer.hpp"
#include "crow.h"
#include "httplib.h"

#include <algorithm>
#include <exception>
#include <cctype>
#include <unordered_set>

namespace Hallazgo_api {

namespace {

bool isValidExpansionToken(const std::string& token) {
    if (token.size() < 2) {
        return false; // відсікає одиночні літери на кшталт "s" від "'s"
    }

    const bool all_digits = std::all_of(
        token.begin(),
        token.end(),
        [](unsigned char ch) { return std::isdigit(ch); }
    );
    if (all_digits) {
        return false; // відсікає номери списку "1", "2", "3"...
    }

    static const std::unordered_set<std::string> stop_words = {
        "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
        "in", "into", "is", "it", "of", "on", "or", "that", "the", "to", "with"
    };

    return stop_words.find(token) == stop_words.end();
}

} // namespace

OllamaClient::OllamaClient(const std::string& host, int port)
    : host(host), port(port) {
    worker_thread = std::thread(&OllamaClient::processQueue, this);
}

OllamaClient::~OllamaClient() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stopping = true;
    }

    queue_condition.notify_one();

    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

ExpansionResult OllamaClient::expandQuery(
    const std::string& query,
    const std::vector<std::string>& original_tokens
) {
    auto request = std::make_shared<Request>();
    request->query = query;
    request->original_tokens = original_tokens;

    std::future<ExpansionResult> result = request->result.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        if (stopping) {
            ExpansionResult fallback;
            fallback.tokens = original_tokens;
            fallback.error = "Ollama request queue is stopping";
            return fallback;
        }

        request_queue.push(request);
    }

    queue_condition.notify_one();

    return result.get();
}

void OllamaClient::processQueue() {
    while (true) {
        std::shared_ptr<Request> request;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            queue_condition.wait(lock, [this] {
                return stopping || !request_queue.empty();
            });

            if (stopping && request_queue.empty()) {
                return;
            }

            request = request_queue.front();
            request_queue.pop();
        }

        ExpansionResult result = generateExpansion(
            request->query,
            request->original_tokens
        );

        request->result.set_value(std::move(result));
    }
}

ExpansionResult OllamaClient::generateExpansion(
    const std::string& query,
    const std::vector<std::string>& original_tokens
) {
    ExpansionResult expansion;
    expansion.tokens = original_tokens;

    try {
        httplib::Client client(host, port);

        client.set_connection_timeout(3, 0);
        client.set_read_timeout(60, 0);
        client.set_write_timeout(5, 0);

        crow::json::wvalue request;
        request["model"] = "qwen2.5:1.5b-instruct";
        request["prompt"] =
            "You are helping improve search in a collection of books. "
            "Generate up to 5 short alternative search phrases for the query. "
            "Use only synonyms, closely related concepts, and alternative "
            "ways to express the same topic. "
            "Keep the original meaning and topic. "
            "Do not add broad or loosely related concepts. "
            "Do not include explanations, introductions, or numbering. "
            "Return one phrase per line.\n"
            "Query: " + query;

        request["stream"] = false;
        request["keep_alive"] = "30s";
        request["options"]["temperature"] = 0.2;
        request["options"]["num_predict"] = 64;
        request["options"]["num_ctx"] = 512;

        auto response = client.Post(
            "/api/generate",
            request.dump(),
            "application/json"
        );

        if (!response) {
            expansion.error = "Could not connect to Ollama";
            return expansion;
        }

        if (response->status != 200) {
            expansion.error = "Ollama returned HTTP status "
                + std::to_string(response->status);
            return expansion;
        }

        auto result = crow::json::load(response->body);

        if (!result || !result.has("response") ||
            result["response"].t() != crow::json::type::String) {
            expansion.error = "Invalid Ollama response";
            return expansion;
        }

        const std::string generated_text = result["response"].s();

        if (generated_text.empty()) {
            expansion.error = "Ollama returned empty response";
            return expansion;
        }

        const auto generated_tokens = tokenize(generated_text);

        for (const auto& token : generated_tokens) {
            if (!isValidExpansionToken(token)) {
                continue;
            }

            if (std::find(
                expansion.tokens.begin(),
                expansion.tokens.end(),
                token
            ) == expansion.tokens.end()) {
                expansion.tokens.push_back(token);
                ++expansion.added_terms;
            }
        }

        expansion.used = expansion.added_terms > 0;

        if (!expansion.used) {
            expansion.error = "Ollama did not generate new search terms";
        }
    }
    catch (const std::exception& error) {
        expansion.error = error.what();
    }

    return expansion;
}

bool OllamaClient::isAvailable() {
    try {
        httplib::Client client(host, port);
        client.set_connection_timeout(2, 0);

        auto response = client.Get("/api/tags");

        return response && response->status == 200;
    }
    catch (...) {
        return false;
    }
}

}
