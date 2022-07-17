#pragma once

#include "document.h"
#include "search_server.h"
#include "paginator.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

using namespace std::string_literals;

template <typename It>
std::ostream& operator<<(std::ostream& output, std::pair<It, It> p) {
    for (auto doc = p.first; doc != p.second; ++doc) {
        output << *doc;
    }
    return output;
}

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
