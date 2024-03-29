#pragma once

#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"


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
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;   // INDEX word: {doc_id: word_frequency}
    std::map<int, std::map<std::string_view, double>> docid_word_freqs_;    // INDEX doc_id: {word: frequency}
    std::map<int, DocumentData> documents_;    // doc's id: {rating, status}
    std::set<int> added_doc_ids_;    // doc_ids

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
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
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

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    //par/seq
    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const Policy& exPol, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& exPol, const std::string_view raw_query, DocumentStatus status) const;
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& exPol, const std::string_view raw_query) const;

    //not specified policy
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    int GetDocumentCount() const;

    using MatchingDocs_sv = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchingDocs_sv MatchDocument(const std::string_view raw_query, int document_id) const;
    MatchingDocs_sv MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const;
    MatchingDocs_sv MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(std::execution::parallel_policy ex, int document_id);
    void RemoveDocument(std::execution::sequenced_policy ex, int document_id);
    void RemoveDocument(int document_id);
};

template <typename Policy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& exPol, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    std::vector<Document> response;
    std::string raw_query_s;
    raw_query_s = raw_query;
    Query query = ParseQuery(raw_query_s);
    response = FindAllDocuments(exPol, query, document_predicate);
    std::sort(exPol, response.begin(), response.end(),
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
template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& exPol, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(exPol, raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}
template <typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& exPol, const std::string_view raw_query) const {
    return FindTopDocuments(exPol, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}


// Find all docs
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
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
//par
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(5000);
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [this, &document_to_relevance, &document_predicate](const auto& word) {
        if (word_to_document_freqs_.count(word)) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
        });
    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](const auto& word) {
        if (word_to_document_freqs_.count(word)) {
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        });

    std::vector<Document> matched_documents;
    const auto res = document_to_relevance.BuildOrdinaryMap();
    for_each(std::execution::seq, res.begin(), res.end(), [&matched_documents, this](const auto p) {
        matched_documents.push_back(Document{
        p.first,
        p.second,
        documents_.at(p.first).rating
            });
        });
    return matched_documents;
}
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

void RemoveDuplicates(SearchServer& search_server);

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string_view query);