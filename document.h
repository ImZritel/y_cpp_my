#pragma once

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