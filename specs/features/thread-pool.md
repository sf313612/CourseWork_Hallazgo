# Thread Pool and Parallel Indexing

## Purpose

This feature builds the PG-19 search index in parallel using Hallazgo's own configurable thread pool. It supports the SRS requirements for thread-safe index access and equivalent search results across worker counts.

## Requirements

- The system shall use a custom thread pool with a configurable worker count of at least one.
- The system shall provide its own task queue logic, synchronized with a mutex and condition variable. `std::queue` alone shall not serve as the complete queue implementation.
- Workers shall execute accepted tasks safely. Graceful shutdown shall finish queued tasks, stop and join all workers, and reject new tasks after shutdown begins.
- Task failures shall be reported to the submitter or indexing operation through exception propagation. A task failure shall not silently terminate a worker thread and shall be reported to the caller.
- Concurrent updates and reads of the shared `InvertedIndex` shall not corrupt index data. Shared `KeywordSearch` index metadata shall also be safe during concurrent access.
- Documents shall be indexed in parallel. Search results shall contain the same documents and preserve the same ranking regardless of the configured worker count. Small floating-point differences in scores are acceptable.
- Indexing time shall be measured with 1, 2, 4, and 8 workers where the hardware allows. Record the worker count, document set, and elapsed time for each run.
- Search requests shall not be served while the index is being built, and the index shall never be exposed to search operations in an inconsistent state during construction.

## Implementation Choices

- CMake currently does not set `CXX_STANDARD`; the effective standard depends on the compiler and its defaults. Do not assume C++17 or change the standard as part of this feature. Use synchronization supported by the configured toolchain. `std::mutex` is a compatible baseline; `std::shared_mutex` may be used only when C++17 or later is explicitly configured.
- The task queue may use a suitable standard container internally, but Hallazgo must implement queue synchronization, waiting, task acceptance, and shutdown behavior itself.
- The document scheduling strategy and index locking strategy are design choices. They must preserve the requirements above and be verified by tests.
- Do not require task cancellation or staging-index publication as part of this feature; those behaviors are not required by the current SRS.

## Verification

- Unit tests shall cover worker-count validation, task execution, graceful shutdown, rejection after shutdown, and exception propagation.
- Concurrent index tests shall check that updates remain valid and search results are not corrupted.
- Indexing tests shall compare the searchable results produced with one worker and multiple workers.
- Initial tests may use in-memory documents. Integration with reading actual PG-19 files is still required for the complete indexing pipeline.

