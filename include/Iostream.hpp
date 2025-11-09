#pragma once
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>

class OStream {
    std::ostringstream buffer_;

public:
    void write(const std::string& s) {
        buffer_ << s;
    }
    void writeLine(const std::string& s) {
        buffer_ << s << '\n';
    }
    void writeBytes(const std::vector<uint8_t>& bytes) {
        for (uint8_t b : bytes) buffer_ << static_cast<char>(b);
    }
    std::string str() const {
        return buffer_.str();
    }
    void clear() {
        buffer_.str("");
        buffer_.clear();
    }
};

class IStream {
    std::istringstream buffer_;

public:
    void load(const std::string& s) {
        buffer_.str(s);
        buffer_.clear();
    }
    std::string readLine() {
        std::string s;
        std::getline(buffer_, s);
        return s;
    }
    std::vector<uint8_t> readAllBytes() {
        std::vector<uint8_t> bytes;
        char c;
        while (buffer_.get(c)) bytes.push_back(static_cast<uint8_t>(c));
        return bytes;
    }
    bool eof() const {
        return buffer_.eof();
    }
};
