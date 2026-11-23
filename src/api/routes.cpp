#include "routes.hpp"
#include "tokenizer.hpp"

#include <exception>
#include <string>
#include <chrono>
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

crow::response makeErrorResponse(
    int status,
    const std::string& code,
    const std::string& message
) {
    crow::json::wvalue body;
    body["error"]["code"] = code;
    body["error"]["message"] = message;

    crow::response response(status);
    response.set_header("Content-Type", "application/json");
    response.body = body.dump();

    return response;
}

struct SearchExecution {
    bool index_ready = true;
    crow::json::wvalue response;
};

SearchExecution performSearch(
    ApiState& state,
    const std::string& query_text,
    const std::function<void(const crow::json::wvalue&)>& sendProgress = {}
) {
    SearchExecution execution;

    const auto tokens = tokenize(query_text);

    // Перевіряємо готовність індексу.
    {
        std::lock_guard<std::mutex> lock(state.mutex);

        if (!state.search.isReady()) {
            execution.index_ready = false;
            return execution;
        }
    }

    const auto expansion_start = std::chrono::steady_clock::now();

    const auto expansion = state.ollama.expandQuery(query_text, tokens);

    const auto expansion_end = std::chrono::steady_clock::now();

    const auto expansion_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            expansion_end - expansion_start
        ).count();

    // Надсилаємо результат розширення запиту.
    if (sendProgress) {
        crow::json::wvalue message;
        message["stage"] = "query_expansion";
        message["data"]["original"] = query_text;

        crow::json::wvalue::list expanded_terms;

        for (const auto& term : expansion.tokens) {
            expanded_terms.push_back(term);
        }

        message["data"]["expanded_terms"] = std::move(expanded_terms);

        sendProgress(message);
    }

    const auto search_start = std::chrono::steady_clock::now();

    crow::json::wvalue::list results;
    crow::json::wvalue::list progress_results;      

    {
        std::lock_guard<std::mutex> lock(state.mutex);

        // Повторно перевіряємо готовність індексу.
        if (!state.search.isReady()) {
            execution.index_ready = false;
            return execution;
        }

        const auto search_results =
            state.search.search(expansion.tokens);

        const std::size_t result_count =
            std::min(search_results.size(), std::size_t(10));

        for (std::size_t i = 0; i < result_count; ++i) {
            const auto& item = search_results[i];

            crow::json::wvalue result_item;
            result_item["document_id"] = item.document_id;

            const std::string title =
                state.metadata.getTitle(item.document_id);

            result_item["title"] =
                title.empty() ? item.document_id : title;

            result_item["score"] = item.score;

            results.push_back(std::move(result_item));

            if (sendProgress) {
                crow::json::wvalue progress_item;
                progress_item["document_id"] = item.document_id;
                progress_item["title"] = title.empty() ? item.document_id : title;
                progress_item["score"] = item.score;

                progress_results.push_back(std::move(progress_item));
            }
        }
    }

    const auto search_end = std::chrono::steady_clock::now();

    const auto search_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            search_end - search_start
        ).count();

    if (sendProgress) {
        crow::json::wvalue message;
        message["stage"] = "keyword_search";

        message["data"]["results"] = std::move(progress_results);
        message["data"]["time_ms"] =
            static_cast<std::uint64_t>(search_time_ms);

        sendProgress(message);
    }

    execution.response["results"] = std::move(results);
    execution.response["query_expansion_time_ms"] =
        static_cast<std::uint64_t>(expansion_time_ms);

    crow::json::wvalue::list added_terms;

    for (const auto& term : expansion.tokens) {
        if (std::find(tokens.begin(), tokens.end(), term) == tokens.end()) {
            added_terms.push_back(term);
        }
    }

    execution.response["added_terms"] = std::move(added_terms);

    execution.response["search_time_ms"] =
        static_cast<std::uint64_t>(search_time_ms);
    execution.response["original_terms_count"] = tokens.size();
    execution.response["expanded_terms_count"] = expansion.tokens.size();
    execution.response["added_terms_count"] = expansion.added_terms;
    execution.response["query_expansion_used"] = expansion.used;
    execution.response["query_expansion_error"] = expansion.error;

    return execution;
}


bool startIndexing(
    ApiState& state,
    std::size_t workers,
    std::string& current_job_id
) {
    std::lock_guard<std::mutex> start_lock(state.indexing_start_mutex);

    {
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.indexing) {
            return false;
        }
    }

    if (state.indexing_thread.joinable()) {
        state.indexing_thread.join();
    }

    std::lock_guard<std::mutex> lock(state.mutex);

    state.indexer = std::make_unique<Hallazgo_indexing::ParallelIndexer>(
        state.search, workers
    );

    state.indexing_status = "started";
    state.indexing_started_at = std::chrono::steady_clock::now();
    state.index_build_time_ms = 0;
    state.indexing = true;
    state.thread_count = workers;
    state.last_error.clear();
    state.job_id = "job-" + std::to_string(state.next_job_id++);

    current_job_id = state.job_id;

    auto* indexer = state.indexer.get();

    state.indexing_thread = std::thread([&state, indexer]() {
        try {
            indexer->indexFiles(state.dataset_directory);

            std::lock_guard<std::mutex> lock(state.mutex);

            state.indexing = false;
            state.indexing_status = "completed";

            state.index_build_time_ms =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() -
                        state.indexing_started_at
                    ).count()
                );
        }
        catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(state.mutex);

            state.last_error = error.what();
            state.index_build_time_ms =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() -
                        state.indexing_started_at
                    ).count()
                );

            state.indexing_status = "error";
            state.indexing = false;

            std::cerr << "Indexing error: "
                      << state.last_error << std::endl;
        }
        catch (...) {
            std::lock_guard<std::mutex> lock(state.mutex);

            state.last_error = "Unknown indexing error";
            state.index_build_time_ms =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() -
                        state.indexing_started_at
                    ).count()
                );

            state.indexing_status = "error";
            state.indexing = false;

            std::cerr << state.last_error << std::endl;
        }
    });

    return true;
}

void registerRoutes(crow::SimpleApp& app, ApiState& state) {

    CROW_ROUTE(app, "/docs")
    ([] {
        const std::string html = R"(
            <!DOCTYPE html>
            <html>
            <head>
                <title>Hallazgo API</title>
                <link rel="stylesheet"
                    href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
            </head>
            <body>
                <div id="swagger-ui"></div>
                <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
                <script>
                    SwaggerUIBundle({
                        url: "/openapi.yaml",
                        dom_id: "#swagger-ui"
                    });
                </script>
            </body>
            </html>
        )";

        crow::response response(html);
        response.set_header("Content-Type", "text/html; charset=utf-8");
        return response;
    });

    CROW_ROUTE(app, "/openapi.yaml")
    ([] {
        std::ifstream file("docs/openapi.yaml", std::ios::binary);
        if (!file) {
            return crow::response(404, "OpenAPI specification not found");
        }

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        crow::response response(content);
        response.set_header("Content-Type", "application/yaml");
        return response;
    });

    CROW_ROUTE(app, "/api/health")
    ([&state] {
        crow::json::wvalue response;
        response["status"] = "ok";
        response["ollama"] = state.ollama.isAvailable();

        return response;
    });

    CROW_ROUTE(app, "/api/stats")
    ([&state] {
        std::lock_guard<std::mutex> lock(state.mutex);

        crow::json::wvalue response;

        if (state.indexing_status == "null") {
            response["indexing_status"] = nullptr;
        } else if (state.indexing && state.indexer &&
                state.indexer->getProcessedFiles() > 0) {
            response["indexing_status"] = "progress";
        } else {
            response["indexing_status"] = state.indexing_status;
        }

        response["processed_document_count"] =
            state.indexer ? state.indexer->getProcessedFiles() : 0;

        response["total_document_count"] =
            state.indexer ? state.indexer->getTotalFiles() : 0;

        if (state.indexing) {
            response["index_build_time_ms"] =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() -
                        state.indexing_started_at
                    ).count()
                );
        } else {
            response["index_build_time_ms"] = state.index_build_time_ms;
        }

        response["thread_count"] = state.thread_count;

        response["last_error"] = state.last_error;

        return response;
    });

    
    CROW_ROUTE(app, "/api/index").methods(crow::HTTPMethod::Post)
    ([&state](const crow::request& req) {
        auto body = crow::json::load(req.body);

        if (!body || !body.has("threads") ||
            body["threads"].t() != crow::json::type::Number ||
            body["threads"].d() < 1 ||
            body["threads"].d() != body["threads"].i()) {
            return makeErrorResponse(
                400, "invalid_request", "Invalid threads value"
            );
        }

        const std::size_t workers =
            static_cast<std::size_t>(body["threads"].i());

        std::string current_job_id;

        if (!startIndexing(state, workers, current_job_id)) {
            return makeErrorResponse(
                409,
                "indexing_in_progress",
                "Another indexing job is already running"
            );
        }

        crow::json::wvalue response;
        response["job_id"] = current_job_id;

        crow::response result(202);
        result.set_header("Content-Type", "application/json");
        result.body = response.dump();

        return result;
    });

    CROW_ROUTE(app, "/api/search").methods(crow::HTTPMethod::Post)
    ([&state](const crow::request& req) {
        auto body = crow::json::load(req.body);

        if (!body || !body.has("query") || !body.has("mode") ||
            body["query"].t() != crow::json::type::String ||
            body["mode"].t() != crow::json::type::String) {
            return makeErrorResponse(
                400, "invalid_request", "Invalid search request"
            );
        }

        const std::string query_text = body["query"].s();
        const std::string mode = body["mode"].s();

        if (mode != "keyword" || query_text.empty()) {
            return makeErrorResponse(
                400, "invalid_request", "Invalid search request"
            );
        }

        const std::vector<std::string> tokens = tokenize(query_text);

        if (tokens.empty()) {
            return makeErrorResponse(
                400, "invalid_request", "Query contains no searchable words"
            );
        }

        const auto execution = performSearch(state, query_text);

        if (!execution.index_ready) {
            return makeErrorResponse(
                503,
                "index_not_ready",
                "Indexing is in progress or no successfully built index is available"
            );
        }

        crow::response result(200);
        result.set_header("Content-Type", "application/json");
        result.body = execution.response.dump();

        return result;

    });


    CROW_WEBSOCKET_ROUTE(app, "/ws/search")
    .onopen([](crow::websocket::connection& conn) {
        crow::json::wvalue message;
        message["type"] = "connected";
        message["message"] = "WebSocket connection established";
        conn.send_text(message.dump());
    })
    .onmessage([&state](
        crow::websocket::connection& conn,
        const std::string& data,
        bool is_binary
    ) {
        if (is_binary) {
            crow::json::wvalue message;
            message["type"] = "error";
            message["message"] = "Binary messages are not supported";
            conn.send_text(message.dump());
            return;
        }

        auto body = crow::json::load(data);

        if (!body || !body.has("query") ||
            body["query"].t() != crow::json::type::String) {
            crow::json::wvalue message;
            message["type"] = "error";
            message["message"] = "Invalid search request";
            conn.send_text(message.dump());
            return;
        }

        const std::string query_text = body["query"].s();

        if (tokenize(query_text).empty()) {
            crow::json::wvalue message;
            message["type"] = "error";
            message["message"] = "Query contains no searchable words";
            conn.send_text(message.dump());
            return;
        }

        const auto execution = performSearch(
            state,
            query_text,
            [&conn](const crow::json::wvalue& message) {
                conn.send_text(message.dump());
            }
        );

        if (!execution.index_ready) {
            crow::json::wvalue message;
            message["type"] = "error";
            message["message"] = "Index is not ready";
            conn.send_text(message.dump());
            return;
        }

        crow::json::wvalue message;
        message["stage"] = "done";

        conn.send_text(message.dump());
    })
    .onclose([](
        crow::websocket::connection&,
        const std::string&,
        uint16_t
    ) {
    })
    .onerror([](
        crow::websocket::connection&,
        const std::string&
    ) {
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws/index")
    .onopen([](crow::websocket::connection& conn) {
        crow::json::wvalue message;
        message["type"] = "connected";
        message["message"] = "Index WebSocket connection established";

        conn.send_text(message.dump());
    })
    .onmessage([&state](
        crow::websocket::connection& conn,
        const std::string& data,
        bool is_binary
    ) {
        if (is_binary) {
            crow::json::wvalue message;
            message["stage"] = "error";
            message["data"]["message"] =
                "Binary messages are not supported";

            conn.send_text(message.dump());
            return;
        }

        auto body = crow::json::load(data);

        if (!body) {
            crow::json::wvalue message;
            message["stage"] = "error";
            message["data"]["message"] = "Invalid request";

            conn.send_text(message.dump());
            return;
        }

        // Запуск індексації.
        if (body.has("threads")) {
            if (body["threads"].t() != crow::json::type::Number ||
                body["threads"].d() < 1 ||
                body["threads"].d() != body["threads"].i()) {

                crow::json::wvalue message;
                message["stage"] = "error";
                message["data"]["message"] = "Invalid threads value";

                conn.send_text(message.dump());
                return;
            }

            const std::size_t workers =
                static_cast<std::size_t>(body["threads"].i());

            std::string current_job_id;

            if (!startIndexing(state, workers, current_job_id)) {
                crow::json::wvalue message;
                message["stage"] = "error";
                message["data"]["message"] =
                    "Another indexing job is already running";

                conn.send_text(message.dump());
                return;
            }

            crow::json::wvalue message;
            message["stage"] = "indexing_started";
            message["data"]["job_id"] = current_job_id;
            message["data"]["threads"] = workers;

            conn.send_text(message.dump());
            return;
        }

        // Запит поточного прогресу.
        if (body.has("action") &&
            body["action"].t() == crow::json::type::String &&
            body["action"].s() == "progress") {

            crow::json::wvalue message;

            {
                std::lock_guard<std::mutex> lock(state.mutex);

                const std::size_t processed =
                    state.indexer
                        ? state.indexer->getProcessedFiles()
                        : 0;

                const std::size_t total =
                    state.indexer
                        ? state.indexer->getTotalFiles()
                        : 0;

                const int progress =
                    total > 0
                        ? static_cast<int>(processed * 100 / total)
                        : 0;

                message["data"]["job_id"] = state.job_id;
                message["data"]["processed_documents"] = processed;
                message["data"]["total_documents"] = total;
                message["data"]["progress_percent"] = progress;

                if (state.indexing) {
                    message["stage"] = "indexing_progress";
                }
                else if (state.indexing_status == "completed") {
                    message["stage"] = "done";
                    message["data"]["time_ms"] =
                        state.index_build_time_ms;
                }
                else if (state.indexing_status == "error") {
                    message["stage"] = "error";
                    message["data"]["message"] = state.last_error;
                }
                else {
                    message["stage"] = "idle";
                }
            }

            conn.send_text(message.dump());
            return;
        }

        crow::json::wvalue message;
        message["stage"] = "error";
        message["data"]["message"] = "Unknown request";

        conn.send_text(message.dump());
    })
    .onclose([](
        crow::websocket::connection&,
        const std::string&,
        uint16_t
    ) {
    })
    .onerror([](
        crow::websocket::connection&,
        const std::string&
    ) {
    });

}