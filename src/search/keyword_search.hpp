#pragma once

#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include "inverted_index.hpp"

namespace Hallazgo_search {

    // Один результат пошуку
    struct SearchResult {
        std::string document_id;
        double score;
    };

    // Пошук документів за ключовими словами
    class KeywordSearch {
    public:
        void beginIndexing();
        bool isReady();
        void finishIndexing(bool successful = true);

        // Додає документ до пошукової системи
        void addDocument(
            const std::string& document_id,
            const std::vector<std::string>& tokens
        );

        // Додає документ до пошукової системи, якщо він ще не доданий
        void addDocumentUnique(
            const std::string& document_id,
            const std::vector<std::string>& tokens
        );

        // Знаходить документи за запитом і повертає їх за релевантністю
        std::vector<SearchResult> search(
            const std::vector<std::string>& query
        );
    private:
        std::mutex mutex;
        Hallazgo_inverted_index::InvertedIndex index;
        std::vector<std::pair<std::string, std::size_t>> document_lengths;
        std::vector<std::string> pending_document_ids;
        std::size_t total_documents = 0;
        std::condition_variable indexing_finished;
        bool indexing_in_progress = false;
        bool index_ready = false;
    };

}
