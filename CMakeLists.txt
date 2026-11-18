
# Мінімальна версія CMake та назва проєкту
cmake_minimum_required(VERSION 3.20)
project(HallazgoTests LANGUAGES CXX)

# Дозволяємо запуск тестів через CTest
enable_testing()

# --- Tokenizer tests ---
# Створюємо програму для тестування Tokenizer
add_executable(tokenizer_test
    tests/unit/tokenizer_test.cpp
    src/core/tokenizer.cpp
)

target_include_directories(tokenizer_test PRIVATE
    src/core
)

# Додаємо Tokenizer до списку тестів CTest
add_test(NAME tokenizer_test COMMAND tokenizer_test)

# Код 77 означає, що тест потрібно пропустити
set_tests_properties(tokenizer_test PROPERTIES SKIP_RETURN_CODE 77)


# --- Inverted Index tests ---
# Створюємо програму для тестування Inverted Index
add_executable(inverted_index_test
    tests/unit/inverted_index_test.cpp
    src/indexing/inverted_index.cpp
)

# Вказуємо папку, де знаходиться заголовковий файл inverted_index.hpp
target_include_directories(inverted_index_test PRIVATE
    src/indexing
)

# Додаємо Inverted Index до списку тестів CTest
add_test(NAME inverted_index_test COMMAND inverted_index_test)


# --- Search tests ---
# Створюємо програму для тестування пошуку
add_executable(search_test
    tests/unit/search_test.cpp
    src/search/keyword_search.cpp
    src/indexing/inverted_index.cpp
)

# Вказуємо папку із заголовковим файлом Inverted Index
target_include_directories(search_test PRIVATE
    src/search
    src/indexing
)

# Додаємо Search до списку тестів CTest
add_test(NAME search_test COMMAND search_test)


# --- Thread Pool tests ---
add_executable(thread_pool_test
    tests/unit/thread_pool_test.cpp
    src/core/thread_pool.cpp
)

target_include_directories(thread_pool_test PRIVATE
    src/core
)

add_test(NAME thread_pool_test COMMAND thread_pool_test)

# --- Parallel Indexer tests ---
add_executable(parallel_indexer_test
    tests/unit/parallel_indexer_test.cpp
    src/indexing/parallel_indexer.cpp
    src/indexing/inverted_index.cpp
    src/search/keyword_search.cpp
    src/core/tokenizer.cpp
    src/core/thread_pool.cpp
)

target_include_directories(parallel_indexer_test PRIVATE
    src/indexing
    src/search
    src/core
)

add_test(NAME parallel_indexer_test COMMAND parallel_indexer_test)