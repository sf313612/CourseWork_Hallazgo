#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "ollama_client.hpp"
#include "tokenizer.hpp"

int main() {
    const char* host_env = std::getenv("OLLAMA_HOST");
    const char* port_env = std::getenv("OLLAMA_PORT");

    const std::string host = host_env ? host_env : "127.0.0.1";
    const int port = port_env ? std::stoi(port_env) : 11434;

    std::cout << "Connecting to local Ollama service at "
              << host << ":" << port << "...\n";

    Hallazgo_api::OllamaClient ollama(host, port);

    if (!ollama.isAvailable()) {
        std::cout << "SKIPPED: Ollama is not available at "
                  << host << ":" << port << ".\n";
        return 77;
    }

    const std::string query = "shipwrecked sailor island";
    const std::vector<std::string> original_tokens = tokenize(query);

    std::cout << "Ollama is available. Testing query expansion...\n";

    const auto result = ollama.expandQuery(query, original_tokens);

    if (!result.error.empty()) {
        std::cerr << "FAIL: Ollama returned an expansion error: "
                  << result.error << '\n';
        return 1;
    }

    // Every original query token must remain in the expanded token list.
    for (const auto& token : original_tokens) {
        if (std::find(result.tokens.begin(), result.tokens.end(), token)
            == result.tokens.end()) {
            std::cerr << "FAIL: Original token is missing: "
                      << token << '\n';
            return 1;
        }
    }

    // Count distinct tokens that were not present in the original query.
    std::vector<std::string> actual_added_terms;

    for (const auto& token : result.tokens) {
        if (std::find(original_tokens.begin(), original_tokens.end(), token)
                == original_tokens.end() &&
            std::find(actual_added_terms.begin(), actual_added_terms.end(), token)
                == actual_added_terms.end()) {
            actual_added_terms.push_back(token);
        }
    }

    if (actual_added_terms.empty()) {
        std::cerr << "FAIL: Ollama did not add any new search terms.\n";
        return 1;
    }

    if (!result.used) {
        std::cerr << "FAIL: Expansion contains new terms but used is false.\n";
        return 1;
    }

    if (result.added_terms != actual_added_terms.size()) {
        std::cerr << "FAIL: added_terms does not match the actual number "
                     "of distinct added tokens.\n";
        return 1;
    }

    std::cout << "Original tokens: ";
    for (const auto& token : original_tokens) {
        std::cout << token << " ";
    }

    std::cout << "\nAdded terms: ";
    for (const auto& token : actual_added_terms) {
        std::cout << token << " ";
    }

    std::cout << "\nOriginal token count: " << original_tokens.size()
              << "\nExpanded token count: " << result.tokens.size()
              << "\nAdded terms count: " << result.added_terms
              << "\nExpansion used: yes\n";

    std::cout << "PASS: Ollama query expansion returned new terms "
                 "and preserved the original query tokens.\n";

    return 0;
}
