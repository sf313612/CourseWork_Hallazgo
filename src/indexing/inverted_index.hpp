#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <array>

namespace Hallazgo_inverted_index {

    struct Posting {
        std::uint32_t doc_id;
        std::uint32_t term_frequency;
    };

    struct TermEntry {
        std::string term;
        std::vector<Posting> postings;
    };

    class InvertedIndex {
    public:
        void addDocument(
            std::uint32_t doc_id,
            const std::vector<std::string>& tokens
        );

        void addDocumentUnique(
            std::uint32_t doc_id,
            const std::vector<std::string>& tokens
        );

        std::vector<Posting> find(const std::string& term);

    private:
        static const std::size_t bucket_count = 64;

        struct Bucket {
            std::vector<TermEntry> entries;
            std::vector<std::size_t> lookup_slots;
            std::mutex mutex;
        };

        static bool findEntryIndex(
            const Bucket& bucket,
            const std::string& term,
            std::size_t& index
        );

        static void rebuildLookup(
            Bucket& bucket,
            std::size_t new_capacity
        );

        static void ensureLookupCapacity(Bucket& bucket);

        static void insertLookup(
            Bucket& bucket,
            std::size_t entry_index
        );

        std::array<Bucket, bucket_count> buckets;
    };

}