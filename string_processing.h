#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <execution>

int ReadLineWithNumber();
std::vector<std::string> SplitIntoWords(const std::string& text);
std::vector<std::string_view> SplitIntoWords(const std::string_view text);
