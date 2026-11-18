#include <atomic>
#include <iostream>
#include <string>
#include <thread>
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

bool testDuplicateDocumentIdMergesTerms() {
    InvertedIndex index;
    index.addDocument("doc1", {"apple", "apple"});
    index.addDocument("doc1", {"apple", "banana"});

    const std::vector<Posting> apple = index.find("apple");
    const std::vector<Posting> banana = index.find("banana");
    CHECK(apple.size() == 1);
    CHECK(apple[0].document_id == "doc1");
    CHECK(apple[0].term_frequency == 3);
    CHECK(banana.size() == 1);
    CHECK(banana[0].document_id == "doc1");
    CHECK(banana[0].term_frequency == 1);
    return true;
}

bool testConcurrentDocumentInsertion() {
    InvertedIndex index;
    const std::size_t worker_count = 4;
    const std::size_t documents_per_worker = 25;
    std::vector<std::thread> workers;

    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([worker_id, documents_per_worker, &index] {
            for (std::size_t document_id = 0;
                 document_id < documents_per_worker;
                 ++document_id) {
                const std::string id =
                    "worker-" + std::to_string(worker_id) + "-doc-" +
                    std::to_string(document_id);
                index.addDocument(id, {"shared", "shared"});
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    const std::vector<Posting> postings = index.find("shared");
    CHECK(postings.size() == worker_count * documents_per_worker);
    for (const Posting& posting : postings) {
        CHECK(posting.term_frequency == 2);
    }
    return true;
}

bool testConcurrentFindWhileInserting() {
    InvertedIndex index;
    const std::size_t document_count = 1000;
    std::atomic<bool> reader_ready(false);
    std::atomic<bool> insertion_finished(false);

    std::thread reader([&index, &reader_ready, &insertion_finished] {
        index.find("shared");
        reader_ready.store(true);
        while (!insertion_finished.load()) {
            index.find("shared");
        }
    });

    std::thread writer([&index, &reader_ready, &insertion_finished, document_count] {
        while (!reader_ready.load()) {
            std::this_thread::yield();
        }
        for (std::size_t document_id = 0;
             document_id < document_count;
             ++document_id) {
            index.addDocument(
                "concurrent-doc-" + std::to_string(document_id),
                {"shared"}
            );
        }
        insertion_finished.store(true);
    });

    writer.join();
    reader.join();

    const std::vector<Posting> postings = index.find("shared");
    CHECK(postings.size() == document_count);
    for (const Posting& posting : postings) {
        CHECK(posting.term_frequency == 1);
    }
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

    if (testDuplicateDocumentIdMergesTerms()) {
        std::cout << "testDuplicateDocumentIdMergesTerms: PASSED\n";
    } else {
        std::cout << "testDuplicateDocumentIdMergesTerms: FAILED\n";
        all_passed = false;
    }

    if (testConcurrentDocumentInsertion()) {
        std::cout << "testConcurrentDocumentInsertion: PASSED\n";
    } else {
        std::cout << "testConcurrentDocumentInsertion: FAILED\n";
        all_passed = false;
    }

    if (testConcurrentFindWhileInserting()) {
        std::cout << "testConcurrentFindWhileInserting: PASSED\n";
    } else {
        std::cout << "testConcurrentFindWhileInserting: FAILED\n";
        all_passed = false;
    }

    return all_passed ? 0 : 1;
}
