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
        index_ready = false;
    }

    void KeywordSearch::finishIndexing(bool successful) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            indexing_in_progress = false;
            index_ready = successful && total_documents > 0;
        }

        indexing_finished.notify_all();
    }

    bool KeywordSearch::isReady() {
        std::lock_guard<std::mutex> lock(mutex);
        return index_ready && !indexing_in_progress;
    }

    void KeywordSearch::addDocument(
        const std::string& document_id,
        const std::vector<std::string>& tokens
    ) {
        {
            std::lock_guard<std::mutex> lock(mutex);

            if (document_lengths_map.find(document_id) != document_lengths_map.end()) {
                return;
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
            document_lengths_map[document_id] = tokens.size();
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

    void KeywordSearch::addDocumentUnique(
        const std::string& document_id,
        const std::vector<std::string>& tokens
    ) {
        index.addDocumentUnique(document_id, tokens);

        std::lock_guard<std::mutex> lock(mutex);
        document_lengths.push_back({document_id, tokens.size()});
        document_lengths_map[document_id] = tokens.size();
        ++total_documents;
    }

    std::vector<SearchResult> KeywordSearch::search(
        const std::vector<std::string>& query
    ) {
        std::unique_lock<std::mutex> lock(mutex);
        indexing_finished.wait(lock, [this] {
            return !indexing_in_progress;
        });
        std::unordered_map<std::string, double> score_map;
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
                auto doc_it = document_lengths_map.find(posting.document_id);
                if (doc_it == document_lengths_map.end() ||
                    doc_it->second == 0) {
                    continue;
                }

                const double tf =
                    static_cast<double>(posting.term_frequency) /
                    doc_it->second;

                const double term_score = tf * idf;

                score_map[posting.document_id] += term_score;
            }
        }

        std::vector<SearchResult> results;
        results.reserve(score_map.size());
        for (const auto& pair : score_map) {
            results.push_back({pair.first, pair.second});
        }

        std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score) {
                return a.score > b.score;
            }

            return a.document_id < b.document_id;
        });

        return results;
    }

}
