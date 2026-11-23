# Hallazgo

**Short name:** Halla  
**Full title:** Intelligent Literary Discovery Engine

Hallazgo is a C++ literary search system developed for English-language books from the PG-19 dataset. It features parallel document indexing, BM25 relevance scoring, REST API endpoints, WebSocket pipeline streaming, CLI & Web user interfaces and LLM query expansion via Ollama.

## Project Architecture

- **C++ Core Engine**: Tokenizer, thread-safe inverted index, custom `ThreadPool`, and `ParallelIndexer`.
- **Search System**: BM25 keyword search with relevance scoring, document length normalization, and stop-word filtering.
- **REST & WebSocket API**: Built with Crow, providing `/api/health`, `/api/index`, `/api/search`, `/api/stats`, and `/ws/search`.
- **LLM Integration**: `OllamaClient` connecting locally to `qwen2.5:1.5b-instruct` for automated query expansion.
- **Clients**: Interactive Web UI (`clients/web/`) and C++ CLI client (`clients/cli/`).

## Automated Testing

Hallazgo includes a comprehensive unit and integration test suite:

- **Unit Tests**: Tokenizer, Inverted Index, Search Engine, ThreadPool, Parallel Indexer, Metadata Store.
- **REST API Integration Tests**: Real HTTP server tests covering health check, index lifecycle, search endpoints, error responses, and 8-client concurrent request handling.
- **Opt-in LLM Integration Test**: Live query expansion test against a local Ollama instance.

For detailed instructions on building, running, and configuring tests on Windows with CMake and CTest, see [Testing Documentation](docs/testing/testing.md).

## Quick Start (Building and Testing)

```powershell
# Configure CMake build
cmake -S . -B build

# Build project and test suite
cmake --build build --config Release

# Run automated tests with CTest
ctest --test-dir build -C Release --output-on-failure
```

## Running with Docker

```powershell
docker compose up -d --build
```
Access the Web UI at `http://localhost:18080`.

