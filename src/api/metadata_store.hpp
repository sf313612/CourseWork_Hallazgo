#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Hallazgo_api {

class MetadataStore {
public:
    bool load(const std::string& path);
    std::string getTitle(const std::string& document_id) const;

private:
    std::vector<std::pair<std::string, std::string>> titles;
};

}