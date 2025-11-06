#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "FileContent.hpp"

enum class OpenMode { Truncate, Append };

class VirtualOStream {
public:
    VirtualOStream(FileContent& f, OpenMode m);

    std::size_t tellp() const noexcept { return pos_; }
    void seekp(std::size_t p);

    void write(const void* src, std::size_t n);
    void write(const std::vector<std::uint8_t>& buf){ write(buf.data(), buf.size()); }

    template<class T>
    void writeValue(const T& v){ write(&v, sizeof(T)); }

    void flush() noexcept {}

private:
    FileContent& file_;
    std::size_t  pos_;
};
