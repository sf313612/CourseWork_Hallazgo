#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "api_state.hpp"
#include "crow.h"
#include "httplib.h"
#include "ollama_client.hpp"
#include "routes.hpp"

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                          \
                      << ": CHECK failed: " #expression << '\n';              \
            return false;                                                     \
        }                                                                     \
    } while (false)

namespace {
constexpr int TEST_PORT = 18081;
const std::string TEST_HOST = "127.0.0.1";

class TestServer {
public:
    TestServer(
        const std::string& dataset_dir,
        const std::string& metadata_path,
        bool enable_query_expansion = true
    )
        : state_(dataset_dir, enable_query_expansion) {
        registerRoutes(app_, state_);

        server_thread_ = std::thread([this]() {
            app_.port(TEST_PORT).multithreaded().run();
        });

        httplib::Client client(TEST_HOST, TEST_PORT);
        client.set_connection_timeout(1, 0);
        client.set_read_timeout(1, 0);

        bool ready = false;
        for (int i = 0; i < 50; ++i) {
            auto response = client.Get("/api/stats");
            if (response && response->status == 200) {
                ready = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!ready) {
            app_.stop();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            throw std::runtime_error("Test HTTP server did not start");
        }
    }

    ~TestServer() {
        app_.stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    ApiState& state() {
        return state_;
    }

private:
    ApiState state_;
    crow::SimpleApp app_;
    std::thread server_thread_;
};

struct TestDatasetFixture {
    std::string dataset_dir = "test_dataset_temp";
    std::string metadata_path = "test_dataset_temp/metadata.csv";

    TestDatasetFixture() {
        std::filesystem::remove_all(dataset_dir);
        std::filesystem::create_directories(dataset_dir);

        std::ofstream metadata(metadata_path);
        metadata << "doc1,The Odyssey of Homer,800,http://example.com/1\n";
        metadata << "doc2,The Iliad of Homer,850,http://example.com/2\n";
        metadata << "doc3,Don Quixote,1605,http://example.com/3\n";
        metadata.close();

        std::ofstream doc1(dataset_dir + "/doc1.txt");
        doc1 << "ancient greek epic poem about odysseus journey home "
                "after troy dragon magic";
        doc1.close();

        std::ofstream doc2(dataset_dir + "/doc2.txt");
        doc2 << "ancient greek epic poem about the trojan war "
                "achilles wall of troy";
        doc2.close();

        std::ofstream doc3(dataset_dir + "/doc3.txt");
        doc3 << "spanish novel about quixote knight adventure "
                "windmill dragon";
        doc3.close();
    }

    ~TestDatasetFixture() {
        std::filesystem::remove_all(dataset_dir);
    }
};

bool testApiHealth() {
    TestDatasetFixture fixture;
    TestServer server(fixture.dataset_dir, fixture.metadata_path);

    httplib::Client client(TEST_HOST, TEST_PORT);
    auto response = client.Get("/api/health");

    CHECK(response && response->status == 200);

    auto body = crow::json::load(response->body);
    CHECK(body);
    CHECK(body.has("status"));
    CHECK(body["status"].s() == "ok");
    CHECK(body.has("ollama"));
    CHECK(body["ollama"].t() == crow::json::type::True ||
          body["ollama"].t() == crow::json::type::False);

    return true;
}

bool testApiStatsInitialState() {
    TestDatasetFixture fixture;
    TestServer server(fixture.dataset_dir, fixture.metadata_path);

    httplib::Client client(TEST_HOST, TEST_PORT);
    auto response = client.Get("/api/stats");

    CHECK(response && response->status == 200);

    auto body = crow::json::load(response->body);

    CHECK(body);
    CHECK(body.has("documents"));
    CHECK(body.has("index_build_time_ms"));
    CHECK(body.has("threads_used"));

    CHECK(body["documents"].i() == 0);
    CHECK(body["index_build_time_ms"].i() == 0);
    CHECK(body["threads_used"].i() == 0);

    // Старі поля не повинні входити до публічного API.
    CHECK(!body.has("indexing_status"));
    CHECK(!body.has("processed_document_count"));
    CHECK(!body.has("total_document_count"));
    CHECK(!body.has("thread_count"));
    CHECK(!body.has("last_error"));

    return true;
}

bool testApiIndexingAndSearchLifecycle() {
    TestDatasetFixture fixture;
    TestServer server(fixture.dataset_dir, fixture.metadata_path);

    httplib::Client client(TEST_HOST, TEST_PORT);

    crow::json::wvalue search_request;
    search_request["query"] = "dragon";
    search_request["mode"] = "keyword";

    // Searching before indexing must be rejected.
    auto response = client.Post(
        "/api/search", search_request.dump(), "application/json");
    CHECK(response && response->status == 503);

    // Start indexing and wait for completion.
    crow::json::wvalue index_request;
    index_request["threads"] = 2;

    response = client.Post(
        "/api/index", index_request.dump(), "application/json");

    CHECK(response && response->status == 200);

    auto index_body = crow::json::load(response->body);
    CHECK(index_body);
    CHECK(index_body.has("status"));
    CHECK(index_body["status"].s() == "ok");
    CHECK(index_body.has("documents_indexed"));
    CHECK(index_body["documents_indexed"].i() == 3);
    CHECK(index_body.has("time_ms"));
    
    response = client.Get("/api/stats");
    CHECK(response && response->status == 200);

    auto stats_body = crow::json::load(response->body);
    CHECK(stats_body);
    CHECK(stats_body["documents"].i() == 3);
    CHECK(stats_body["threads_used"].i() == 2);
    CHECK(stats_body["index_build_time_ms"].i() > 0);

    // Search after indexing.
    response = client.Post(
        "/api/search", search_request.dump(), "application/json");
    CHECK(response && response->status == 200);

    auto search_body = crow::json::load(response->body);
    CHECK(search_body);
    CHECK(search_body.has("results"));
    CHECK(search_body["results"].t() == crow::json::type::List);
    CHECK(search_body["results"].size() > 0);
    
    CHECK(search_body.has("time_ms"));

    const auto& first_result = search_body["results"][0];

    CHECK(first_result.has("doc_id"));
    CHECK(first_result.has("score"));
    CHECK(first_result.has("snippet"));

    // Invalid search mode must be rejected.
    crow::json::wvalue invalid_mode_request;
    invalid_mode_request["query"] = "dragon";
    invalid_mode_request["mode"] = "invalid_mode";

    response = client.Post(
        "/api/search", invalid_mode_request.dump(), "application/json");
    CHECK(response && response->status == 400);

    // A query with no searchable tokens must be rejected.
    crow::json::wvalue empty_query_request;
    empty_query_request["query"] = "   !!! ??? ";
    empty_query_request["mode"] = "keyword";

    response = client.Post(
        "/api/search", empty_query_request.dump(), "application/json");
    CHECK(response && response->status == 400);

    return true;
}

bool testApiInvalidIndexingRequests() {
    TestDatasetFixture fixture;
    TestServer server(fixture.dataset_dir, fixture.metadata_path);

    httplib::Client client(TEST_HOST, TEST_PORT);

    crow::json::wvalue zero_threads_request;
    zero_threads_request["threads"] = 0;

    auto response = client.Post(
        "/api/index", zero_threads_request.dump(), "application/json");
    CHECK(response && response->status == 400);

    crow::json::wvalue missing_threads_request;
    missing_threads_request["invalid_field"] = 5;

    response = client.Post(
        "/api/index", missing_threads_request.dump(), "application/json");
    CHECK(response && response->status == 400);

    return true;
}

bool testConcurrentClientRequests() {
    TestDatasetFixture fixture;
    TestServer server(
        fixture.dataset_dir,
        fixture.metadata_path,
        false
    );

    // Prepare a small searchable index directly, so this test focuses
    // on concurrent HTTP requests rather than indexing.
    server.state().search.beginIndexing();
    server.state().search.addDocument(
        "doc1", {"ancient", "greek", "epic", "dragon"});
    server.state().search.addDocument(
        "doc2", {"spanish", "novel", "quixote", "dragon"});
    server.state().search.finishIndexing(true);

    constexpr int client_threads_count = 8;
    constexpr int requests_per_client = 20;

    std::atomic<int> success_count{0};
    std::vector<std::thread> client_threads;

    for (int i = 0; i < client_threads_count; ++i) {
        client_threads.emplace_back([&success_count, requests_per_client]() {
            httplib::Client client(TEST_HOST, TEST_PORT);
            client.set_connection_timeout(2, 0);
            client.set_read_timeout(10, 0);

            for (int request_index = 0;
                 request_index < requests_per_client;
                 ++request_index) {
                if (request_index % 2 == 0) {
                    crow::json::wvalue request;
                    request["query"] = "dragon";
                    request["mode"] = "keyword";

                    auto response = client.Post(
                        "/api/search", request.dump(), "application/json");

                    if (response && response->status == 200) {
                        ++success_count;
                    } else {
                        std::cerr << "Search request failed: "
                                << (response ? std::to_string(response->status)
                                            : "no response")
                                << '\n';
                    }
                } else {
                    auto response = client.Get("/api/stats");

                    if (response && response->status == 200) {
                        ++success_count;
                    } else {
                        std::cerr << "Stats request failed: "
                                << (response ? std::to_string(response->status)
                                            : "no response")
                                << '\n';
                    }
                }
            }
        });
    }

    for (auto& thread : client_threads) {
        thread.join();
    }

    CHECK(success_count.load() ==
          client_threads_count * requests_per_client);

    return true;
}

bool testWebSocketRouteRejectsPlainHttpRequest() {
    TestDatasetFixture fixture;
    TestServer server(fixture.dataset_dir, fixture.metadata_path);

    // This is deliberately only a route/handshake rejection check.
    // It does NOT test a complete WebSocket connection or its messages.
    httplib::Client client(TEST_HOST, TEST_PORT);
    auto response = client.Get("/ws/search");

    CHECK(response);
    CHECK(response->status == 404);

    return true;
}

bool testOllamaClientFallbackWhenUnavailable() {
    Hallazgo_api::OllamaClient ollama("127.0.0.1", 59999);

    CHECK(!ollama.isAvailable());

    const std::vector<std::string> original_tokens = {"dragon", "magic"};
    auto result = ollama.expandQuery("dragon magic", original_tokens);

    CHECK(result.tokens == original_tokens);
    CHECK(!result.used);
    CHECK(result.added_terms == 0);
    CHECK(!result.error.empty());

    return true;
}

struct TestCase {
    const char* name;
    bool (*run)();
};
} // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"GET /api/health", testApiHealth},
        {"GET /api/stats initial state", testApiStatsInitialState},
        {"REST API indexing and search lifecycle",
         testApiIndexingAndSearchLifecycle},
        {"POST /api/index validation", testApiInvalidIndexingRequests},
        {"Concurrent HTTP client requests", testConcurrentClientRequests},
        {"WebSocket route rejects plain HTTP request",
         testWebSocketRouteRejectsPlainHttpRequest},
        {"Ollama fallback handling", testOllamaClientFallbackWhenUnavailable},
    };

    int failures = 0;

    for (const TestCase& test : tests) {
        try {
            if (test.run()) {
                std::cout << "PASS: " << test.name << '\n';
            } else {
                std::cerr << "FAIL: " << test.name << '\n';
                ++failures;
            }
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name
                      << " (exception: " << error.what() << ")\n";
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}
