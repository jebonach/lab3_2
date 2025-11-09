#include "VirtualOStream.hpp"
#include "Errors.hpp"
#include <cstring>

VirtualOStream::VirtualOStream(FileContent& f, OpenMode m)
: file_(f), pos_(0)
{
    if (m == OpenMode::Truncate) file_.truncate(0);
    else if (m == OpenMode::Append) pos_ = file_.size();
}

void VirtualOStream::seekp(std::size_t p){
    pos_ = p;
}

void VirtualOStream::write(const void* src, std::size_t n){
    const std::uint8_t* p = static_cast<const std::uint8_t*>(src);
    if (n==0) return;
    // ensure capacity & write
    std::vector<std::uint8_t> tmp(p, p+n);
    file_.write(pos_, tmp);
    pos_ += n;
}
