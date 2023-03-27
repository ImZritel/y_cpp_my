#pragma once

#include "string_processing.h"
#include "document.h"


#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <stdexcept>
#include <set>
#include <map>
#include <cmath>
#include <execution>

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string content;
    };
    std::set<std::string> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;   // INDEX word: {doc_id: word_frequency}
    std::map<int, std::map<std::string_view, double>> docid_word_freqs_;    // INDEX doc_id: {word: frequency}
    std::map<int, DocumentData> documents_;    // doc's id: {rating, status}
    std::vector<int> added_doc_ids_;    // doc_ids

    //bool IsStopWord(const std::string& word) const;
    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string& word);
    static bool IsValidWord(const std::string_view word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(std::execution::parallel_policy, const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
public:
    explicit SearchServer(std::string sws);
    explicit SearchServer(std::string_view sws);

    template <typename stringContainer>
    explicit SearchServer(const stringContainer& stop_words) {
        for (const std::string& sw : stop_words) {
            if (!IsValidWord(sw)) {
                throw std::invalid_argument("Error: invalid word."s);
            }
            stop_words_.insert(sw);
        }
    }


    //void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    //template <typename DocumentPredicate>
    //std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
    //std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
    //std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    int GetDocumentCount() const;

    using MatchingDocs = std::tuple<std::vector<std::string>, DocumentStatus>;
    // MatchingDocs MatchDocument(const std::string& raw_query, int document_id) const;
     //MatchingDocs MatchDocument(std::execution::parallel_policy ex, const std::string& raw_query, int document_id) const;
     //MatchingDocs MatchDocument(std::execution::sequenced_policy ex, const std::string& raw_query, int document_id) const;
    using MatchingDocs_sv = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchingDocs_sv MatchDocument(const std::string_view raw_query, int document_id) const;
    MatchingDocs_sv MatchDocument(std::execution::parallel_policy ex, const std::string_view raw_query, int document_id) const;
    MatchingDocs_sv MatchDocument(std::execution::sequenced_policy ex, const std::string_view raw_query, int document_id) const;

    std::vector<int>::const_iterator begin() const;
    std::vector<int>::const_iterator end() const;
    //const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(std::execution::parallel_policy ex, int document_id);
    void RemoveDocument(std::execution::sequenced_policy ex, int document_id);
    void RemoveDocument(int document_id);
};

/*
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
    std::vector<Document> response;
    Query query = ParseQuery(raw_query);
    response = FindAllDocuments(query, document_predicate);
    std::sort(response.begin(), response.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (response.size() > MAX_RESULT_DOCUMENT_COUNT) {
        response.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return response;
}*/
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    std::vector<Document> response;
    std::string raw_query_s;
    raw_query_s = raw_query;
    Query query = ParseQuery(raw_query_s);
    response = FindAllDocuments(query, document_predicate);
    std::sort(response.begin(), response.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (response.size() > MAX_RESULT_DOCUMENT_COUNT) {
        response.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return response;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const auto& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const auto& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(Document{
            document_id,
            relevance,
            documents_.at(document_id).rating
            });
    }
    return matched_documents;
}

void RemoveDuplicates(SearchServer& search_server);

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view query);