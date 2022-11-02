#include <algorithm>
#include <string>
#include <execution>

#include "process_queries.h"
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
        queries.begin(), queries.end(), result.begin(),
        [&search_server](std::string q) { return search_server.FindTopDocuments(q); });
    return result;
}

// do transform_reduce: transform vectors to lists, reduce: link those lists list.splice(list.end(), next_list)
std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<Document> result;
    for (auto v1_el : ProcessQueries(search_server, queries)) {
        for (auto v2_el : v1_el) {
            result.push_back(std::move(v2_el));
        }
    }
    return result;
}