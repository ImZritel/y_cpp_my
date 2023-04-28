#include <algorithm>
#include <string>
#include <execution>

#include "process_queries.h"
#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string_view> queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
        queries.begin(), queries.end(), result.begin(),
        [&search_server](std::string_view q) { return search_server.FindTopDocuments(q); });
    return result;
}



std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::string_view > sv_q(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), sv_q.begin(), [](const auto& str) {return std::string_view(str);});
    return ProcessQueries(search_server, sv_q);
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string_view> queries) {
    std::vector<Document> result;
    for (auto v1_el : ProcessQueries(search_server, queries)) {
        for (auto v2_el : v1_el) {
            result.push_back(std::move(v2_el));
        }
    }
    return result;
}

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