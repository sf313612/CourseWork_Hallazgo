#include "keyword_search.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Hallazgo_search {

    void KeywordSearch::beginIndexing() {
        std::lock_guard<std::mutex> lock(mutex);

        if (indexing_in_progress) {
            throw std::logic_error("indexing is already in progress");
        }

        indexing_in_progress = true;
    }

    void KeywordSearch::finishIndexing() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            indexing_in_progress = false;
        }

        indexing_finished.notify_all();
    }

    void KeywordSearch::addDocument(
        const std::string& document_id,
        const std::vector<std::string>& tokens
    ) {
        {
            std::lock_guard<std::mutex> lock(mutex);

            for (const auto& document : document_lengths) {
                if (document.first == document_id) {
                    return;
                }
            }

            for (const std::string& pending_id : pending_document_ids) {
                if (pending_id == document_id) {
                    return;
                }
            }

            pending_document_ids.push_back(document_id);
        }

        try {
            index.addDocument(document_id, tokens);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);

            for (auto it = pending_document_ids.begin();
                it != pending_document_ids.end(); ++it) {
                if (*it == document_id) {
                    pending_document_ids.erase(it);
                    break;
                }
            }

            throw;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);

            document_lengths.push_back({document_id, tokens.size()});
            ++total_documents;

            for (auto it = pending_document_ids.begin();
                it != pending_document_ids.end(); ++it) {
                if (*it == document_id) {
                    pending_document_ids.erase(it);
                    break;
                }
            }
        }
    }

    std::vector<SearchResult> KeywordSearch::search(
        const std::vector<std::string>& query
    ) {
        std::unique_lock<std::mutex> lock(mutex);
        indexing_finished.wait(lock, [this] {
            return !indexing_in_progress;
        });
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
