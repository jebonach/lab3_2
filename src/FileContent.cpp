#include "FileContent.hpp"
#include <cstring>

void FileContent::write(std::size_t off, const std::vector<byte>& buf) {
    throwIf(off > data_.size(), ErrorCode::OutOfRange);
    if (off + buf.size() > data_.size()) {
        data_.resize(off + buf.size());
    }
    std::memcpy(data_.data() + off, buf.data(), buf.size());
}

void FileContent::append(const std::vector<byte>& buf) {
    data_.insert(data_.end(), buf.begin(), buf.end());
}

std::vector<FileContent::byte> FileContent::read(std::size_t off, std::size_t n) const {
    throwIf(off > data_.size(), ErrorCode::OutOfRange);
    std::size_t len = std::min(n, data_.size() - off);
    return {data_.begin() + off, data_.begin() + off + len};
}

void FileContent::truncate(std::size_t newSize) {
    data_.resize(newSize);
}

void FileContent::replaceAll(const std::vector<byte>& buf) {
    data_ = buf;
}
