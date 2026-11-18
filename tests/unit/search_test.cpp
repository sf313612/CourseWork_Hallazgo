
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "keyword_search.hpp"

using namespace Hallazgo_search;

bool testSearchReturnsMatchingDocuments() {
    KeywordSearch search;

    search.addDocument("doc1", {"apple", "apple", "banana"});
    search.addDocument("doc2", {"apple", "apple", "orange"});
    search.addDocument("doc3", {"banana", "orange", "orange"});

    const std::vector<SearchResult> results =
        search.search({"apple", "banana"});
    const std::vector<std::string> expected_ids = {"doc1", "doc2", "doc3"};

    if (results.size() != expected_ids.size()) {
        std::cerr << "Expected 3 matching documents\n";
        return false;
    }

    for (std::size_t i = 0; i < expected_ids.size(); ++i) {
        if (results[i].document_id != expected_ids[i]) {
            std::cerr << "Unexpected result document ID at rank " << i << "\n";
            return false;
        }
    }

    if (!(results[0].score > results[1].score &&
          results[1].score > results[2].score)) {
        std::cerr << "Expected multi-term relevance scores to be strictly ranked\n";
        return false;
    }

    return true;
}

bool testMoreRelevantDocumentGetsHigherScore() {
    KeywordSearch search;

    search.addDocument("doc1", {"dragon", "dragon", "dragon", "magic"});
    search.addDocument("doc2", {"dragon", "magic", "magic", "magic"});
    search.addDocument("doc3", {"castle", "king"});

    const std::vector<SearchResult> results =
        search.search({"dragon"});

    if (results.size() != 2) {
        std::cerr << "Expected 2 matching documents\n";
        return false;
    }

    if (results[0].score <= results[1].score) {
        std::cerr << "Expected results to be sorted by score\n";
        return false;
    }

    if (results[0].document_id != "doc1") {
        std::cerr << "Expected doc1 to rank first\n";
        return false;
    }

    return true;
}

bool testEmptyQueryReturnsNoResults() {
    KeywordSearch search;

    search.addDocument("doc1", {"dragon", "magic"});

    const std::vector<SearchResult> results = search.search({});

    if (!results.empty()) {
        std::cerr << "Expected empty results for empty query\n";
        return false;
    }

    return true;
}

bool testDuplicateDocumentIdIsIgnored() {
    KeywordSearch search;

    search.addDocument("doc1", {"dragon"});
    search.addDocument("doc1", {"magic"});

    const std::vector<SearchResult> results = search.search({"magic"});

    if (!results.empty()) {
        std::cerr << "Expected duplicate document ID to be ignored\n";
        return false;
    }

    return true;
}

bool testConcurrentDuplicateDocumentIdIsIgnored() {
    KeywordSearch search;
    const std::size_t worker_count = 8;
    const std::string document_id = "shared-document";
    std::vector<std::string> terms;
    std::vector<std::thread> workers;

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        const std::string term = "unique-term-" + std::to_string(worker_id);
        terms.push_back(term);
        workers.emplace_back([&search, document_id, term] {
            search.addDocument(document_id, {term});
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    std::size_t matching_terms = 0;
    for (const std::string& term : terms) {
        const std::vector<SearchResult> results = search.search({term});
        if (!results.empty()) {
            if (results.size() != 1 || results[0].document_id != document_id) {
                std::cerr << "Unexpected result for concurrent duplicate ID\n";
                return false;
            }
            ++matching_terms;
        }
    }

    if (matching_terms != 1) {
        std::cerr << "Expected exactly one duplicate-ID insertion to be accepted\n";
        return false;
    }
    return true;
}

bool testRepeatedQueryTermsDoNotIncreaseScore() {
    KeywordSearch search;
    search.addDocument("doc1", {"dragon", "magic"});
    search.addDocument("doc2", {"castle"});

    const std::vector<SearchResult> single_term = search.search({"dragon"});
    const std::vector<SearchResult> repeated_term =
        search.search({"dragon", "dragon"});

    if (single_term.size() != 1 || repeated_term.size() != 1) {
        std::cerr << "Expected one result for both queries\n";
        return false;
    }
    if (single_term[0].document_id != repeated_term[0].document_id ||
        single_term[0].score != repeated_term[0].score) {
        std::cerr << "Repeated query terms changed the result score\n";
        return false;
    }
    return true;
}

bool testConcurrentInsertionMakesAllDocumentsSearchable() {
    KeywordSearch search;
    const std::size_t worker_count = 4;
    const std::size_t documents_per_worker = 25;
    std::vector<std::thread> workers;

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([worker_id, documents_per_worker, &search] {
            for (std::size_t document_number = 0;
                 document_number < documents_per_worker;
                 ++document_number) {
                const std::string id =
                    "worker-" + std::to_string(worker_id) + "-doc-" +
                    std::to_string(document_number);
                const std::string unique_term =
                    "term-" + std::to_string(worker_id) + "-" +
                    std::to_string(document_number);
                search.addDocument(id, {"shared", unique_term});
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        for (std::size_t document_number = 0;
             document_number < documents_per_worker;
             ++document_number) {
            const std::string id =
                "worker-" + std::to_string(worker_id) + "-doc-" +
                std::to_string(document_number);
            const std::string unique_term =
                "term-" + std::to_string(worker_id) + "-" +
                std::to_string(document_number);
            const std::vector<SearchResult> results = search.search({unique_term});
            if (results.size() != 1 || results[0].document_id != id) {
                std::cerr << "Document was not searchable after concurrent insertion: "
                          << id << '\n';
                return false;
            }
        }
    }
    return true;
}

bool testSearchWhileDocumentsAreInserted() {
    KeywordSearch search;
    const std::size_t document_count = 250;
    std::atomic<bool> reader_ready(false);
    std::atomic<bool> insertion_finished(false);

    std::thread reader([&search, &reader_ready, &insertion_finished] {
        search.search({"shared"});
        reader_ready.store(true);
        while (!insertion_finished.load()) {
            search.search({"shared"});
        }
    });

    std::thread writer([&search, &reader_ready, &insertion_finished, document_count] {
        while (!reader_ready.load()) {
            std::this_thread::yield();
        }
        for (std::size_t document_number = 0;
             document_number < document_count;
             ++document_number) {
            const std::string id = "reader-writer-doc-" +
                                   std::to_string(document_number);
            const std::string unique_term = "reader-writer-term-" +
                                            std::to_string(document_number);
            search.addDocument(id, {"shared", unique_term});
        }
        insertion_finished.store(true);
    });

    writer.join();
    reader.join();

    for (std::size_t document_number = 0;
         document_number < document_count;
         ++document_number) {
        const std::string id = "reader-writer-doc-" +
                               std::to_string(document_number);
        const std::string unique_term = "reader-writer-term-" +
                                        std::to_string(document_number);
        const std::vector<SearchResult> results = search.search({unique_term});
        if (results.size() != 1 || results[0].document_id != id) {
            std::cerr << "Document was not searchable after insertion: "
                      << id << '\n';
            return false;
        }
    }
    return true;
}

int main() {
    if (!testSearchReturnsMatchingDocuments()) {
        std::cerr << "Search test: FAILED\n";
        return 1;
    }

    if (!testMoreRelevantDocumentGetsHigherScore()) {
        std::cerr << "TF-IDF test: FAILED\n";
        return 1;
    }

    if (!testEmptyQueryReturnsNoResults()) {
        return 1;
    }

    if (!testDuplicateDocumentIdIsIgnored()) {
        return 1;
    }

    if (!testConcurrentDuplicateDocumentIdIsIgnored()) {
        std::cerr << "Concurrent duplicate document ID test: FAILED\n";
        return 1;
    }

    if (!testRepeatedQueryTermsDoNotIncreaseScore()) {
        return 1;
    }

    if (!testConcurrentInsertionMakesAllDocumentsSearchable()) {
        std::cerr << "Concurrent insertion test: FAILED\n";
        return 1;
    }

    if (!testSearchWhileDocumentsAreInserted()) {
        std::cerr << "Concurrent search and insertion test: FAILED\n";
        return 1;
    }

    std::cout << "Search test: PASSED\n";
    return 0;
}
