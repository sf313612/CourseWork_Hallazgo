#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <unordered_map>
#include <cstdint>

#include "inverted_index.hpp"

namespace Hallazgo_search {

    struct SearchResult {
        std::string document_id;
        double score;
    };

    class KeywordSearch {
    public:
        void clear();
        void beginIndexing();
        bool isReady();
        void finishIndexing(bool successful = true);

        void addDocument(
            const std::string& document_id,
            const std::vector<std::string>& tokens
        );

        void addDocumentUnique(
            const std::string& document_id,
            const std::vector<std::string>& tokens
        );

        std::vector<SearchResult> search(
            const std::vector<std::string>& query
        );

    private:
        std::mutex mutex;

        Hallazgo_inverted_index::InvertedIndex index;

        std::vector<std::string> document_ids;
        std::vector<std::size_t> document_lengths;

        std::unordered_map<std::string, std::uint32_t> document_id_map;

        std::vector<std::string> pending_document_ids;

        std::size_t total_documents = 0;
        std::size_t total_document_length = 0;

        std::condition_variable indexing_finished;

        bool indexing_in_progress = false;
        bool index_ready = false;
    };

}