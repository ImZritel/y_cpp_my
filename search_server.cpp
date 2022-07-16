#include "search_server.h"


SearchServer::SearchServer(std::string sws) {
    for (const std::string& word : SplitIntoWords(sws)) {
        if (!IsValidWord(word)) { throw std::invalid_argument("Error: invalid word."); }
        stop_words_.insert(word);
    }
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    std::vector<std::string> words = SplitIntoWordsNoStop(document);
    if (document_id < 0 || documents_.count(document_id) > 0) { throw std::invalid_argument("Error: doc id is negative or duplicate already existing id."); }
    else {
        added_doc_ids_.push_back(document_id);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    std::tuple<std::vector<std::string>, DocumentStatus> result = { matched_words, documents_.at(document_id).status };
    return result;
}

int SearchServer::GetDocumentId(int index) const {
    //what is this method for??
    if (index >= added_doc_ids_.size()) {
        throw std::out_of_range("Error: index is out of range.");
    }
    else {
        return added_doc_ids_[index];
    }
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            if (IsValidWord(word)) {
                words.push_back(word);
            }
            else {
                throw std::invalid_argument("Error: invalid word in a document.");
            }
        }

    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    QueryWord result;
    bool is_minus = false;
    // Word shouldn't be empty
    if (text.size() > 0) {
        if (text[0] == '-') {
            if (text.size() == 1 || text[1] == '-') {
                throw std::invalid_argument("Error: word length is less than 1 or wrong '-' usage.");
            }
            else {
                is_minus = true;
                text = text.substr(1);
            }
        }
        result = {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    else {
        result = {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    return result;
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        QueryWord query_word;
        query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (!IsValidWord(query_word.data)) {
                throw std::invalid_argument("Error: invalid word in query.");
            }
            else {
                if (query_word.is_minus) {
                    result.minus_words.insert(query_word.data);
                }
                else {
                    result.plus_words.insert(query_word.data);
                }
            }
        }
    }
    return result;
}


// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
