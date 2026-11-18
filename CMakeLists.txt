cmake_minimum_required(VERSION 3.20)
project(HallazgoTests LANGUAGES CXX)

enable_testing()

add_executable(tokenizer_test
    tests/unit/tokenizer_test.cpp
    src/core/tokenizer.cpp
)

add_test(NAME tokenizer_test COMMAND tokenizer_test)
set_tests_properties(tokenizer_test PROPERTIES SKIP_RETURN_CODE 77)

add_executable(inverted_index_test
    tests/unit/inverted_index_test.cpp
    src/indexing/inverted_index.cpp
)

target_include_directories(inverted_index_test PRIVATE
    src/indexing
)

add_test(NAME inverted_index_test COMMAND inverted_index_test)