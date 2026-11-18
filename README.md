# Hallazgo

**Short name:** Halla  
**Full title:** Intelligent Literary Discovery Engine  
**Tagline:** Find the story you remember.

Hallazgo is a literary search and discovery project for English-language books from the PG-19 dataset. It aims to help readers find books from remembered details.

## Current Status

Day 3 components are implemented: text tokenization, an in-memory inverted index, and keyword search with TF-IDF ranking. Unit tests for these components are configured with CMake and a self-contained test harness.

Thread-safe index access and parallel indexing with a custom thread pool are still requirements and have not been implemented.

## Planned Technologies

C++, CMake, Docker Compose, Ollama, REST API, and WebSocket.

Development follows Specification-Driven Development (SDD) and Test-Driven Development (TDD).
