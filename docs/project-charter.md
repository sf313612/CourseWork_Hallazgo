# Hallazgo — Project Charter

**Project name:** Hallazgo  
**Short name:** Halla  
**Full title:** Intelligent Literary Discovery Engine  
**Tagline:** Find the story you remember.  
**Project type:** University course project — Parallel Computing  
**Status:** Draft

## 1. Project Background

Readers sometimes remember a book through its plot, characters, themes, or particular events but cannot recall its title or author. Traditional keyword search may fail when the user's description differs from the wording in the original text.

Hallazgo aims to make literary discovery easier by allowing users to search for books using remembered descriptions and keywords. It combines keyword-based search, query expansion using a locally running LLM, and parallel processing.

## 2. Project Goal and Target Users

The goal is to design and implement a literary search engine for English-language books from the PG-19 dataset.

The system is intended for readers who want to find a book based on what they remember about its content, as well as users exploring books by plot, topics, or themes.

Hallazgo will demonstrate how parallel computing can be used to build a search index and handle multiple search requests.

## 3. Project Scope

The project will include:

- Processing English-language documents from PG-19.
- Building a custom thread-safe inverted index using a self-implemented thread pool.
- Parallel index construction with a configurable number of threads.
- Keyword search with ranked results and LLM-assisted query expansion through Ollama.
- REST API and WebSocket updates about search progress.
- CLI and Web UI for interacting with the system.
- Docker Compose deployment, automated tests, and performance experiments.

The initial version will focus on keyword-based retrieval rather than neural semantic search. Model training, personalized recommendations, user accounts, and distributed search infrastructure are outside the project scope.

## 4. Technical Constraints and Success Criteria

The backend will be implemented in C++. The system will use Docker Compose, and the LLM will run locally through Ollama. The corpus will contain at least 10,000 English-language documents from PG-19 and have a minimum size of 2 GB. Dataset files will remain outside the Git repository.

The project will be considered successful when:

1. The application can be started using Docker Compose and reports its health status.
2. The selected corpus can be processed and indexed.
3. The inverted index supports thread-safe operations and is built in parallel using the project's own configurable thread pool.
4. Users can search for books and receive ranked results, with query expansion provided by Ollama.
5. The REST API, WebSocket updates, CLI, and Web UI work as intended.
6. Automated tests verify the main functionality and concurrent operations.
7. Performance experiments evaluate indexing speed and search throughput under concurrent requests.
8. The implementation, architecture diagrams, documentation, and results are prepared for the course defense.

## 5. Development Approach

Development will follow Specification-Driven Development (SDD) and Test-Driven Development (TDD). Requirements and design will guide implementation, while tests and acceptance criteria will be used to verify each major feature.

The project will be developed incrementally, with meaningful Git commits and documented testing and performance results.