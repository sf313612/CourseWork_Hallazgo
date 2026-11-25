
#include "parallel_indexer.hpp"
#include "keyword_search.hpp"
#include "tokenizer.hpp"

#include <atomic>
#include <exception>
#include <future>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <iostream>

namespace Hallazgo_indexing {

    ParallelIndexer::ParallelIndexer(
        Hallazgo_search::KeywordSearch& search,
        std::size_t worker_count
    )
        : search(search),
        pool(worker_count),
        worker_count(worker_count) {
        }

    void ParallelIndexer::indexDocuments(
        const std::vector<Document>& documents
    ) {
        if (documents.empty()) {
            return;
        }

        std::atomic<std::size_t> next_document(0);
        std::vector<std::future<void>> futures;
        std::exception_ptr error;

        const std::size_t task_count =
            std::min(worker_count, documents.size());

        futures.reserve(task_count);
        search.beginIndexing();

        try {
            for (std::size_t i = 0; i < task_count; ++i) {
                futures.push_back(pool.submit([this, &documents, &next_document] {
                    while (true) {
                        const std::size_t index =
                            next_document.fetch_add(1);

                        if (index >= documents.size()) {
                            break;
                        }

                        const Document& document = documents[index];

                        const std::vector<std::string> tokens =
                            tokenize(document.text);

                        search.addDocument(document.id, tokens);
                    }
                }));
            }
        } catch (...) {
            error = std::current_exception();
        }

        for (std::future<void>& future : futures) {
            try {
                future.get();
            } catch (...) {
                if (!error) {
                    error = std::current_exception();
                }
            }
        }

        search.finishIndexing(!error);

            if (error) {
                std::rethrow_exception(error);
            }
        }

    void ParallelIndexer::indexFiles(
        const std::string& directory,
        std::size_t max_documents
    ) {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                files.push_back(entry.path());
            }
        }

        std::sort(files.begin(), files.end());

        if (max_documents > 0 && files.size() > max_documents) {
            files.resize(max_documents);
        }

        if (files.empty()) {
            throw std::runtime_error("No text files found");
        }

        total_files.store(files.size());
        processed_files.store(0);

        std::atomic<std::size_t> next_file(0);
        std::vector<std::future<void>> futures;
        std::exception_ptr error;

        std::atomic<std::uint64_t> read_time(0);
        std::atomic<std::uint64_t> tokenize_time(0);
        std::atomic<std::uint64_t> index_time(0);

        const std::size_t task_count = std::min(worker_count, files.size());

        search.beginIndexing();

        try {
            for (std::size_t i = 0; i < task_count; ++i) {
                futures.push_back(pool.submit([this, &files, &next_file, &read_time, &tokenize_time, &index_time] {
                    while (true) {
                        const std::size_t index = next_file.fetch_add(1);

                        if (index >= files.size()) {
                            break;
                        }

                        const auto read_start = std::chrono::steady_clock::now();

                        std::ifstream file(files[index], std::ios::binary);

                        if (!file.is_open()) {
                            throw std::runtime_error("Cannot open document");
                        }

                        const std::string content(
                            (std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>()
                        );

                        const auto read_end = std::chrono::steady_clock::now();

                        const std::string document_id =
                            files[index].stem().string();

                        const auto tokenize_start = std::chrono::steady_clock::now();

                        const std::vector<std::string> tokens = tokenize(content);

                        const auto tokenize_end = std::chrono::steady_clock::now();

                        search.addDocumentUnique(document_id, tokens);

                        const auto index_end = std::chrono::steady_clock::now();

                        read_time.fetch_add(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                read_end - read_start
                            ).count()
                        );

                        tokenize_time.fetch_add(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                tokenize_end - tokenize_start
                            ).count()
                        );

                        index_time.fetch_add(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                index_end - tokenize_end
                            ).count()
                        );

                        processed_files.fetch_add(1);
                    }
                }));
            }
        } catch (...) {
            error = std::current_exception();
        }

        for (auto& future : futures) {
            try {
                future.get();
            } catch (...) {
                if (!error) {
                    error = std::current_exception();
                }
            }
        }

        std::cout << "Total read time: " << read_time.load() << " ms\n";
        std::cout << "Total tokenization time: " << tokenize_time.load() << " ms\n";
        std::cout << "Total index insertion time: " << index_time.load() << " ms\n";
        search.finishIndexing(!error);

        if (error) {
            std::rethrow_exception(error);
        }
    }

    std::size_t ParallelIndexer::getProcessedFiles() const {
        return processed_files.load();
    }

    std::size_t ParallelIndexer::getTotalFiles() const {
        return total_files.load();
    }

}
