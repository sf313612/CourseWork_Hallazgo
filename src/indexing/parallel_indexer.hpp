
#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <atomic>

#include "thread_pool.hpp"

namespace Hallazgo_search {
    class KeywordSearch;
}

namespace Hallazgo_indexing {

    struct Document {
        std::string id;
        std::string text;
    };

    class ParallelIndexer {
    public:
        ParallelIndexer(
            Hallazgo_search::KeywordSearch& search,
            std::size_t worker_count
        );

        void indexDocuments(const std::vector<Document>& documents);
        void indexFiles(
            const std::string& directory,
            std::size_t max_documents = 0
        );
        std::size_t getProcessedFiles() const;
        std::size_t getTotalFiles() const;

    private:
        Hallazgo_search::KeywordSearch& search;
        ThreadPool pool;
        std::size_t worker_count;
        std::atomic<std::size_t> processed_files{0};
        std::atomic<std::size_t> total_files{0};
    };

}
