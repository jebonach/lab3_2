#pragma once

#include "Errors.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

template <typename Fn>
void expectThrows(ErrorCode expected, Fn&& fn) {
    bool caught = false;
    try {
        fn();
        assert(!"expected VfsException");
    } catch (const VfsException& ex) {
        caught = true;
        assert(ex.code == expected);
    } catch (...) {
        assert(!"unexpected exception type");
    }
    assert(caught && "expected VfsException");
}

class CoutCapture {
public:
    CoutCapture() : oldBuf_(std::cout.rdbuf(buffer_.rdbuf())) {}
    ~CoutCapture() { std::cout.rdbuf(oldBuf_); }

    std::string str() const { return buffer_.str(); }

private:
    std::ostringstream buffer_;
    std::streambuf* oldBuf_;
};
