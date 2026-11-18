#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <mutex>

namespace Hallazgo_inverted_index {

    struct Posting {
        std::string document_id;
        std::size_t term_frequency;
    };

    struct TermEntry {
        std::string term;
        std::vector<Posting> postings;
    };
    
    class InvertedIndex {
        public:
        void addDocument(const std::string& document_id, const std::vector<std::string>& tokens);
        std::vector<Posting> find(const std::string& term);

        private:
        std::vector<TermEntry> entries;
        mutable std::mutex entries_mutex;   
    };

}