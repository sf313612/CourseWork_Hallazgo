#pragma once

#include <string>
#include <vector>
#include <utility>
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
        // Додає документ до пошукової системи
        void addDocument(
            const std::string& document_id,
            const std::vector<std::string>& tokens
        );

        // Знаходить документи за запитом і повертає їх за релевантністю
        std::vector<SearchResult> search(
            const std::vector<std::string>& query
        );
    private:
        Hallazgo_inverted_index::InvertedIndex index;
        std::vector<std::pair<std::string, std::size_t>> document_lengths;
        std::size_t total_documents = 0;
    };

}
