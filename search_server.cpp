#include "search_server.h"

#include <execution>

using namespace std::string_literals;

SearchServer::SearchServer(std::string sws) : SearchServer(std::string_view(sws)) {}
SearchServer::SearchServer(std::string_view swsv) {
    for (const auto word : SplitIntoWords(swsv)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Error: invalid word (string_view constructor)."s);
        }
        stop_words_.insert(std::string(word));
    }
}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0 || documents_.count(document_id) > 0) { throw std::invalid_argument("Error: doc id is negative or duplicate already existing id."s); }
    else {
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status,
                std::string(document)
            });
        std::vector<std::string_view> words = SplitIntoWordsNoStop(std::string_view(documents_.at(document_id).content));
        added_doc_ids_.insert(document_id);
        const double inv_word_count = 1.0 / words.size();
        for (const std::string_view word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
            docid_word_freqs_[document_id][word] += inv_word_count;
        }
    }
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
//par
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::par, raw_query, [&status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view raw_query) const {
    return FindTopDocuments(std::execution::par, raw_query, [](int document_id, DocumentStatus document_status, int rating) { return document_status == DocumentStatus::ACTUAL; });
}
//seq
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

SearchServer::MatchingDocs_sv SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}
SearchServer::MatchingDocs_sv SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    if (!docid_word_freqs_.count(document_id)) {
        throw std::out_of_range("");
    }
    const auto& doc_to_word = docid_word_freqs_.at(document_id);

    for (const auto& word : query.minus_words) {
        if (doc_to_word.count(word) > 0) {
            {
                return { matched_words, documents_.at(document_id).status };
            }
        }
    }
    for (const auto& word : query.plus_words) {
        if (doc_to_word.count(word) > 0) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

/*Returns MatchingDocs_sv (that is words, doc status) that exists in both: the query and doc(id).*/
SearchServer::MatchingDocs_sv SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view raw_query, int document_id) const {
    if (!docid_word_freqs_.count(document_id)) {
        throw std::out_of_range("out_of_range in MatchDocument ");
    }
    const auto& doc_word_freqs = docid_word_freqs_.at(document_id);

    std::vector<std::string_view> splited_query = SplitIntoWords(raw_query);
    if (std::any_of(std::execution::par, splited_query.begin(), splited_query.end(),
        [this, &doc_word_freqs](const auto& word) {const auto parsed_w = ParseQueryWord(word);
    return parsed_w.is_minus
        && doc_word_freqs.count(parsed_w.data) > 0
        ; })) {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> plus_ws(splited_query.size());
    const auto plus_end_it = std::copy_if(std::execution::par, splited_query.begin(), splited_query.end(),
        plus_ws.begin(),
        [this, &doc_word_freqs](const auto& word) {const auto pw = ParseQueryWord(word); return !pw.is_stop //
        && doc_word_freqs.count(word) > 0; });
    std::sort(std::execution::par, plus_ws.begin(), plus_end_it);
    const auto plus_end = std::unique(std::execution::par, plus_ws.begin(), plus_end_it);
    std::vector<std::string_view> res(std::make_move_iterator(plus_ws.begin()), std::make_move_iterator(plus_end));
    return { res, documents_.at(document_id).status };
}


std::set<int>::const_iterator SearchServer::begin() const {
    return added_doc_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return added_doc_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string_view, double> f{};
    if (!docid_word_freqs_.count(document_id)) {
        return f;
    }
    return docid_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy ex, int document_id) {
    if (!docid_word_freqs_.count(document_id)) {
        throw std::invalid_argument("Error: no document with such id (RemoveDocument)."s);
    }
    std::vector<std::string_view> words_v(docid_word_freqs_.at(document_id).size());
    std::transform(std::execution::par,
        docid_word_freqs_.at(document_id).begin(), docid_word_freqs_.at(document_id).end(),
        words_v.begin(),
        [](const auto& p) {return std::get<0>(p); });    // get the right words
    std::for_each(std::execution::par, words_v.begin(), words_v.end(),
        [this, document_id](const auto& word) {word_to_document_freqs_.at(word).erase(document_id); });
    docid_word_freqs_.erase(document_id);
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy ex, int document_id) {
    for (auto& wf : docid_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(wf.first).erase(document_id);
    }
    docid_word_freqs_.erase(document_id);
    added_doc_ids_.erase(find(added_doc_ids_.begin(), added_doc_ids_.end(), document_id));
    documents_.erase(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (docid_word_freqs_.count(document_id)) {
        RemoveDocument(std::execution::seq, document_id);
    }
    else {
        throw std::invalid_argument("Error: no document with such id (RemoveDocument)."s);
    }
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(std::string(word)) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    if (word.size() == 1 && word == "-") { return false; }
    else if (word.size() > 1 && word[0] == word[1] && word[1] == '-') { return false; }
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}
bool SearchServer::IsValidWord(const std::string_view word) {
    if (word.size() == 1 && word == "-") { return false; }
    else if (word.size() > 1 && word[0] == word[1] && word[1] == '-') { return false; }
    return std::none_of(word.begin(), word.end(), [](auto c) {
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
                throw std::invalid_argument("Error: invalid word (SplitIntoWordsNoStop string)."s);
            }
        }

    }
    return words;
}
std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            if (IsValidWord(word)) {
                words.push_back(word);
            }
            else {
                throw std::invalid_argument("Error: invalid word (SplitIntoWordsNoStop string_view)."s);
            }
        }

    }
    return words;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::execution::parallel_policy, const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const auto& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            if (IsValidWord(word)) {
                words.push_back(word);
            }
            else {
                throw std::invalid_argument("Error: invalid word (SplitIntoWordsNoStop string_view)."s);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    QueryWord result;
    bool is_minus = false;

    if (text.size() > 0) {
        if (text[0] == '-') {
            if (text.size() == 1 || text[1] == '-') {
                throw std::invalid_argument("invalid_argument ParseQueryWord");
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
            false
        };
    }
    return result;
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    Query result;
    auto words = SplitIntoWords(text);
    std::for_each(words.begin(), words.end(), [this, &result](const auto& word) {QueryWord query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
        if (IsValidWord(query_word.data)) {
            query_word.is_minus ? result.minus_words.push_back(query_word.data) : result.plus_words.push_back(query_word.data);
        }
        else {
            throw std::invalid_argument("Error: invalid word (ParseQuery)."s);
        }
    }
        });
    std::sort(result.minus_words.begin(), result.minus_words.end());
    std::sort(result.plus_words.begin(), result.plus_words.end());
    const auto mw_end = std::unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.resize(mw_end - result.minus_words.begin());
    const auto pw_end = std::unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.resize(pw_end - result.plus_words.begin());
    return result;
}


// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e) {
        std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query) {
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

void MatchDocuments(const SearchServer& search_server, const std::string_view query) {
    try {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = *(std::next(search_server.begin(), index));
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}