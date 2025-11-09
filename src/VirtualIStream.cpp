#include "VirtualIStream.hpp"
#include "Errors.hpp"
#include <cstring>

std::vector<std::uint8_t> VirtualIStream::read(std::size_t n){
    if (pos_ > file_.size()) throwErr(Errc::OutOfRange, "VirtualIStream::read", "pos > size");
    auto chunk = file_.read(pos_, n);
    pos_ += chunk.size();
    return chunk;
}
