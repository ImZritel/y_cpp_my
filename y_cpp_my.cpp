#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
inline static constexpr int INVALID_DOCUMENT_ID = -1;

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
            stop_words_.insert(word);
        }
    }

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        for (const string& sw : stop_words) {
            stop_words_.insert(sw);
        }
    }

    bool AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        vector<string> words;
        bool is_ok = true;
        is_ok = SplitIntoWordsNoStop(document, words);
        if (document_id < 0 || documents_.count(document_id) > 0) { is_ok = false; }

        if (!is_ok) { return is_ok; }
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
        return is_ok;
    }

    template <typename DocumentPredicate>
    optional<vector<Document>> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        Query query;
        vector<Document> response;
        bool is_ok = ParseQuery(raw_query, query);
        if (!is_ok) {
            return nullopt;
        }
        else {
            response = FindAllDocuments(query, document_predicate);

            sort(response.begin(), response.end(),
                [](const Document& lhs, const Document& rhs) {
                    if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                        return lhs.rating > rhs.rating;
                    }
                    else {
                        return lhs.relevance > rhs.relevance;
                    }
                });
            if (response.size() > MAX_RESULT_DOCUMENT_COUNT) {
                response.resize(MAX_RESULT_DOCUMENT_COUNT);
            }
        }
        return response;
    }

    optional<vector<Document>> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    optional<vector<Document>> FindTopDocuments (const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    optional<tuple<vector<string>, DocumentStatus>> MatchDocument(const string& raw_query, int document_id) const {
        Query query;
        bool is_ok = ParseQuery(raw_query, query);
        if (!is_ok) {
            return nullopt;
        }
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
            return INVALID_DOCUMENT_ID;
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

    [[nodiscard]] bool SplitIntoWordsNoStop(const string& text, vector<string>& words) const {
        bool is_ok = true;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                if (IsValidWord(word)) {
                    words.push_back(word);
                }
                else {
                    is_ok = false;
                    return is_ok;
                }
            }
            
        }
        return is_ok;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    bool ParseQueryWord(string text, QueryWord& result) const {
        bool is_ok = true;
        bool is_minus = false;
        // Word shouldn't be empty
        if (text.size() > 0) {
            if (text[0] == '-') {
                if (text.size() == 1 || text[1] == '-') {
                    is_ok = false;
                    return is_ok;
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
        return is_ok;
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    [[nodiscard]] bool ParseQuery(const string& text, Query& result) const {
        bool is_ok = true;
        for (const string& word : SplitIntoWords(text)) {
            QueryWord query_word; 
            is_ok = ParseQueryWord(word, query_word);
            if (!is_ok) { return is_ok; }
            else {
                if (!query_word.is_stop) {
                    if (!IsValidWord(query_word.data)) {
                        return is_ok = false;
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
                //else if (!IsValidWord(query_word.data)) {
                //    return is_ok = false;
                //}
            }
        }
        return is_ok;
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

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server("и в на"s);
    //cout << "String 1 in english"s << endl;
    //cout << "—трока 2 на русском"s << endl;
    // явно игнорируем результат метода AddDocument, чтобы избежать предупреждени€
    // о неиспользуемом результате его вызова
    (void)search_server.AddDocument(1, "пушистый кот пушистый хвост и человек-паук"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

    if (!search_server.AddDocument(1, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 })) {
        cout << "Error: doc id already exists"s << endl;
    }

    if (!search_server.AddDocument(-3, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 })) {
        cout << "Error: negative doc id"s << endl;
    }

    if (!search_server.AddDocument(4, "большой пЄс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 })) {
        cout << "Error: spec symbols are occuered"s << endl;
    }

    if (!search_server.AddDocument(5, "-\x13"s, DocumentStatus::ACTUAL, { 1, 3, 2 })) {
        cout << "Error: spec symbols are occuered"s << endl;
    }
    
    if (auto documents = search_server.FindTopDocuments("хвост человек-паук"s)) {
        for (const Document& document : *documents) {
            PrintDocument(document);
        }
    }
    else {
        cout << "Query error"s << endl;
    }
}