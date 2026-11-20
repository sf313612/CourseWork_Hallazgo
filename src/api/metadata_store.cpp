#include "metadata_store.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace Hallazgo_api {

namespace {

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inside_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char character = line[i];

        if (character == '"') {
            if (inside_quotes && i + 1 < line.size() &&
                line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                inside_quotes = !inside_quotes;
            }
        } else if (character == ',' && !inside_quotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += character;
        }
    }

    fields.push_back(field);
    return fields;
}

}

bool MetadataStore::load(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    titles.clear();
    std::string line;

    while (std::getline(file, line)) {
        const std::vector<std::string> fields = parseCsvLine(line);

        if (fields.size() < 2 || fields[0].empty() ||
            fields[1].empty()) {
            continue;
        }

        titles.emplace_back(fields[0], fields[1]);
    }

    std::sort(titles.begin(), titles.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

    return !titles.empty();
}

std::string MetadataStore::getTitle(
    const std::string& document_id) const {

    const auto it = std::lower_bound(
        titles.begin(), titles.end(), document_id,
        [](const auto& entry, const std::string& id) {
            return entry.first < id;
        }
    );

    if (it == titles.end() || it->first != document_id) {
        return "";
    }

    return it->second;
}

}
