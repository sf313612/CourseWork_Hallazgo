
#include "parallel_indexer.hpp"
#include "keyword_search.hpp"
#include "tokenizer.hpp"

#include <atomic>
#include <exception>
#include <future>
#include <vector>
#include <algorithm>

namespace Hallazgo_indexing {

    ParallelIndexer::ParallelIndexer(
        Hallazgo_search::KeywordSearch& search,
        std::size_t worker_count
    )
        : search(search),
        pool(worker_count),
        worker_count(worker_count) {
        }

    void ParallelIndexer::indexDocuments(
        const std::vector<Document>& documents
    ) {
        if (documents.empty()) {
            return;
        }

        std::atomic<std::size_t> next_document(0);
        std::vector<std::future<void>> futures;
        std::exception_ptr error;

        const std::size_t task_count =
            std::min(worker_count, documents.size());

        futures.reserve(task_count);
        search.beginIndexing();

        try {
            for (std::size_t i = 0; i < task_count; ++i) {
                futures.push_back(pool.submit([this, &documents, &next_document] {
                    while (true) {
                        const std::size_t index =
                            next_document.fetch_add(1);

                        if (index >= documents.size()) {
                            break;
                        }

                        const Document& document = documents[index];

                        const std::vector<std::string> tokens =
                            tokenize(document.text);

                        search.addDocument(document.id, tokens);
                    }
                }));
            }
        } catch (...) {
            error = std::current_exception();
        }

        for (std::future<void>& future : futures) {
            try {
                future.get();
            } catch (...) {
                if (!error) {
                    error = std::current_exception();
                }
            }
        }

        search.finishIndexing();

        if (error) {
            std::rethrow_exception(error);
        }
    }

}
