#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> ids_to_remove;
    std::map<std::set<std::string>, int> words_sieve;    // to check if the words have already appeared
    for (const int doc_id : search_server) {
        std::set<std::string> candidate;
        for (auto& wf : search_server.GetWordFrequencies(doc_id)) {
            candidate.insert(wf.first);
        };
        if (words_sieve.count(candidate)) {
            if (words_sieve[candidate] > doc_id) {
                ids_to_remove.insert(words_sieve[candidate]);
                words_sieve[candidate] = doc_id;
            }
            else {
                ids_to_remove.insert(doc_id);
            }
        }
        else {
            words_sieve[candidate] = doc_id;
        }
    }

    for (int id : ids_to_remove) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id "s << id << "\n"s;
    }
    std::cout << std::flush;
}
