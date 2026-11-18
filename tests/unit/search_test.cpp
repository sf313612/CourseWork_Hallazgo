
#include <iostream>
#include <string>
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

    if (!testRepeatedQueryTermsDoNotIncreaseScore()) {
        return 1;
    }

    std::cout << "Search test: PASSED\n";
    return 0;
}
