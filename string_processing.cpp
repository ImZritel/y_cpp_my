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
