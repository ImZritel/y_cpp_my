#include "string_processing.h"

std::string ReadLine() {
    std::string s;
    std::getline(std::cin, s);
    return s;
}
int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}
std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
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