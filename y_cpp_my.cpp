#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <stdexcept>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ' && !word.empty()) {
            words.push_back(word);
            word = "";
        }
        else if (c != ' ') {
            word += c;
        }
    }

    words.push_back(word);

    return words;
}

struct Document {
    explicit Document() {
        id = 0;
        relevance = 0.0;
        rating = 0;
    }

    explicit Document(int doc_id, double doc_relevance, int doc_rating) {
        id = doc_id;
        relevance = doc_relevance;
        rating = doc_rating;
    }
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
class SearchServer {
public:
    explicit SearchServer(string sws) {
        for (const string& word : SplitIntoWords(sws)) {
            if (!IsValidWord(word)) { throw invalid_argument("Error: invalid word."s); }
            stop_words_.insert(word);
        }
    }

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        for (const string& sw : stop_words) {
            if (!IsValidWord(sw)) { throw invalid_argument("Error: invalid word."s); }
            stop_words_.insert(sw);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        vector<string> words = SplitIntoWordsNoStop(document);
        if (document_id < 0 || documents_.count(document_id) > 0) { throw invalid_argument("Error: doc id is negative or duplicate already existing id."s); }
        else {
            added_doc_ids_.push_back(document_id);
            const double inv_word_count = 1.0 / words.size();
            for (const string& word : words) {
                word_to_document_freqs_[word][document_id] += inv_word_count;
            }
            documents_.emplace(document_id,
                DocumentData{
                    ComputeAverageRating(ratings),
                    status
                });
        }
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        vector<Document> response;
        Query query = ParseQuery(raw_query);
        response = FindAllDocuments(query, document_predicate);
        sort(response.begin(), response.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    vector<Document> FindTopDocuments (const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        tuple<vector<string>, DocumentStatus> result = { matched_words, documents_.at(document_id).status };
        return result;
    }

    int GetDocumentId(int index) const {
        //what is this method for??
        if (index >= added_doc_ids_.size()) {
            throw out_of_range("Error: index is out of range.");
        }
        else {
            return added_doc_ids_[index];
        }
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> added_doc_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                if (IsValidWord(word)) {
                    words.push_back(word);
                }
                else {
                    throw invalid_argument("Error: invalid word in a document."s);
                }
            }
            
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        QueryWord result;
        bool is_minus = false;
        // Word shouldn't be empty
        if (text.size() > 0) {
            if (text[0] == '-') {
                if (text.size() == 1 || text[1] == '-') {
                    throw invalid_argument("Error: word length is less than 1 or wrong '-' usage."s);
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

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query result;
        for (const string& word : SplitIntoWords(text)) {
            QueryWord query_word; 
            query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (!IsValidWord(query_word.data)) {
                    throw invalid_argument("Error: invalid word in query."s);
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
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(Document{
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

//******************************

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}