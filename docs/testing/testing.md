# Testing Guide for Hallazgo

This document describes the automated testing setup for **Hallazgo**, including unit tests, REST API integration tests, WebSocket route verification, multi-client concurrency tests, and live Ollama LLM integration tests.

---

## 1. Automated Test Suites Overview

The project builds **8 test executables**: 7 default test suites registered with CMake/CTest and 1 opt-in integration test for the live Ollama service.

### A. Unit Tests (`tests/unit/`)
- `tokenizer_test`: Verifies lowercase normalization, alphanumeric token splitting, punctuation removal, whitespace/tab handling, and empty inputs.
- `inverted_index_test`: Verifies single/multi-word lookup, term frequencies, document posting lists, unique term insertion, duplicate document ID handling, concurrent document insertions, and hash collision resilience.
- `search_test`: Verifies BM25 relevance scoring, document length normalization, term frequency saturation, stop-word filtering, duplicate term handling, ranking correctness, and thread safety during concurrent indexing.
- `thread_pool_test`: Verifies worker thread initialization, task submission, `std::future` return values, cross-thread exception propagation, graceful shutdown, post-shutdown task rejection, and parallel multi-worker execution.
- `parallel_indexer_test`: Verifies parallel directory indexing using `ParallelIndexer`, index consistency across different worker thread counts (1 vs. 4), index readiness synchronization, and empty directory handling.
- `metadata_store_test`: Verifies CSV metadata parsing, quoted titles containing commas, missing document fallback handling, and file read error handling.

### B. Integration Tests (`tests/integration/`)
- `integration_test`: Launches a multithreaded Crow HTTP/WebSocket server on port `18081` using a temporary deterministic dataset (`doc1.txt`–`doc3.txt` and `metadata.csv`) and executes end-to-end checks via `httplib::Client`:
  - **`GET /api/health`**: Verifies `status: "ok"` and boolean `ollama` availability flag.
  - **`GET /api/stats` (Initial State)**: Verifies initial statistics (`processed_document_count == 0`, `total_document_count == 0`, `indexing_status`).
  - **`POST /api/index` & Search Lifecycle**: Verifies that searching before indexing returns `503 Service Unavailable`; triggers asynchronous indexing (`202 Accepted` with `job_id`), polls `GET /api/stats` until `indexing_status == "completed"`, verifies keyword search (`200 OK` with ranked `results`), and validates `400 Bad Request` for invalid search modes or queries without searchable tokens.
  - **`POST /api/index` Validation**: Verifies `400 Bad Request` when `threads < 1` or when the `threads` field is missing.
  - **Concurrent HTTP Clients**: Spawns 8 parallel client threads executing 160 simultaneous `POST /api/search` and `GET /api/stats` requests (20 per thread) to ensure thread safety and absence of race conditions.
  - **WebSocket Route Rejection**: Verifies that `/ws/search` rejects plain HTTP `GET` requests without WebSocket upgrade headers (`404 Not Found`).
  - **Ollama Client Fallback**: Verifies that `OllamaClient` falls back gracefully to original query tokens (`used: false`, non-empty `error`) when the LLM endpoint is unreachable.

### C. Opt-In Integration Tests (`tests/integration/`)
- `ollama_integration_test`: Connects to a live Ollama server at `127.0.0.1:11434` using the `qwen2.5:1.5b-instruct` model to verify query expansion, stop-word/numbering filtering, and preservation of original query tokens. (Excluded from default `ctest` runs so offline builds do not fail.)

---

## 2. Step-by-Step: Building and Running Tests on Windows

### Step 1: Prerequisites
- **Visual Studio 2022** (with Desktop development with C++ and CMake tools) or **CMake 3.20+** + **MSVC**.
- **vcpkg** with `crow` and `cpp-httplib` installed (e.g., `vcpkg install crow cpp-httplib`).

### Step 2: Configure CMake
Open PowerShell in the `Hallazgo` repository root and configure the build directory (adjust the `CMAKE_TOOLCHAIN_FILE` path to match your local vcpkg installation):

```powershell
cmake -S . -B build-crow -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Step 3: Build All Test Executables
Compile all unit, integration, and server targets in `Release` mode:

```powershell
cmake --build build-crow --config Release
```

### Step 4: Run All Default Tests via CTest
Execute all 7 registered CTest suites at once:

```powershell
ctest --test-dir build-crow -C Release --output-on-failure
```

### Step 5: (Optional) Run Individual Test Executables Directly
To inspect detailed console output from a specific test binary:

```powershell
.\build-crow\Release\tokenizer_test.exe
.\build-crow\Release\inverted_index_test.exe
.\build-crow\Release\search_test.exe
.\build-crow\Release\thread_pool_test.exe
.\build-crow\Release\parallel_indexer_test.exe
.\build-crow\Release\metadata_store_test.exe
.\build-crow\Release\integration_test.exe
```

---

## 3. Running the Full System & Automated Model Provisioning

When starting the containerized stack via Docker Compose, model provisioning is handled automatically by the `ollama-init` helper container (which pulls `qwen2.5:1.5b-instruct` into the shared Ollama volume on startup—no manual `ollama pull` step is required):

```powershell
docker compose up -d --build
```

Once `ollama-init` finishes successfully, the REST API and Web UI are accessible at `http://localhost:18080` (and interactive Swagger documentation at `http://localhost:18080/docs`).

---

## 4. Running the Live Ollama Integration Test (`127.0.0.1:11434`)

Because `ollama_integration_test` connects directly to `127.0.0.1:11434`, while the default `docker-compose.yml` keeps the `ollama` service isolated on the internal Docker network without publishing port `11434` to the host, choose one of the following two options to run this opt-in test:

### Option A: Run against a local host Ollama instance (Recommended on Windows)
1. Start the native Ollama desktop/CLI service on the host (which listens on `127.0.0.1:11434` by default) and ensure `qwen2.5:1.5b-instruct` is available locally.
2. Run the test binary from the `Hallazgo` root directory:
   ```powershell
   .\build-crow\Release\ollama_integration_test.exe
   ```

### Option B: Temporarily expose port `11434` in Docker Compose
1. Start the `ollama` and `ollama-init` services via Docker Compose so the model is provisioned automatically:
   ```powershell
   docker compose up -d ollama ollama-init
   ```
2. Use a temporary Docker Compose override file (e.g., `docker-compose.override.yml`) to map `"127.0.0.1:11434:11434"` under the `ollama` service instead of modifying the main `docker-compose.yml` (`docker compose -f docker-compose.yml -f docker-compose.override.yml up -d ollama`), then execute:
   ```powershell
   .\build-crow\Release\ollama_integration_test.exe
   ```

*(If no Ollama instance is reachable at `127.0.0.1:11434`, the test prints a skip notice and exits with code `77`.)*

---

## 5. Testing Scope & Limitations

- **Deterministic Temporary Fixtures**: Automated unit and integration tests create small, isolated temporary files (`test_dataset_temp`) and clean them up automatically. The full 1–2 GB PG-19 book dataset is not required to run `ctest`.
- **WebSocket Protocol Verification**: `integration_test` only verifies that plain HTTP `GET` requests to `/ws/search` are rejected (`404 Not Found`), which does not constitute full WebSocket frame testing. End-to-end real-time JSON stage streaming (`connected` -> `query_expansion` -> `keyword_search` -> `done`) over an upgraded WebSocket connection is manually verified through the Web UI (`clients/web/app.js`).
- **Concurrency vs. Scalability Benchmarking**: The multi-client test in `integration_test` (8 concurrent clients, 160 requests) validates thread safety and absence of race conditions, but does not replace full-scale performance benchmarking. Separate scalability experiments on the full dataset ($\ge 1\text{–}2\text{ GB}$, $\ge 10{,}000$ documents)—measuring index build time across thread counts ($1 \dots N$) and request throughput (req/s) under varying concurrent client loads—are documented in Section 3 of the coursework report.