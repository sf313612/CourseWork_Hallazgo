#include "routes.hpp"
#include "tokenizer.hpp"

#include <exception>
#include <string>
#include <chrono>
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>

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

    CROW_ROUTE(app, "/api/health")([] {
        crow::json::wvalue response;
        response["status"] = "ok";
        response["ollama"] = false;

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
            {
                std::lock_guard<std::mutex> lock(state.mutex);

                if (state.indexing) {
                    return makeErrorResponse(
                        409, "indexing_in_progress", "Another indexing job is already running"
                    );
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

            const std::string current_job_id = state.job_id;
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
                                std::chrono::steady_clock::now() - state.indexing_started_at
                            ).count()
                        );
                }
                catch (const std::exception& error) {
                    std::lock_guard<std::mutex> lock(state.mutex);

                    state.last_error = error.what();

                    std::cerr << "Indexing error: "
                            << state.last_error << std::endl;

                    state.index_build_time_ms =
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() -
                                state.indexing_started_at
                            ).count()
                        );

                    state.indexing_status = "error";
                    state.indexing = false;
                }
                catch (...) {
                    std::lock_guard<std::mutex> lock(state.mutex);

                    state.last_error = "Unknown indexing error";

                    std::cerr << state.last_error << std::endl;

                    state.index_build_time_ms =
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() -
                                state.indexing_started_at
                            ).count()
                        );

                    state.indexing_status = "error";
                    state.indexing = false;
                }
            });

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
            body["mode"].t() != crow::json::type::String ||
            body["mode"].s() != "keyword" ||
            body["query"].s().size() == 0) {
            return makeErrorResponse(
                400, "invalid_request", "Invalid search request"
            );
        }

        const std::string query_text = body["query"].s();
        const std::vector<std::string> tokens = tokenize(query_text);

        if (tokens.empty()) {
            return makeErrorResponse(
                400, "invalid_request", "Query contains no searchable words"
            );
        }

        // Prevent starting a new index operation during this search.
        std::lock_guard<std::mutex> lock(state.mutex);

        if (!state.search.isReady()) {
            return makeErrorResponse(
                503,
                "index_not_ready",
                "Indexing is in progress or no successfully built index is available"
            );
        }

        const auto start_time = std::chrono::steady_clock::now();

        const auto search_results = state.search.search(tokens);

        const auto end_time = std::chrono::steady_clock::now();

        const auto search_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time
            ).count();

        crow::json::wvalue response;
        crow::json::wvalue::list results;

        const std::size_t result_limit = 10;
        const std::size_t result_count = std::min(search_results.size(), result_limit);

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
        }

        response["results"] = std::move(results);
        response["search_time_ms"] =
            static_cast<std::uint64_t>(search_time_ms);

        crow::response result(200);
        result.set_header("Content-Type", "application/json");
        result.body = response.dump();

        return result;
    });

}