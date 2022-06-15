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
/*
TODO:
    Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
    Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
    Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
    Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
    Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
    Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
    Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
    Поиск документов, имеющих заданный статус.
    Корректное вычисление релевантности найденных документов.
*/

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the the big brown deogi named shen city"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        assert(server.FindTopDocuments("in"s).empty());
    }
}

/*
Разместите код остальных тестов здесь
*/

void TestMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, {0, 1, 2});
        const auto found_docs = server.FindTopDocuments("city"s);
        assert(found_docs.size() == 2);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "the big brown deogi named shen city"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        const auto found_docs = server.FindTopDocuments("city -big"s);
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }
}

void TestRelevance() {
    const int doc_id = 42;
    const string content = "a b c d"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::ACTUAL, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, 3, 2 });
        server.AddDocument(2, "e f g z"s, DocumentStatus::ACTUAL, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e -c"s);
        assert(found_docs.size() == 2);
        assert(found_docs[0].id == 2);
        assert(found_docs[1].id == 1);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(2, "e f g z"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        const auto found_docs = server.FindTopDocuments("e f"s);
        assert(found_docs.size() == 4);
        assert(found_docs[0].id == 2);
        assert(found_docs[1].id == 0);
        assert(found_docs[2].id == 1);
        assert(found_docs[3].id == 43);
    }
}


void TestRating() {
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
        server.AddDocument(2, "e f g z"s, DocumentStatus::ACTUAL, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e -c"s);
        assert(found_docs.size() == 2);
        assert(found_docs[0].rating == 21);
        assert(found_docs[1].rating == -9);
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
        assert(found_docs == ans1);
        const auto found_docs2 = server.MatchDocument(q2, 43);
        tuple<vector<string>, DocumentStatus> ans2 = { {}, DocumentStatus::IRRELEVANT };
        assert(found_docs2 == ans2);
        const auto found_docs3 = server.MatchDocument(q32, 43);
        tuple<vector<string>, DocumentStatus> ans3 = { {}, DocumentStatus::IRRELEVANT };
        assert(found_docs2==ans3);
    }

    //tuple<vector<string>, DocumentStatus> MatchDocument(const string & raw_query, int document_id)
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
        const auto found_docs = server.FindTopDocuments("e"s, [](const int& id, const DocumentStatus& ds, const int& rating) {return id > 1; });
        assert(found_docs.size() == 2);
        assert(found_docs[0].id == 43);
        assert(found_docs[1].id == 2);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, [](const int& id, const DocumentStatus& ds, const int& rating) {return static_cast<int>(ds) > 1; });
        assert(found_docs.size() == 2);
        assert(found_docs[0].id == 2);
        assert(found_docs[1].id == 1);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, [](const int& id, const DocumentStatus& ds, const int& rating) {return ds == DocumentStatus::ACTUAL; });
        assert(found_docs.size() == 1);
        assert(found_docs[0].id == 43);
    }

    {
        SearchServer server;
        server.SetStopWords("a"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id + 1, "b c d e"s, DocumentStatus::ACTUAL, { 0, 1, 2 });
        server.AddDocument(0, "c d e f n"s, DocumentStatus::IRRELEVANT, { 0, -1, -2 });
        server.AddDocument(1, "d e f g k l m"s, DocumentStatus::BANNED, { 0, -30, 2 });
        server.AddDocument(2, "e f g z x"s, DocumentStatus::REMOVED, { 0, 31, 32 });
        const auto found_docs = server.FindTopDocuments("e"s, [](const int& id, const DocumentStatus& ds, const int& rating) {return rating < -5; });
        assert(found_docs.size() == 1);
        assert(found_docs[0].id == 1);
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
        const auto found_docs = server.FindTopDocuments("e"s, [](const int& id, const DocumentStatus& ds, const int& rating) {return id > 1; });
        assert(found_docs.size() == 2);
        assert(abs(found_docs[0].relevance - 0.055786) < 1e-6);
        assert(abs(found_docs[1].relevance - 0.044629) < 1e-6);
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWords();
    TestMatching();
    TestRelevance();
    TestRating();
    TestPredicateFiltering();
    TestRelevanceComputing();
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}

/*
// ==================== для примера =========================


void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
*/