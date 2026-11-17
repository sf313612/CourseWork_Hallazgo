#include <iostream>
#include <string>
#include <vector>

std::vector<std::string> tokenize(const std::string& input);

struct TestCase {
    const char* name;
    bool (*run)();
};

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": CHECK failed: " #expression << '\n';             \
            return false;                                                     \
        }                                                                     \
    } while (false)

bool testLowercase() {
    const std::vector<std::string> tokens = tokenize("Hello");
    CHECK(tokens == std::vector<std::string>{"hello"});
    return true;
}

bool testTokenization() {
    const std::vector<std::string> expected = {"the", "old", "house"};
    CHECK(tokenize("The OLD house") == expected);
    return true;
}

bool testPunctuation() {
    const std::vector<std::string> expected = {"hello", "world"};
    CHECK(tokenize("Hello, WORLD!") == expected);
    return true;
}

int main() {
    const std::vector<TestCase> tests = {
        {"lowercase normalization", testLowercase},
        {"testTokenization", testTokenization},
        {"testPunctuation", testPunctuation},
    };

    int failures = 0;
    for (const TestCase& test : tests) {
        if (test.run()) {
            std::cout << "PASS: " << test.name << '\n';
        } else {
            std::cerr << "FAIL: " << test.name << '\n';
            ++failures;
        }
    }

    return failures == 0 ? 0 : 1;
}