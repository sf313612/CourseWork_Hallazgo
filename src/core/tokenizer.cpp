#include "tokenizer.hpp"

#include <cctype>

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string word;

    for (unsigned char character : input) {
        if (std::isalnum(character)) {
            word.push_back(static_cast<char>(std::tolower(character)));
        } else if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    }

    if (!word.empty()) {
        tokens.push_back(word);
    }

    return tokens;
}