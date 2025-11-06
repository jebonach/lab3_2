#include "FileContent.hpp"
#include <cstring>

void FileContent::write(std::size_t off, const std::vector<byte>& buf){
    if (off > data_.size()) throwErr(Errc::OutOfRange, "FileContent::write", "off > size");
    if (off + buf.size() > data_.size()) data_.resize(off + buf.size());
    std::memcpy(data_.data() + off, buf.data(), buf.size());
}

void FileContent::append(const std::vector<byte>& buf){
    auto old = data_.size();
    data_.resize(old + buf.size());
    std::memcpy(data_.data() + old, buf.data(), buf.size());
}

std::vector<FileContent::byte> FileContent::read(std::size_t off, std::size_t n) const{
    if (off > data_.size()) throwErr(Errc::OutOfRange, "FileContent::read", "off > size");
    std::size_t can = std::min(n, data_.size() - off);
    return std::vector<byte>(data_.begin()+off, data_.begin()+off+can);
}

void FileContent::truncate(std::size_t newSize){
    data_.resize(newSize);
}

void FileContent::replaceAll(const std::vector<byte>& buf){
    data_ = buf;
}
