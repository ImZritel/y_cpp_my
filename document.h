#pragma once

#include <iostream>
#include <vector>

struct Document {
    explicit Document();
    explicit Document(int doc_id, double doc_relevance, int doc_rating);

    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

std::ostream& operator<<(std::ostream& output, Document document);

template <typename It>
std::ostream& operator<<(std::ostream& output, std::pair<It, It> p) {
    for (auto doc = p.first; doc != p.second; ++doc) {
        output << *doc;
    }
    return output;
}

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view> words, DocumentStatus status);