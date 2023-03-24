#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> ids_to_remove;
    std::set<std::set<std::string_view>> words_sieve;    // to check if the words already appeared
    for (const int doc_id : search_server) {
        std::set<std::string_view> candidate;
        for (const auto& wf : search_server.GetWordFrequencies(doc_id)) {
            candidate.insert(wf.first);
        };
        if (words_sieve.count(candidate)) {
                ids_to_remove.insert(doc_id);
        }
        else {
            std::set<std::string_view> candidate_s;
            for (const auto& sv : candidate) {
                candidate_s.insert(sv);
            }
            words_sieve.insert(candidate_s);
        }
    }

    for (int id : ids_to_remove) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id "s << id << "\n"s;
    }
    std::cout << std::flush;
}
