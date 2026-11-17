cmake_minimum_required(VERSION 3.20)
project(HallazgoTokenizerTests LANGUAGES CXX)

enable_testing()

add_executable(tokenizer_test
    tests/unit/tokenizer_test.cpp
    src/core/tokenizer.cpp
)

add_test(NAME tokenizer_test COMMAND tokenizer_test)
set_tests_properties(tokenizer_test PROPERTIES SKIP_RETURN_CODE 77)