
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <future>
#include <stdexcept>
#include <filesystem>
#include <fstream>

#include "parallel_indexer.hpp"
#include "keyword_search.hpp"

using namespace Hallazgo_indexing;
using namespace Hallazgo_search;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::cerr << "CHECK failed: " #expression << '\n'; \
            return false; \
        } \
    } while (false)

bool testSameResultsWithDifferentWorkerCounts() {
    const std::vector<Document> documents = {
        {"doc1", "dragon castle magic sword"},
        {"doc2", "dragon mountain adventure"},
        {"doc3", "castle king kingdom"},
        {"doc4", "magic dragon wizard"},
        {"doc5", "space ship galaxy"},
        {"doc6", "king sword castle"}
    };

    KeywordSearch single_search;
    KeywordSearch parallel_search;

    ParallelIndexer single_indexer(single_search, 1);
    ParallelIndexer parallel_indexer(parallel_search, 4);

    single_indexer.indexDocuments(documents);
    parallel_indexer.indexDocuments(documents);

    const std::vector<SearchResult> single_results =
        single_search.search({"dragon", "castle"});

    const std::vector<SearchResult> parallel_results =
        parallel_search.search({"dragon", "castle"});

    CHECK(single_results.size() == parallel_results.size());

    for (std::size_t i = 0; i < single_results.size(); ++i) {
        CHECK(single_results[i].document_id ==
              parallel_results[i].document_id);

        CHECK(std::abs(single_results[i].score -
                       parallel_results[i].score) < 1e-9);
    }

    return true;
}

bool testSearchWaitsUntilIndexingFinishes() {
    KeywordSearch search;

    search.beginIndexing();

    std::future<std::vector<SearchResult>> search_future =
        std::async(std::launch::async, [&search] {
            return search.search({"dragon"});
        });

    const bool search_is_blocked =
        search_future.wait_for(std::chrono::milliseconds(100)) ==
        std::future_status::timeout;

    search.finishIndexing();

    search_future.wait();
    search_future.get();

    CHECK(search_is_blocked);

    return true;
}

bool testConcurrentIndexingIsRejected() {
    KeywordSearch search;

    search.beginIndexing();

    bool exception_thrown = false;

    try {
        search.beginIndexing();
    } catch (const std::logic_error&) {
        exception_thrown = true;
    }

    search.finishIndexing();

    CHECK(exception_thrown);

    return true;
}

bool testIndexFiles() {
    const std::string directory = "index_files_test";

    std::filesystem::create_directories(directory);

    {
        std::ofstream(directory + "/10001.txt")
            << "dragon castle magic";
        std::ofstream(directory + "/10002.txt")
            << "dragon mountain adventure";
        std::ofstream(directory + "/10003.txt")
            << "castle king kingdom";
    }

    KeywordSearch search;
    ParallelIndexer indexer(search, 2);

    indexer.indexFiles(directory);

    const auto results = search.search({"dragon"});

    CHECK(results.size() == 2);
    CHECK(results[0].document_id == "10001" ||
          results[0].document_id == "10002");

    std::filesystem::remove_all(directory);
    return true;
}

bool testIndexFilesEmptyDirectory() {
    const std::string directory = "empty_index_test";
    std::filesystem::create_directories(directory);

    KeywordSearch search;
    ParallelIndexer indexer(search, 2);

    bool exception_thrown = false;

    try {
        indexer.indexFiles(directory);
    } catch (const std::runtime_error&) {
        exception_thrown = true;
    }

    std::filesystem::remove_all(directory);

    CHECK(exception_thrown);
    return true;
}

int main() {
    if (!testSameResultsWithDifferentWorkerCounts()) {
        return 1;
    }

    if (!testSearchWaitsUntilIndexingFinishes()) {
        return 1;
    }

    if (!testConcurrentIndexingIsRejected()) {
        return 1;
    }

    if (!testIndexFiles()) {
        return 1;
    }

    if (!testIndexFilesEmptyDirectory()) {
        return 1;
    }

    std::cout << "Parallel indexer test: PASSED\n";
    return 0;
}
