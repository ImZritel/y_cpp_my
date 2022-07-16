#include "document.h"

Document::Document() {
    id = 0;
    relevance = 0.0;
    rating = 0;
}

Document::Document(int doc_id, double doc_relevance, int doc_rating) {
    id = doc_id;
    relevance = doc_relevance;
    rating = doc_rating;
}