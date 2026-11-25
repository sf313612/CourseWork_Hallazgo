#include "keyword_search.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <limits>
#include <stdexcept>

namespace Hallazgo_search {

    void KeywordSearch::clear() {
        std::lock_guard<std::mutex> lock(mutex);

        if (indexing_in_progress) {
            throw std::logic_error("Cannot clear index while indexing is in progress");
        }

        index.clear();

        document_ids.clear();
        document_lengths.clear();
        document_id_map.clear();
        pending_document_ids.clear();

        total_documents = 0;
        total_document_length = 0;

        index_ready = false;
    }

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
        std::uint32_t doc_id;

        {
            std::lock_guard<std::mutex> lock(mutex);

            if (document_id_map.find(document_id) != document_id_map.end()) {
                return;
            }

            for (const std::string& pending_id : pending_document_ids) {
                if (pending_id == document_id) {
                    return;
                }
            }

            if (document_ids.size() >
                std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("too many documents");
            }

            pending_document_ids.push_back(document_id);

            try {
                doc_id = static_cast<std::uint32_t>(document_ids.size());

                document_ids.push_back(document_id);

                try {
                    document_lengths.push_back(0);
                    document_id_map.emplace(document_id, doc_id);
                } catch (...) {
                    document_lengths.resize(document_ids.size() - 1);
                    document_ids.pop_back();
                    throw;
                }
            } catch (...) {
                pending_document_ids.pop_back();
                throw;
            }
        }

        try {
            index.addDocument(doc_id, tokens);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);

            document_id_map.erase(document_id);
            document_ids[doc_id].clear();
            document_lengths[doc_id] = 0;

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

            document_lengths[doc_id] = tokens.size();
            total_document_length += tokens.size();
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
        std::uint32_t doc_id;

        {
            std::lock_guard<std::mutex> lock(mutex);

            if (document_id_map.find(document_id) != document_id_map.end()) {
                return;
            }

            for (const std::string& pending_id : pending_document_ids) {
                if (pending_id == document_id) {
                    return;
                }
            }

            if (document_ids.size() >
                std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("too many documents");
            }

            pending_document_ids.push_back(document_id);

            try {
                doc_id = static_cast<std::uint32_t>(document_ids.size());

                document_ids.push_back(document_id);

                try {
                    document_lengths.push_back(0);
                    document_id_map.emplace(document_id, doc_id);
                } catch (...) {
                    document_lengths.resize(document_ids.size() - 1);
                    document_ids.pop_back();
                    throw;
                }
            } catch (...) {
                pending_document_ids.pop_back();
                throw;
            }
        }

        try {
            index.addDocumentUnique(doc_id, tokens);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);

            document_id_map.erase(document_id);
            document_ids[doc_id].clear();
            document_lengths[doc_id] = 0;

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

            document_lengths[doc_id] = tokens.size();
            total_document_length += tokens.size();
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

        if (total_documents == 0 || total_document_length == 0) {
            return results;
        }

        const double average_document_length =
            static_cast<double>(total_document_length) / total_documents;

        const double k1 = 1.5;
        const double b = 0.75;

        std::unordered_map<std::uint32_t, double> score_map;
        std::unordered_set<std::string> scored_terms;

        static const std::unordered_set<std::string> stop_words = {
            "a", "an", "the",
            "and", "or", "but",
            "after", "before",
            "in", "on", "at", "to",
            "of", "for", "with",
            "is", "are", "was", "were",
            "be", "been"
        };

        for (const std::string& term : query) {
            if (stop_words.find(term) != stop_words.end()) {
                continue;
            }

            if (!scored_terms.insert(term).second) {
                continue;
            }

            const auto postings = index.find(term);

            if (postings.empty()) {
                continue;
            }

            const double document_frequency =
                static_cast<double>(postings.size());

            const double idf = std::log(
                1.0 + (total_documents - document_frequency + 0.5) /
                (document_frequency + 0.5)
            );

            for (const auto& posting : postings) {
                const std::uint32_t doc_id = posting.doc_id;

                if (doc_id >= document_lengths.size() ||
                    document_lengths[doc_id] == 0) {
                    continue;
                }

                const double tf =
                    static_cast<double>(posting.term_frequency);

                const double document_length =
                    static_cast<double>(document_lengths[doc_id]);

                const double denominator =
                    tf + k1 * (1.0 - b +
                    b * document_length / average_document_length);

                const double term_score =
                    idf * (tf * (k1 + 1.0)) / denominator;

                score_map[doc_id] += term_score;
            }
        }

        results.reserve(score_map.size());

        for (const auto& pair : score_map) {
            results.push_back({
                document_ids[pair.first],
                pair.second
            });
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