#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

#include "keyword_search.hpp"
#include "metadata_store.hpp"
#include "parallel_indexer.hpp"

struct ApiState {
    explicit ApiState(const std::string& directory)
        : dataset_directory(directory) {}

    ~ApiState() {
        if (indexing_thread.joinable()) {
            indexing_thread.join();
        }
    }

    Hallazgo_search::KeywordSearch search;
    Hallazgo_api::MetadataStore metadata;

    std::string dataset_directory;

    std::mutex mutex;
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
