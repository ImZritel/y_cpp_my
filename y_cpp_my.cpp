#include "document.h"
#include "paginator.h"
#include "request_queue.h"
#include "search_server.h"
#include "string_processing.h"
#include "wrappers.h"
#include "log_duration.h"

#include <iostream>
using namespace std;
int main() {
    SearchServer search_server(""s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::BANNED, { 1, 1, 1 });

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;

    auto results = search_server.FindTopDocuments("cat");
    for (const auto& doc : results) { 
        PrintDocument(doc); 
    }
    {
        LogDuration h("Operation time"s);
        MatchDocuments(search_server, "cat");
    }
    {
        LOG_DURATION_STREAM("Operation time"s, std::cout);
        MatchDocuments(search_server, "cat");
    }
    return 0;
}