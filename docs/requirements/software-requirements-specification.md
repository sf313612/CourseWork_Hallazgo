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

The operator starts indexing of the PG-19 corpus through the REST API. Hallazgo processes the documents and builds the inverted index in parallel using its own thread pool. The system reports the number of processed documents and indexing time.

If the corpus cannot be read, the system reports an indexing error.

## 3. Functional Requirements

- **FR-1 — Corpus processing:** The system shall process English-language documents from PG-19 and report the number of processed documents.
- **FR-2 — Inverted index:** The system shall use its own thread-safe inverted index and build it in parallel using a configurable, self-implemented thread pool.
- **FR-3 — Search:** The system shall accept keyword queries and return matching books ranked by relevance.
- **FR-4 — Query expansion:** The system shall use a locally running Ollama model to expand search queries before keyword retrieval.
- **FR-5 — REST API:** The system shall provide:
  - `GET /api/health` — backend and Ollama status.
  - `POST /api/index` — start indexing with a specified thread count.
  - `POST /api/search` — search using a text query.
  - `GET /api/stats` — indexing statistics.
- **FR-6 — WebSocket:** The system shall provide `/ws/search` to stream search progress through the `query_expansion`, `keyword_search`, and `done` stages.
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

### Acceptance Criteria

The project is considered complete when:

1. The application starts using Docker Compose and its health endpoint works.
2. The selected corpus can be indexed, and the number of processed documents is reported.
3. Single-threaded and multi-threaded indexing produce equivalent searchable results.
4. Keyword search returns relevant ranked results, with query expansion provided by Ollama.
5. The REST API and WebSocket search updates work as specified.
6. Both CLI and Web UI support searching.
7. Automated tests verify the main functionality and concurrent operations.
8. Performance experiments are completed and their results documented.