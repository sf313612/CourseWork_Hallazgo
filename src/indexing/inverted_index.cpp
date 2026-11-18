
#include "inverted_index.hpp"

#include <functional>

namespace Hallazgo_inverted_index {

    std::vector<Posting> InvertedIndex::find(const std::string& term) {
        const std::size_t bucket_id =
            std::hash<std::string>{}(term) % bucket_count;

        Bucket& bucket = buckets[bucket_id];
        std::lock_guard<std::mutex> lock(bucket.mutex);

        for (const TermEntry& entry : bucket.entries) {
            if (entry.term == term) {
                return entry.postings;
            }
        }

        return {};
    }

    void InvertedIndex::addDocument(
        const std::string& document_id,
        const std::vector<std::string>& tokens
    ) {
        for (const std::string& token : tokens) {
            const std::size_t bucket_id =
                std::hash<std::string>{}(token) % bucket_count;

            Bucket& bucket = buckets[bucket_id];
            std::lock_guard<std::mutex> lock(bucket.mutex);

            TermEntry* found_entry = nullptr;

            for (TermEntry& entry : bucket.entries) {
                if (entry.term == token) {
                    found_entry = &entry;
                    break;
                }
            }

            if (found_entry != nullptr) {
                bool posting_found = false;

                for (Posting& posting : found_entry->postings) {
                    if (posting.document_id == document_id) {
                        posting.term_frequency++;
                        posting_found = true;
                        break;
                    }
                }

                if (!posting_found) {
                    Posting new_posting;
                    new_posting.document_id = document_id;
                    new_posting.term_frequency = 1;

                    found_entry->postings.push_back(new_posting);
                }
            } else {
                TermEntry new_entry;
                new_entry.term = token;

                Posting new_posting;
                new_posting.document_id = document_id;
                new_posting.term_frequency = 1;

                new_entry.postings.push_back(new_posting);
                bucket.entries.push_back(new_entry);
            }
        }
    }

}
