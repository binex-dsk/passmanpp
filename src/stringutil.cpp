#include "stringutil.h"
#include <iostream>
#include <cstring>

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string trim(const std::string& line) {
    const char* whiteSpace = " \t\v\r\n";
    std::size_t start = line.find_first_not_of(whiteSpace);
    std::size_t end = line.find_last_not_of(whiteSpace);
    return start == end ? std::string() : line.substr(start, end - start + 1);
}

std::string trimNull(const std::string& line) {
    std::size_t start = line.find_first_not_of("\u0000");
    std::size_t end = line.find_last_not_of("\u0000");
    return start == end ? std::string() : line.substr(start, end - start + 1);

}

std::vector<std::string> split(std::string text, char delim) {
    std::string tmp;
    std::vector<std::string> stk;
    std::stringstream ss(text);
    while(std::getline(ss, tmp, delim)) {
        stk.push_back(tmp);
    }
    return stk;
}

std::string atos(int asciiVal) {
    return std::string(reinterpret_cast<char*>(&asciiVal));
}
