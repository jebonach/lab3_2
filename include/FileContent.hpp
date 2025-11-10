#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include "Errors.hpp"

class FileContent {
public:
    using byte = std::uint8_t;

    std::size_t size() const noexcept { return data_.size(); }
    const std::vector<byte>& bytes() const noexcept { return data_; }

    // overwrite [off, off+buf.size)
    void write(std::size_t off, const std::vector<byte>& buf);
    void append(const std::vector<byte>& buf);
    std::vector<byte> read(std::size_t off, std::size_t n) const;

    // POD helpers
    template<class T>
    void writeValue(std::size_t off, const T& v) {
        const byte* p = reinterpret_cast<const byte*>(&v);
        write(off, std::vector<byte>(p, p + sizeof(T)));
    }

    template<class T>
    T readValue(std::size_t off) const {
        auto b = read(off, sizeof(T));
        if (b.size() != sizeof(T))
            throwErr(Errc::OutOfRange, "FileContent::readValue", "not enough bytes");
        T v{};
        std::memcpy(&v, b.data(), sizeof(T));
        return v;
    }

    void truncate(std::size_t newSize);
    void replaceAll(const std::vector<byte>& buf);

    // Text sugar
    void assignText(const std::string& s) { replaceAll(std::vector<byte>(s.begin(), s.end())); }
    std::string asText() const { return std::string(data_.begin(), data_.end()); }

private:
    std::vector<byte> data_;
};
