
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


bool testBM25DocumentLengthNormalization() {
    KeywordSearch search;

    search.addDocument("short", {"dragon", "magic"});

    search.addDocument("long", {
        "dragon", "magic",
        "castle", "king", "forest", "sword",
        "battle", "wizard", "stone", "river"
    });

    const auto results = search.search({"dragon"});

    if (results.size() != 2) {
        std::cerr << "Expected 2 matching documents\n";
        return false;
    }

    if (results[0].document_id != "short") {
        std::cerr << "Expected shorter document to rank higher\n";
        return false;
    }

    if (results[0].score <= results[1].score) {
        std::cerr << "Document length normalization failed\n";
        return false;
    }

    return true;
}


bool testBM25TermFrequencySaturation() {
    KeywordSearch search;

    search.addDocument("one", {
        "dragon", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    });

    search.addDocument("two", {
        "dragon", "dragon", "b1", "b2", "b3", "b4", "b5", "b6"
    });

    search.addDocument("four", {
        "dragon", "dragon", "dragon", "dragon",
        "c1", "c2", "c3", "c4"
    });

    const auto results = search.search({"dragon"});

    if (results.size() != 3) {
        std::cerr << "Expected 3 matching documents\n";
        return false;
    }

    if (results[0].document_id != "four" ||
        results[1].document_id != "two" ||
        results[2].document_id != "one") {
        std::cerr << "Term frequency ranking is incorrect\n";
        return false;
    }

    const double first_increase =
        results[1].score - results[2].score;

    const double second_increase =
        results[0].score - results[1].score;

    if (first_increase <= second_increase) {
        std::cerr << "Expected diminishing score increase for repeated terms\n";
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


bool testAddDocumentUniqueRepeatedInsertion() {
    KeywordSearch search;

    search.addDocumentUnique("doc1", {"dragon", "magic"});
    search.addDocumentUnique("doc1", {"castle", "king"});

    const auto original_results = search.search({"dragon", "magic"});
    const auto duplicate_results = search.search({"castle", "king"});

    if (original_results.size() != 1 ||
        original_results[0].document_id != "doc1") {
        std::cerr << "Original document was not preserved\n";
        return false;
    }

    if (!duplicate_results.empty()) {
        std::cerr << "Repeated insertion added new terms to the document\n";
        return false;
    }

    return true;
}

bool testConcurrentAddDocumentUnique() {
    KeywordSearch search;

    const std::size_t worker_count = 4;
    const std::size_t documents_per_worker = 25;
    std::vector<std::thread> workers;

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([worker_id, documents_per_worker, &search] {
            for (std::size_t i = 0; i < documents_per_worker; ++i) {
                const std::string id =
                    "unique-worker-" + std::to_string(worker_id) +
                    "-doc-" + std::to_string(i);

                const std::string term =
                    "unique-term-" + std::to_string(worker_id) +
                    "-" + std::to_string(i);

                search.addDocumentUnique(id, {"shared", term});
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    const auto results = search.search({"shared"});

    if (results.size() != worker_count * documents_per_worker) {
        std::cerr << "Unexpected document count after concurrent unique insertion\n";
        return false;
    }

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        for (std::size_t i = 0; i < documents_per_worker; ++i) {
            const std::string term =
                "unique-term-" + std::to_string(worker_id) +
                "-" + std::to_string(i);

            const auto document_results = search.search({term});

            if (document_results.size() != 1) {
                std::cerr << "Document missing after concurrent unique insertion\n";
                return false;
            }
        }
    }

    return true;
}

bool testIndexReadinessAfterSuccessfulIndexing() {
    KeywordSearch search;

    if (search.isReady()) {
        std::cerr << "Empty index should not be ready\n";
        return false;
    }

    search.beginIndexing();

    if (search.isReady()) {
        std::cerr << "Index should not be ready during indexing\n";
        return false;
    }

    search.addDocument("doc1", {"dragon", "magic"});
    search.finishIndexing(true);

    if (!search.isReady()) {
        std::cerr << "Index should be ready after successful indexing\n";
        return false;
    }

    return true;
}

bool testLargeScaleKeywordSearchPerformanceAndRanking() {
    KeywordSearch search;
    const std::size_t doc_count = 500;

    // Create many documents sharing a common term ("shipwrecked")
    // and additional matching/non-matching terms.
    for (std::size_t i = 0; i < doc_count; ++i) {
        std::string doc_id = "doc_" + std::to_string(i);
        std::vector<std::string> tokens = {"shipwrecked", "sailor"};
        // Add varying extra terms to test ranking & deterministic ordering
        for (std::size_t j = 0; j < (i % 5); ++j) {
            tokens.push_back("extra" + std::to_string(j));
        }
        search.addDocument(doc_id, tokens);
    }

    // Add some documents without the common term
    search.addDocument("doc_other_1", {"ocean", "wave"});
    search.addDocument("doc_other_2", {"mountain", "climb"});

    // Search with multiple terms including common and additional terms
    auto results = search.search({"shipwrecked", "sailor"});

    if (results.empty()) {
        std::cerr << "Expected non-empty results for large scale search\n";
        return false;
    }

    // Verify all expected documents containing "shipwrecked" or "sailor" are returned
    if (results.size() != doc_count) {
        std::cerr << "Expected " << doc_count << " matching documents, got " << results.size() << "\n";
        return false;
    }

    // Verify ranking (sorted by score descending, then document ID ascending)
    for (std::size_t i = 0; i < results.size() - 1; ++i) {
        if (results[i].score < results[i + 1].score) {
            std::cerr << "Results not sorted by score descending at index " << i << "\n";
            return false;
        }
        if (results[i].score == results[i + 1].score) {
            if (results[i].document_id >= results[i + 1].document_id) {
                std::cerr << "Results with equal score not sorted by document ID ascending\n";
                return false;
            }
        }
    }

    return true;
}


bool testStopWordsAreIgnored() {
    KeywordSearch search;

    search.addDocument("doc1", {"shipwrecked", "sailor"});
    search.addDocument("doc2", {"home", "years"});
    search.addDocument("doc3", {"the", "and", "a"});

    auto results = search.search({"the", "shipwrecked", "and", "sailor"});

    if (results.size() != 1 || results[0].document_id != "doc1") {
        std::cerr << "Stop words were not ignored correctly\n";
        return false;
    }

    return true;
}

bool testStopWordsOnlyQueryReturnsNoResults() {
    KeywordSearch search;

    search.addDocument("doc1", {"shipwrecked", "sailor"});
    search.addDocument("doc2", {"home", "years"});

    auto results = search.search({"the", "and", "a", "after"});

    if (!results.empty()) {
        std::cerr << "Stop-word-only query should return no results\n";
        return false;
    }

    return true;
}

bool testStopWordsPreserveOrSearch() {
    KeywordSearch search;

    search.addDocument("doc1", {"shipwrecked"});
    search.addDocument("doc2", {"sailor"});
    search.addDocument("doc3", {"mountain"});

    auto results = search.search({"the", "shipwrecked", "sailor"});

    if (results.size() != 2) {
        std::cerr << "OR search behavior changed after stop-word filtering\n";
        return false;
    }

    return true;
}


bool testIndexIsNotReadyAfterFailedIndexing() {
    KeywordSearch search;

    search.beginIndexing();
    search.addDocument("doc1", {"dragon"});
    search.finishIndexing(false);

    if (search.isReady()) {
        std::cerr << "Index should not be ready after failed indexing\n";
        return false;
    }

    return true;
}

bool testDuplicateQueryTermsDoNotChangeScore() {
    KeywordSearch search;

    search.addDocument("doc1", {"shipwrecked", "sailor"});
    search.addDocument("doc2", {"ocean", "waves"});

    auto single_term_results = search.search({"shipwrecked"});
    auto duplicate_term_results = search.search({"shipwrecked", "shipwrecked"});

    if (single_term_results.size() != duplicate_term_results.size()) {
        std::cerr << "Duplicate query terms changed the number of results\n";
        return false;
    }

    for (std::size_t i = 0; i < single_term_results.size(); ++i) {
        if (single_term_results[i].document_id != duplicate_term_results[i].document_id ||
            single_term_results[i].score != duplicate_term_results[i].score) {
            std::cerr << "Duplicate query terms changed document ranking or score\n";
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
        std::cerr << "BM25 ranking test: FAILED\n";
        return 1;
    }

    if (!testBM25DocumentLengthNormalization()) {
    std::cerr << "BM25 document length normalization test: FAILED\n";
        return 1;
    }

    if (!testBM25TermFrequencySaturation()) {
        std::cerr << "BM25 term frequency saturation test: FAILED\n";
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

    if (!testIndexReadinessAfterSuccessfulIndexing()) {
        return 1;
    }

    if (!testLargeScaleKeywordSearchPerformanceAndRanking()) {
        std::cerr << "Large scale keyword search test: FAILED\n";
        return 1;
    }

    if (!testStopWordsAreIgnored()) {
        return 1;
    }

    if (!testStopWordsOnlyQueryReturnsNoResults()) {
        return 1;
    }

    if (!testStopWordsPreserveOrSearch()) {
        return 1;
    }

    if (!testIndexIsNotReadyAfterFailedIndexing()) {
        return 1;
    }

    if (!testAddDocumentUniqueRepeatedInsertion()) {
        std::cerr << "Repeated unique insertion test: FAILED\n";
        return 1;
    }

    if (!testConcurrentAddDocumentUnique()) {
        std::cerr << "Concurrent unique insertion test: FAILED\n";
        return 1;
    }

    if (!testDuplicateQueryTermsDoNotChangeScore()) {
        return 1;
    }

    std::cout << "Search test: PASSED\n";
    return 0;
} 
