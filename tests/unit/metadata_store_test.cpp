
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "metadata_store.hpp"

using namespace Hallazgo_api;

bool testLoadsBookTitle() {
    const std::string path = "metadata_test.csv";
    {
        std::ofstream file(path);
        file << "10005,A Voyage to the Moon,1828,http://example.com\n";
        file << "10006,La Fiammetta,1907,http://example.com\n";
    }

    MetadataStore store;
    const bool loaded = store.load(path);
    const std::string title = store.getTitle("10005");

    std::remove(path.c_str());

    if (!loaded || title != "A Voyage to the Moon") {
        std::cerr << "Failed to load book title\n";
        return false;
    }
    return true;
}

bool testHandlesTitleWithComma() {
    const std::string path = "metadata_test.csv";
    {
        std::ofstream file(path);
        file << "10005,\"A Voyage, to the Moon\",1828,http://example.com\n";
    }

    MetadataStore store;
    const bool loaded = store.load(path);
    const std::string title = store.getTitle("10005");

    std::remove(path.c_str());

    if (!loaded || title != "A Voyage, to the Moon") {
        std::cerr << "Failed to parse title containing comma\n";
        return false;
    }
    return true;
}

bool testUnknownDocumentReturnsEmptyTitle() {
    const std::string path = "metadata_test.csv";
    {
        std::ofstream file(path);
        file << "10005,A Voyage to the Moon,1828,http://example.com\n";
    }

    MetadataStore store;
    store.load(path);
    const std::string title = store.getTitle("99999");

    std::remove(path.c_str());

    if (!title.empty()) {
        std::cerr << "Expected empty title for unknown document\n";
        return false;
    }
    return true;
}

bool testMissingFileReturnsFalse() {
    MetadataStore store;

    if (store.load("missing_metadata_file.csv")) {
        std::cerr << "Expected false for missing metadata file\n";
        return false;
    }
    return true;
}

int main() {
    if (!testLoadsBookTitle() ||
        !testHandlesTitleWithComma() ||
        !testUnknownDocumentReturnsEmptyTitle() ||
        !testMissingFileReturnsFalse()) {
        std::cerr << "MetadataStore test: FAILED\n";
        return 1;
    }

    std::cout << "MetadataStore test: PASSED\n";
    return 0;
}