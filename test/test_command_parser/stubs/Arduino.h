/**
 * Minimal Arduino.h stub for native unit testing
 * Provides String class and other basic Arduino types
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cctype>

// Arduino type aliases
using boolean = bool;
using byte = uint8_t;

// Minimal String class compatible with Arduino String
class String {
   public:
    String() : _str("") {}
    String(const char* s) : _str(s ? s : "") {}
    String(const String& other) : _str(other._str) {}
    String(char c) : _str(1, c) {}
    String(int value) : _str(std::to_string(value)) {}
    String(unsigned int value) : _str(std::to_string(value)) {}
    String(long value) : _str(std::to_string(value)) {}
    String(unsigned long value) : _str(std::to_string(value)) {}

    String& operator=(const String& other) {
        _str = other._str;
        return *this;
    }

    String& operator=(const char* s) {
        _str = s ? s : "";
        return *this;
    }

    bool operator==(const String& other) const { return _str == other._str; }
    bool operator==(const char* s) const { return _str == s; }
    bool operator!=(const String& other) const { return _str != other._str; }
    bool operator!=(const char* s) const { return _str != s; }

    String operator+(const String& other) const {
        String result;
        result._str = _str + other._str;
        return result;
    }

    String& operator+=(const String& other) {
        _str += other._str;
        return *this;
    }

    String& operator+=(const char* s) {
        if (s) _str += s;
        return *this;
    }

    String& operator+=(char c) {
        _str += c;
        return *this;
    }

    char operator[](unsigned int index) const {
        return _str[index];
    }

    const char* c_str() const { return _str.c_str(); }
    unsigned int length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }

    void concat(const String& s) { _str += s._str; }
    void concat(const char* s) { if (s) _str += s; }
    void concat(char c) { _str += c; }

    String substring(unsigned int beginIndex) const {
        return String(_str.substr(beginIndex).c_str());
    }

    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        return String(_str.substr(beginIndex, endIndex - beginIndex).c_str());
    }

    int indexOf(char c) const {
        auto pos = _str.find(c);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    int indexOf(const String& s) const {
        auto pos = _str.find(s._str);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    int indexOf(char c, unsigned int fromIndex) const {
        auto pos = _str.find(c, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    bool startsWith(const String& prefix) const {
        return _str.compare(0, prefix._str.length(), prefix._str) == 0;
    }

    bool startsWith(const char* prefix) const {
        return _str.compare(0, strlen(prefix), prefix) == 0;
    }

    void toLowerCase() {
        std::transform(_str.begin(), _str.end(), _str.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    }

    void toUpperCase() {
        std::transform(_str.begin(), _str.end(), _str.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    }

    void trim() {
        // Trim leading whitespace
        size_t start = _str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            _str.clear();
            return;
        }
        // Trim trailing whitespace
        size_t end = _str.find_last_not_of(" \t\n\r");
        _str = _str.substr(start, end - start + 1);
    }

    long toInt() const {
        try {
            return std::stol(_str);
        } catch (...) {
            return 0;
        }
    }

    float toFloat() const {
        try {
            return std::stof(_str);
        } catch (...) {
            return 0.0f;
        }
    }

    void reserve(unsigned int size) {
        _str.reserve(size);
    }

   private:
    std::string _str;
};

// Minimal millis() stub
inline unsigned long millis() {
    static unsigned long counter = 0;
    return counter++;
}
