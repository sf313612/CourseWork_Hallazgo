
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

#include "keyword_search.hpp"
#include "metadata_store.hpp"
#include "parallel_indexer.hpp"
#include "ollama_client.hpp"

struct ApiState {
    explicit ApiState(
        const std::string& directory,
        bool enable_query_expansion = true
    )
        : ollama(
              std::getenv("OLLAMA_HOST")
                  ? std::getenv("OLLAMA_HOST")
                  : "127.0.0.1",
              std::getenv("OLLAMA_PORT")
                  ? std::stoi(std::getenv("OLLAMA_PORT"))
                  : 11434
          ),
          dataset_directory(directory),
          query_expansion_enabled(enable_query_expansion) {}

    ~ApiState() {
        if (indexing_thread.joinable()) {
            indexing_thread.join();
        }
    }

    Hallazgo_search::KeywordSearch search;
    Hallazgo_api::MetadataStore metadata;

    Hallazgo_api::OllamaClient ollama;

    std::string dataset_directory;
    bool query_expansion_enabled = true;

    std::mutex mutex;
    std::mutex indexing_start_mutex;
    std::thread indexing_thread;
    std::unique_ptr<Hallazgo_indexing::ParallelIndexer> indexer;

    bool indexing = false;
    std::string job_id;
    std::string last_error;
    std::uint64_t next_job_id = 1; 
    std::size_t thread_count = 0;
    std::string indexing_status = "null";

    std::chrono::steady_clock::time_point indexing_started_at;
    std::uint64_t index_build_time_ms = 0;
};
