#include "search_server.h"

#include <execution>

using namespace std::string_literals;

SearchServer::SearchServer(std::string sws) {
    for (const std::string& word : SplitIntoWords(sws)) {
        if (!IsValidWord(word)) { 
            throw std::invalid_argument("Error: invalid word."s); 
        }
        stop_words_.insert(word);
    }
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    std::vector<std::string> words = SplitIntoWordsNoStop(document);
    if (document_id < 0 || documents_.count(document_id) > 0) { throw std::invalid_argument("Error: doc id is negative or duplicate already existing id."s); }
    else {
        added_doc_ids_.push_back(document_id);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
            docid_word_freqs_[document_id][word] += inv_word_count;
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

SearchServer::MatchingDocs SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
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
    return { matched_words, documents_.at(document_id).status };
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return added_doc_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return added_doc_ids_.end();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string, double> f{};
    if (!docid_word_freqs_.count(document_id)) {
        return f;
    }
    return docid_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy ex, int document_id) {
    if (!docid_word_freqs_.count(document_id)) {
        throw std::invalid_argument("Error: no document with such id."s);
    }
    std::vector<const std::string*> words_v(docid_word_freqs_.at(document_id).size());
    std::transform(std::execution::par,
        docid_word_freqs_.at(document_id).begin(), docid_word_freqs_.at(document_id).end(),
        words_v.begin(),
        [](const std::pair<const std::string, double>& p) {return &std::get<0>(p); });    // get the right words
    /*std::copy(std::execution::par, docid_word_freqs_.at(document_id).begin(), docid_word_freqs_.at(document_id).end(), words_v.begin());*/
    /*std::for_each(std::execution::par, docid_word_freqs_.at(document_id).begin(), docid_word_freqs_.at(document_id).end(),
        [&words_v](const auto& pair) {words_v.push_back(move(&pair.first)); });*/
    std::for_each(std::execution::par, words_v.begin(), words_v.end(), 
        [this, document_id](const auto& word) {word_to_document_freqs_.at(*word).erase(document_id); });

    added_doc_ids_.erase(find(std::execution::par, added_doc_ids_.begin(), added_doc_ids_.end(), document_id));

    docid_word_freqs_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy ex, int document_id) {
    for (auto wf : docid_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(wf.first).erase(document_id);
    }
    docid_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    auto pos = find(added_doc_ids_.begin(), added_doc_ids_.end(), document_id);
    added_doc_ids_.erase(pos);
}

void SearchServer::RemoveDocument(int document_id) {
    if (docid_word_freqs_.count(document_id)) {
        RemoveDocument(std::execution::seq, document_id);
    } else {
        throw std::invalid_argument("Error: no document with such id."s);
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
                throw std::invalid_argument("Error: invalid word in a document."s);
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
                throw std::invalid_argument("Error: word length is less than 1 or wrong '-' usage."s);
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
                throw std::invalid_argument("Error: invalid word in query."s);
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

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e) {
        std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Ошибка поиска: "s << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string& query) {
    try {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = *(search_server.begin() + index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}