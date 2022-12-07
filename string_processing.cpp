#include <algorithm>

#include "string_processing.h"
#include "read_input_functions.h"

int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}
std::vector<std::string> SplitIntoWords(const std::string& text) {
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

/* Parallel version of SplitIntoWords. */
std::vector<std::string> SplitIntoWords(std::execution::parallel_policy ex, const std::string& text) {
    /*
    std::vector<char*> ends(text.length());
    const auto ends_end = std::transform(std::execution::par, text.begin(), text.end() - 1,
        text.begin() + 1, 
        ends.begin(),
        [](const auto c1, const auto c2) {return c1 != ' ' && c2 == ' ' ? &c2 : nullptr; });
    ends.erase(ends_end, ends.end());
    std::sort(std::execution::par, ends.begin(), ends.end());
    const auto end = std::unique(std::execution::par, ends.begin(), ends.end());*/
    

    std::vector<std::string> words;
    std::string word = "";
    std::for_each(std::execution::par, text.begin(), text.end(), [&word, &words](const char& c) {
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