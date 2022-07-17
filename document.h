#pragma once

#include <iostream>

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