#include <iostream>
#include <string>
#include <vector>

#include "inverted_index.hpp"

using namespace Hallazgo_inverted_index;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": CHECK failed: " #expression << '\n';             \
            return false;                                                     \
        }                                                                     \
    } while (false)

bool testFindNewWord() {
    InvertedIndex index;
    index.addDocument("doc1", {"apple", "banana"});
    const std::vector<Posting> results = index.find("apple");
    CHECK(results.size() == 1);
    CHECK(results[0].document_id == "doc1");
    CHECK(results[0].term_frequency == 1);
    return true;
}

bool testRepeatedWord() {
    InvertedIndex index;
    index.addDocument("doc1", {"apple", "apple", "banana"});

    const std::vector<Posting> results = index.find("apple");
    CHECK(results.size() == 1);
    CHECK(results[0].document_id == "doc1");
    CHECK(results[0].term_frequency == 2);
    return true;
}

bool testWordInMultipleDocuments() {
    InvertedIndex index;
    index.addDocument("doc1", {"apple", "banana"});
    index.addDocument("doc2", {"apple", "orange"});

    const std::vector<Posting> results = index.find("apple");
    CHECK(results.size() == 2);
    CHECK(results[0].document_id == "doc1");
    CHECK(results[0].term_frequency == 1);
    CHECK(results[1].document_id == "doc2");
    CHECK(results[1].term_frequency == 1);
    return true;
}

bool testFindMissingWord() {
    InvertedIndex index;
    index.addDocument("doc1", {"apple", "banana"});

    const std::vector<Posting> results = index.find("dragon");
    CHECK(results.empty());
    return true;
}

int main() {
    bool all_passed = true;

    if (testFindNewWord()) {
        std::cout << "testFindNewWord: PASSED\n";
    } else {
        std::cout << "testFindNewWord: FAILED\n";
        all_passed = false;
    }

    if (testRepeatedWord()) {
        std::cout << "testRepeatedWord: PASSED\n";
    } else {
        std::cout << "testRepeatedWord: FAILED\n";
        all_passed = false;
    }

    if (testWordInMultipleDocuments()) {
        std::cout << "testWordInMultipleDocuments: PASSED\n";
    } else {
        std::cout << "testWordInMultipleDocuments: FAILED\n";
        all_passed = false;
    }

    if (testFindMissingWord()) {
        std::cout << "testFindMissingWord: PASSED\n";
    } else {
        std::cout << "testFindMissingWord: FAILED\n";
        all_passed = false;
    }

    return all_passed ? 0 : 1;
}