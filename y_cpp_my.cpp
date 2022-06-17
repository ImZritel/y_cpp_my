#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

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
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
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
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
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

    //*****************************
    template <typename FilterFunction>
    vector<Document> FindTopDocuments(const string& raw_query, FilterFunction ff) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });

        //add filtered matched_documents to result
        map<int, DocumentStatus> statuses;
        for (const auto& doc : matched_documents) {
            statuses[doc.id] = documents_.at(doc.id).status;
        }
        vector<Document> filtered_docs;
        for (const auto& md : matched_documents) {
            if (ff(md.id, statuses[md.id], md.rating)) { filtered_docs.push_back({ md.id, md.relevance, md.rating }); }
        }

        if (filtered_docs.size() > MAX_RESULT_DOCUMENT_COUNT) {
            filtered_docs.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return filtered_docs;
    }

    //*****************************
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_to_compare, int rating) { return status_to_compare == status; });
    }


    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
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
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
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
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};


// -------- Начало модульных тестов поисковой системы ----------


// =============================================================
// TESTING framework
// =============================================================

// Defining of output operator<<
template <typename t1, typename t2>
ostream& operator<<(ostream& out, pair<t1, t2> p) {
    out << p.first << ": ";
    out << p.second;
    return out;
}

template <typename el_type>
ostream& operator<<(ostream& out, vector<el_type> v) {
    out << "["s;
    bool first = true;
    for (auto el : v) {

        if (!first) {
            out << ", "s;
        }
        out << el;
        first = false;
    }
    out << "]"s;
    return out;
}

template <typename el_type>
ostream& operator<<(ostream& out, set<el_type> s) {
    out << "{"s;
    bool first = true;
    for (auto el : s) {

        if (!first) {
            out << ", "s;
        }
        out << el;
        first = false;
    }
    out << "}"s;
    return out;
}

template <typename t1, typename t2>
ostream& operator<<(ostream& out, map<t1, t2> m) {
    out << "{"s;
    bool first = true;
    for (auto el : m) {

        if (!first) {
            out << ", "s;
        }
        out << el;
        first = false;
    }
    out << "}"s;
    return out;
}

//template functions and macroses
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))


void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// =============================================================
// END OF TESTING framework
// =============================================================


// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the the big brown deogi named shen city"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        ASSERT(server.FindTopDocuments("in"s).empty());
        ASSERT_EQUAL(server.FindTopDocuments("the cat"s)[0].id, doc_id);
    }
}

void TestMinusWordsFiltersResults() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        const auto found_docs = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(found_docs.size(), 2);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        const auto found_docs = server.FindTopDocuments("city -big"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

void TestSortResultsByRelevance() {
    const int doc_id = 42;
    const string content = "a b c d"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(2, "e f g z"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        const auto found_docs = server.FindTopDocuments("e f"s);
        ASSERT_EQUAL(found_docs.size(), 4);
        ASSERT_EQUAL(found_docs[0].id, 2);
        ASSERT_EQUAL(found_docs[1].id, 0);
        ASSERT_EQUAL(found_docs[2].id, 1);
        ASSERT_EQUAL(found_docs[3].id, 43);
    }
}

void TestComputeRating() {
    const int doc_id = 42;
    const string content = "a b c d"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::ACTUAL, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, -30, 2 });
        const int rating_1 = (0 - 30 + 2) / 3;
        server.AddDocument(2, "e f g z"s, DocumentStatus::ACTUAL, { 0, 31, 32 });
        const int rating_0 = (0 + 31 + 32) / 3;
        const auto found_docs = server.FindTopDocuments("e -c"s);
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].rating, rating_0);
        ASSERT_EQUAL(found_docs[1].rating, rating_1);
    }
}

void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::IRRELEVANT, { 0, 1, 2 });
        string q1 = "in the city"s;
        string q2 = ""s;
        string q32 = " deogi -brown";
        const auto found_docs = server.MatchDocument(q1, 42);
        tuple<vector<string>, DocumentStatus> ans1 = { {"city"s}, DocumentStatus::ACTUAL };
        ASSERT(found_docs == ans1);
        const auto found_docs2 = server.MatchDocument(q2, 43);
        tuple<vector<string>, DocumentStatus> ans2 = { {}, DocumentStatus::IRRELEVANT };
        ASSERT(found_docs2 == ans2);
        const auto found_docs3 = server.MatchDocument(q32, 43);
        tuple<vector<string>, DocumentStatus> ans3 = { {}, DocumentStatus::IRRELEVANT};
        ASSERT(found_docs3 == ans3);
    }
}

void TestPredicateFiltering() {
    //Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
    const int doc_id = 42;
    const string content = "a b c d"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::ACTUAL, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, 
            [](const int& id, const DocumentStatus& ds, const int& rating) 
            {return id > 1; });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, 43);
        ASSERT_EQUAL(found_docs[1].id, 2);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, 
            [](const int& id, const DocumentStatus& ds, const int& rating) 
            {return static_cast<int>(ds) > 1; });
        ASSERT_EQUAL(found_docs.size(), 2);
        ASSERT_EQUAL(found_docs[0].id, 2);
        ASSERT_EQUAL(found_docs[1].id, 1);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, 
            [](const int& id, const DocumentStatus& ds, const int& rating) 
            {return ds == DocumentStatus::ACTUAL; });
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 43);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, 
            [](const int& id, const DocumentStatus& ds, const int& rating) 
            {return rating < -5; });
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }
}

void TestRelevanceComputing() {
    //Корректное вычисление релевантности найденных документов.
    const int doc_id = 42;
    const string content = "a b c d"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::ACTUAL, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, 
            [](const int& id, const DocumentStatus& ds, const int& rating) 
            {return id > 1; });
        ASSERT_EQUAL(found_docs.size(), 2);
        const double relevance_0 = (1.0 / 4) * log(5.0 / 4);
        const double relevance_1 = (1.0 / 5) * log(5.0 / 4);
        ASSERT_EQUAL(found_docs[0].relevance, relevance_0);
        ASSERT_EQUAL(found_docs[1].relevance, relevance_1);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWordsFiltersResults();
    TestMatching();
    TestSortResultsByRelevance();
    TestComputeRating();
    TestPredicateFiltering();
    TestRelevanceComputing();
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}