#include <algorithm>

#include "string_processing.h"
#include "read_input_functions.h"

int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<std::string> SplitIntoWords(const std::string text) {
    std::vector<std::string> words;
    std::string word;
    std::for_each(text.begin(), text.end(), [&word, &words](const char& c) {
        if (c == ' ' && !word.empty()) {
            words.push_back(word);
            word = "";
        }
        else if (c != ' ') {
            word += c;
        }
        });
    words.push_back(word);

    return words;
}

std::vector<std::string_view> SplitIntoWords(const std::string_view text) {
    std::vector<std::string_view> res;
    size_t start_pos = 0;
    size_t stop_pos = 0;
    size_t length = 0;
    while (start_pos < text.size()) {
        start_pos = text.find_first_not_of(std::string_view(" "), start_pos);
        stop_pos = text.find_first_of(std::string_view(" "), start_pos);
        stop_pos == std::string_view::npos ? length = text.size() - start_pos : length = stop_pos - start_pos;
        res.push_back(text.substr(start_pos, length));
        start_pos = stop_pos;
    }
    return res;
}