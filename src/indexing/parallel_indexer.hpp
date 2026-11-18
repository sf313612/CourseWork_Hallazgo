
#pragma once

#include <cstddef>
#include <vector>
#include <string>

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

    private:
        Hallazgo_search::KeywordSearch& search;
        ThreadPool pool;
        std::size_t worker_count;
    };

}
