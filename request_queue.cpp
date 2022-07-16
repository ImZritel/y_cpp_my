#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_(search_server) {
    total_request_count_ = 0;
    null_result_count_ = 0;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    LogRequest(result.empty());
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query);
    LogRequest(result.empty());
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    return null_result_count_;
}

void RequestQueue::LogRequest(bool is_null) {
    ++total_request_count_;
    if (is_null) { ++null_result_count_; };
    requests_.push_back({ total_request_count_, is_null });
    if (requests_.size() > min_in_day_) {
        if (requests_.front().null_result) { --null_result_count_; };
        requests_.pop_front();
    }
}