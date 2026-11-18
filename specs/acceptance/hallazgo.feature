Feature: Hallazgo book search and indexing
  Readers find books from remembered descriptions, and project operators build the searchable corpus.

  Rule: Search for books

    Scenario: Search with Ollama query expansion
      Given a usable search index is available
      And the health endpoint reports that Ollama is available
      When the reader submits a keyword search with a remembered plot description
      Then Hallazgo expands the query using Ollama
      And the API returns HTTP 200 with relevance-ranked results and search_time_ms
      And higher result scores indicate more relevant results

    Scenario: Search using the original query when Ollama is unavailable
      Given a usable search index is available
      And the health endpoint returns status "ok" and ollama false
      When the reader submits a keyword search with a remembered plot description
      Then Hallazgo skips query expansion
      And Hallazgo searches using the original query
      And the API returns HTTP 200 with the search response

    Scenario: Search finds no matching books
      Given a usable search index is available
      When the reader submits a keyword query with no matching books
      Then the API returns HTTP 200
      And the results array is empty
      And the response includes search_time_ms

    Scenario Outline: Search is unavailable when no usable index can serve requests
      Given <index_condition>
      When the reader submits a keyword search
      Then the API returns HTTP 503
      And the error code is "index_not_ready"

      Examples:
        | index_condition                                                |
        | no indexing job has completed successfully                     |
        | indexing is in progress                                        |
        | the latest indexing job failed without a previously usable index |

    Scenario: Search is unavailable during re-indexing despite a previously successful index
      Given a previously successful index exists
      And a re-indexing job is in progress
      When the reader submits a keyword search
      Then the API returns HTTP 503
      And the error code is "index_not_ready"

  Rule: Build the search index

    Scenario: Start indexing and observe progress and completion
      Given the PG-19 corpus is readable
      And no indexing job is active
      When the operator submits POST /api/index with threads set to 4
      Then the API returns HTTP 202 with a job_id
      And indexing continues in the background
      And /ws/index reports status "started" and processed_document_count
      And /ws/index reports status "progress" and processed_document_count
      And /ws/index includes total_document_count and percentage when available
      When indexing completes
      Then /ws/index reports status "completed" and processed_document_count
      And /api/stats reports indexing_status "completed"
      And /api/stats reports processed_document_count and total_document_count
      And /api/stats reports index_build_time_ms and thread_count 4

    Scenario: Reject a duplicate indexing request
      Given an indexing job is active
      When the operator submits another POST /api/index request
      Then the API returns HTTP 409
      And the error code is "indexing_in_progress"

    Scenario: Report an indexing error when the corpus cannot be read
      Given an indexing job has been accepted
      And the corpus cannot be read by the indexing job
      When the indexing job reports its outcome
      Then /ws/index reports status "error"
      And /api/stats reports indexing_status "error"