#include "keyword_search.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Hallazgo_search {

    void KeywordSearch::addDocument(const std::string& document_id, const std::vector<std::string>& tokens) {
        for (const auto& document : document_lengths) {
            if (document.first == document_id) {
                return;
            }
        }
        index.addDocument(document_id, tokens);

        document_lengths.push_back({document_id, tokens.size()});
        ++total_documents;
    }

    std::vector<SearchResult> KeywordSearch::search(
        const std::vector<std::string>& query
    ) {
        std::vector<SearchResult> results;
        std::unordered_set<std::string> scored_terms;

        for (const std::string& term : query) {
            if (!scored_terms.insert(term).second) {
                continue;
            }
            const auto postings = index.find(term);

            if (postings.empty()) {
                continue;
            }

            const double idf = std::log(
                static_cast<double>(total_documents) / postings.size()
            );

            for (const auto& posting : postings) {
                auto result = std::find_if(
                    results.begin(),
                    results.end(),
                    [&](const SearchResult& item) {
                        return item.document_id == posting.document_id;
                    }
                );

                auto document = std::find_if(
                    document_lengths.begin(),
                    document_lengths.end(),
                    [&](const auto& item) {
                        return item.first == posting.document_id;
                    }
                );

                if (document == document_lengths.end() ||
                    document->second == 0) {
                    continue;
                }

                const double tf =
                    static_cast<double>(posting.term_frequency) /
                    document->second;

                const double term_score = tf * idf;

                if (result == results.end()) {
                    results.push_back({posting.document_id, term_score});
                } else {
                    result->score += term_score;
                }
            }
        }

        std::sort(
            results.begin(),
            results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                return a.score > b.score;
            }
        );

        return results;
    }

}