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
        [search_server](std::string q) { return search_server.FindTopDocuments(q); });
    return result;
};