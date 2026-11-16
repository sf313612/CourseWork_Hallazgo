# Hallazgo — Project Charter

**Project name:** Hallazgo  
**Short name:** Halla  
**Full title:** Hallazgo — Intelligent Literary Discovery Engine  
**Tagline:** Find the story you remember.  
**Project type:** University course project — Parallel Computing  
**Status:** Draft  
**Development approach:** Specification-Driven Development (SDD) and Test-Driven Development (TDD)

## 1. Project Background

Readers sometimes remember a book through its plot, characters, themes, or particular events but cannot recall its title or author. Traditional keyword-based search can be insufficient when the user's description differs from the wording used in the original text.

Hallazgo aims to address this problem by providing a literary discovery system that combines keyword-based retrieval, query expansion using a locally deployed Large Language Model (LLM), and parallel processing.

## 2. Problem Statement

Users need a way to discover literary works using incomplete or approximate descriptions of their content.

The system must process a substantial English-language literary corpus efficiently and support concurrent search requests while demonstrating practical applications of parallel computing.

## 3. Project Goal

Design and implement a containerized literary search engine that allows users to discover English-language books from the PG-19 dataset using natural-language descriptions and keywords.

The system will demonstrate parallel inverted-index construction, thread-safe data processing, concurrent request handling, and LLM-assisted query expansion.

## 4. Target Users

- Readers who remember a book's content but not its title.
- Users exploring books by themes, topics, or plot descriptions.
- Users interested in discovering relevant literary works through keyword-based search.

## 5. Project Scope

### 5.1 In Scope

The project will include:

- PG-19 dataset ingestion and document processing.
- Text normalization and tokenization.
- A custom thread-safe inverted index.
- A self-implemented thread pool.
- Parallel inverted-index construction with configurable thread count.
- Keyword-based search with result ranking.
- Query expansion using a locally deployed Ollama LLM.
- A REST API for indexing, searching, health checks, and statistics.
- WebSocket-based search pipeline progress updates.
- A command-line search client.
- A web-based search interface.
- Docker Compose deployment.
- Unit, integration, and concurrency testing.
- Performance and scalability experiments.

### 5.2 Out of Scope

The initial version will not include:

- Neural embedding-based semantic search.
- Model training or fine-tuning.
- Personalized recommendations.
- User accounts and authentication.
- A production-scale distributed search infrastructure.
- Commercial deployment.

These features may be considered for future development.

## 6. Key Stakeholders

| Stakeholder | Responsibility |
|---|---|
| Student / Developer | Requirements, design, implementation, testing, documentation |
| Course Instructor | Academic requirements, evaluation, feedback |
| End User | Uses the system to discover literary works |

## 7. Technical Constraints

- Backend must use a compiled language: C++, Java, or C#. The selected language is C++.
- The system must use Docker Compose.
- The LLM must run locally through Ollama.
- Parallel indexing must use a self-implemented thread pool.
- The inverted index must support thread-safe operations.
- The project must expose the specified REST API and WebSocket interface.
- The dataset must contain at least 10,000 English-language documents.
- The dataset size requirement is ambiguous in the course materials; the stricter interpretation is at least 2 GB.
- Dataset files must remain outside the Git repository.

## 8. Success Criteria

The project will be considered successful when:

1. The application can be started using Docker Compose.
2. The health endpoint reports backend availability and Ollama status.
3. The system can ingest and index the selected literary corpus.
4. The inverted index supports concurrent-safe access.
5. Index construction uses the custom thread pool and configurable parallelism.
6. Users can search the corpus through keyword queries.
7. LLM-based query expansion can influence search results.
8. The WebSocket interface streams actual search pipeline stages.
9. Both CLI and Web UI clients work.
10. Automated tests verify the main functionality and concurrency behavior.
11. Performance experiments measure indexing speed and concurrent request throughput.
12. Architecture, UML diagrams, documentation, and implementation results are prepared for the course defense.

## 9. Development Approach

The project will follow Specification-Driven Development (SDD) and Test-Driven Development (TDD).

The development lifecycle will include:

1. Project initiation and repository setup.
2. Requirements analysis and specification.
3. System architecture and API design.
4. Test planning and test-first implementation.
5. Incremental implementation and verification.
6. Integration and system testing.
7. Performance evaluation.
8. Documentation and defense preparation.

Each implementation stage must have explicit acceptance criteria and verification results.

## 10. Assumptions

- The PG-19 dataset will be used as the primary literary corpus.
- Ollama and a suitable locally runnable LLM will be available in the deployment environment.
- The available hardware will determine the practical maximum thread count and achievable performance.
- The initial system will prioritize functional completeness, correctness, and demonstrable parallelism over production-level scalability.

## 11. Expected Deliverables

- C++ backend source code.
- CLI and Web UI clients.
- Docker Compose configuration.
- Automated test suite.
- SDD artifacts and technical specifications.
- UML diagrams.
- Performance experiment results.
- README and project report.
- Git repository with meaningful Conventional Commits history.