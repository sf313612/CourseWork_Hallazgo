#include "inverted_index.hpp"

namespace Hallazgo_inverted_index {

    std::vector<Posting> InvertedIndex::find(const std::string& term) {
        for (const TermEntry& entry : entries) {
            if (entry.term == term) {
                return entry.postings;
            }
        }

        return {};
    }

    void InvertedIndex::addDocument(
        const std::string& document_id,
        const std::vector<std::string>& tokens
    ) {
        for (const std::string& token : tokens) {

            // 1. Шукаємо слово в індексі
            TermEntry* found_entry = nullptr;

            for (TermEntry& entry : entries) {
                if (entry.term == token) {
                    found_entry = &entry;
                    break;
                }
            }

            // 2. Якщо слово вже існує
            if (found_entry != nullptr) {

                bool posting_found = false;

                // Шукаємо документ у списку цього слова
                for (Posting& posting : found_entry->postings) {
                    if (posting.document_id == document_id) {
                        posting.term_frequency++;
                        posting_found = true;
                        break;
                    }
                }

                // Якщо слова є, але документа ще немає
                if (!posting_found) {
                    Posting new_posting;
                    new_posting.document_id = document_id;
                    new_posting.term_frequency = 1;

                    found_entry->postings.push_back(new_posting);
                }

            } else {
                // 3. Якщо слова ще немає — створюємо новий запис
                TermEntry new_entry;
                new_entry.term = token;

                Posting new_posting;
                new_posting.document_id = document_id;
                new_posting.term_frequency = 1;

                new_entry.postings.push_back(new_posting);
                entries.push_back(new_entry);
            }
        }
    }

}