# Hallazgo — Software Requirements Specification

**Project:** Hallazgo (Halla) — Intelligent Literary Discovery Engine  
**Status:** Draft

## 1. Introduction

Hallazgo is a literary search system that helps readers find English-language books from the PG-19 dataset using remembered plot descriptions, characters, events, or keywords.

The system combines keyword search, query expansion using a locally running Ollama model, and parallel processing.

## 2. Use Cases

### UC-1: Search for a book

**Actor:** Reader

The reader enters a description of a remembered book through the CLI or Web UI. Hallazgo expands the query using Ollama, searches the inverted index, and displays matching books ranked by relevance.

If no matching books are found, the system reports that there are no results.

### UC-2: Build the search index

**Actor:** Project operator

The operator starts indexing of the PG-19 corpus through the REST API with a specified thread count. Hallazgo immediately returns `202 Accepted` with a `job_id` and processes the documents in the background using its own thread pool. The operator can follow progress through `/ws/index`, which reports the job status and processed document count, with total count and percentage when available. `GET /api/stats` reports the current or most recent indexing status and relevant statistics, including indexing time.

If another indexing job is active, the request returns `409 Conflict`. If the corpus cannot be read, the system reports an indexing error.

## 3. Functional Requirements

- **FR-1 — Corpus processing:** The system shall process English-language documents from PG-19 and report the number of processed documents.
- **FR-2 — Inverted index:** The system shall use its own thread-safe inverted index and build it in parallel using a configurable, self-implemented thread pool.
- **FR-3 — Search:** The system shall accept keyword queries and return matching books ranked by relevance. For each query term, normalized term frequency is the term frequency divided by document length, and IDF is `log(total documents / documents containing the term)`. The score is the sum of TF-IDF scores across distinct query terms, and results are sorted by descending score. Search shall be unavailable while indexing is in progress, and the API shall report that the index is not ready.
- **FR-4 — Query expansion:** The system shall use a locally running Ollama model for query expansion when available. If Ollama is unavailable, the system shall skip query expansion and perform keyword search using the original query.
- **FR-5 — REST API:** The system shall provide:
  - `GET /api/health` — backend and Ollama status.
  - `POST /api/index` — start indexing in the background with a specified thread count; immediately return `202 Accepted` with a `job_id`. Only one indexing job may run at a time; a request during an active job shall return `409 Conflict`.
  - `POST /api/search` — search using a text query; while indexing is in progress, report that the index is not ready.
  - `GET /api/stats` — current or most recent indexing status and relevant statistics.
- **FR-6 — WebSocket:** The system shall provide `/ws/search` exclusively for search progress through the `query_expansion`, `keyword_search`, and `done` stages. It shall provide `/ws/index` for real-time indexing progress. Indexing progress messages shall report the job status and processed document count, with total count and percentage when available. Indexing states shall be `started`, `progress`, `completed`, and `error`.
- **FR-7 — User interfaces:** The system shall provide a CLI and Web UI for searching books.

## 4. Non-functional Requirements

- **NFR-1 — Thread safety:** Concurrent index access and search requests shall not corrupt index data.
- **NFR-2 — Testing:** Unit, integration, and concurrency tests shall verify the main system functionality.
- **NFR-3 — Performance:** Experiments shall measure index construction time with different thread counts and search throughput under concurrent requests.

## 5. Technical Constraints and Acceptance Criteria

### Technical Constraints

- The backend shall be implemented in C++.
- The application shall use Docker Compose.
- LLM inference shall use Ollama running locally.
- The project shall use its own configurable thread pool for parallel index construction.
- The corpus shall contain at least 10,000 English-language documents and meet the minimum dataset size of 2 GB.
- Dataset files shall remain outside the Git repository.

### Current Implementation Status

Day 3 implementation includes tokenization, an in-memory inverted index, and keyword search with TF-IDF ranking. Thread-safe index access and parallel indexing with a configurable, self-implemented thread pool remain requirements and are not yet implemented.

### Acceptance Criteria

The project is considered complete when:

1. The application starts using Docker Compose and its health endpoint works.
2. The selected corpus can be indexed, and the number of processed documents is reported. `POST /api/index` returns `202 Accepted` with a `job_id` immediately, and indexing proceeds in the background.
3. Single-threaded and multi-threaded indexing produce equivalent searchable results.
4. Keyword search returns relevant ranked results, with query expansion provided by Ollama.
5. The REST API and WebSocket endpoints work as specified: `/ws/search` streams only search-pipeline updates; `/ws/index` reports indexing status and document progress; and searches during indexing report that the index is not ready.
6. A new indexing request during an active job returns `409 Conflict`, and `/api/stats` exposes the current or most recent indexing status and relevant statistics.
7. Both CLI and Web UI support searching.
8. Automated tests verify the main functionality and concurrent operations.
9. Performance experiments are completed and their results documented.
