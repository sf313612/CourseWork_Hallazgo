#include "inverted_index.hpp"

#include <functional>
#include <algorithm>
#include <utility>
#include <limits>
#include <stdexcept>

namespace Hallazgo_inverted_index {

    void InvertedIndex::clear() {
        for (auto& bucket : buckets) {
            std::lock_guard<std::mutex> lock(bucket.mutex);

            bucket.entries.clear();
            bucket.lookup_slots.clear();
        }
    }
    
    bool InvertedIndex::findEntryIndex(
        const Bucket& bucket,
        const std::string& term,
        std::size_t& index
    ) {
        if (bucket.lookup_slots.empty()) {
            return false;
        }

        std::size_t position =
            std::hash<std::string>{}(term) % bucket.lookup_slots.size();

        while (bucket.lookup_slots[position] != 0) {
            const std::size_t entry_index =
                bucket.lookup_slots[position] - 1;

            if (bucket.entries[entry_index].term == term) {
                index = entry_index;
                return true;
            }

            position = (position + 1) % bucket.lookup_slots.size();
        }

        return false;
    }

    void InvertedIndex::rebuildLookup(
        Bucket& bucket,
        std::size_t new_capacity
    ) {
        std::vector<std::size_t> new_slots(new_capacity, 0);

        for (std::size_t i = 0; i < bucket.entries.size(); ++i) {
            std::size_t position =
                std::hash<std::string>{}(bucket.entries[i].term)
                % new_capacity;

            while (new_slots[position] != 0) {
                position = (position + 1) % new_capacity;
            }

            new_slots[position] = i + 1;
        }

        bucket.lookup_slots.swap(new_slots);
    }

    void InvertedIndex::ensureLookupCapacity(Bucket& bucket) {
        if (bucket.lookup_slots.empty()) {
            rebuildLookup(bucket, 16);
            return;
        }

        if ((bucket.entries.size() + 1) * 10 >=
            bucket.lookup_slots.size() * 7) {
            rebuildLookup(bucket, bucket.lookup_slots.size() * 2);
        }
    }

    void InvertedIndex::insertLookup(
        Bucket& bucket,
        std::size_t entry_index
    ) {
        const std::string& term = bucket.entries[entry_index].term;

        std::size_t position =
            std::hash<std::string>{}(term) % bucket.lookup_slots.size();

        while (bucket.lookup_slots[position] != 0) {
            position = (position + 1) % bucket.lookup_slots.size();
        }

        bucket.lookup_slots[position] = entry_index + 1;
    }

    std::vector<Posting> InvertedIndex::find(const std::string& term) {
        const std::size_t bucket_id =
            std::hash<std::string>{}(term) % bucket_count;

        Bucket& bucket = buckets[bucket_id];
        std::lock_guard<std::mutex> lock(bucket.mutex);

        std::size_t entry_index;

        if (findEntryIndex(bucket, term, entry_index)) {
            return bucket.entries[entry_index].postings;
        }

        return {};
    }

    void InvertedIndex::addDocument(
        std::uint32_t doc_id,
        const std::vector<std::string>& tokens
    ) {
        std::vector<const std::string*> sorted_tokens;
        sorted_tokens.reserve(tokens.size());

        for (const std::string& token : tokens) {
            sorted_tokens.push_back(&token);
        }

        std::sort(
            sorted_tokens.begin(),
            sorted_tokens.end(),
            [](const std::string* a, const std::string* b) {
                return *a < *b;
            }
        );

        std::size_t i = 0;

        while (i < sorted_tokens.size()) {
            const std::string& token = *sorted_tokens[i];
            std::size_t frequency = 1;

            while (i + frequency < sorted_tokens.size() &&
                *sorted_tokens[i + frequency] == token) {
                ++frequency;
            }

            if (frequency > std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("term frequency is too large");
            }

            const std::uint32_t term_frequency =
                static_cast<std::uint32_t>(frequency);

            const std::size_t bucket_id =
                std::hash<std::string>{}(token) % bucket_count;

            Bucket& bucket = buckets[bucket_id];
            std::lock_guard<std::mutex> lock(bucket.mutex);

            std::size_t entry_index;

            if (findEntryIndex(bucket, token, entry_index)) {
                TermEntry& entry = bucket.entries[entry_index];

                bool posting_found = false;

                for (Posting& posting : entry.postings) {
                    if (posting.doc_id == doc_id) {
                        if (term_frequency >
                            std::numeric_limits<std::uint32_t>::max() -
                            posting.term_frequency) {
                            throw std::overflow_error("term frequency is too large");
                        }

                        posting.term_frequency += term_frequency;
                        posting_found = true;
                        break;
                    }
                }

                if (!posting_found) {
                    entry.postings.push_back({doc_id, term_frequency});
                }
            } else {
                ensureLookupCapacity(bucket);

                TermEntry new_entry;
                new_entry.term = token;
                new_entry.postings.push_back({doc_id, term_frequency});

                bucket.entries.push_back(std::move(new_entry));
                insertLookup(bucket, bucket.entries.size() - 1);
            }

            i += frequency;
        }
    }

    void InvertedIndex::addDocumentUnique(
        std::uint32_t doc_id,
        const std::vector<std::string>& tokens
    ) {
        std::vector<const std::string*> sorted_tokens;
        sorted_tokens.reserve(tokens.size());

        for (const std::string& token : tokens) {
            sorted_tokens.push_back(&token);
        }

        std::sort(
            sorted_tokens.begin(),
            sorted_tokens.end(),
            [](const std::string* a, const std::string* b) {
                return *a < *b;
            }
        );

        std::size_t i = 0;

        while (i < sorted_tokens.size()) {
            const std::string& token = *sorted_tokens[i];
            std::size_t frequency = 1;

            while (i + frequency < sorted_tokens.size() &&
                *sorted_tokens[i + frequency] == token) {
                ++frequency;
            }

            if (frequency > std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("term frequency is too large");
            }

            const std::uint32_t term_frequency =
                static_cast<std::uint32_t>(frequency);

            const std::size_t bucket_id =
                std::hash<std::string>{}(token) % bucket_count;

            Bucket& bucket = buckets[bucket_id];
            std::lock_guard<std::mutex> lock(bucket.mutex);

            std::size_t entry_index;

            if (findEntryIndex(bucket, token, entry_index)) {
                TermEntry& entry = bucket.entries[entry_index];

                entry.postings.push_back({doc_id, term_frequency});
            } else {
                ensureLookupCapacity(bucket);

                TermEntry new_entry;
                new_entry.term = token;
                new_entry.postings.push_back({doc_id, term_frequency});

                bucket.entries.push_back(std::move(new_entry));
                insertLookup(bucket, bucket.entries.size() - 1);
            }

            i += frequency;
        }
    }
}