#pragma once

#include "document.h"
#include "search_server.h"
#include <vector>
#include <string>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
        LogRequest(result.empty());
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        int query_id = -1;
        bool null_result = true;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int total_request_count_;
    int null_result_count_;
    const SearchServer& search_server_;
    void LogRequest(bool is_null);
};