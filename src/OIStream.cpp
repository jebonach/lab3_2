#include "OIStream.hpp"
#include "Errors.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

OIStream::OIStream(FileContent& file, StreamMode mode, std::size_t bufSize)
    : file_(file), mode_(mode), bufCapacity_(bufSize) {
    if (bufSize == 0) throw VfsException(ErrorCode::InvalidArg);
}

void OIStream::Open() {
    if (opened_) throw VfsException(ErrorCode::InvalidArg);
    buffer_.assign(bufCapacity_, 0);
    bufFilePos_ = 0;
    bufSizeUsed_ = 0;
    bufPos_ = 0;
    dirty_ = false;
    eof_ = false;
    role_ = BufferRole::Idle;
    opened_ = true;

    if (canRead()) {
        fillBufferForRead(0);
    }
}

void OIStream::Close() {
    if (!opened_) return;
    if (canWrite() && dirty_) {
        flushBufferForWrite();
    }
    opened_ = false;
    bufSizeUsed_ = bufPos_ = 0;
    dirty_ = false;
    eof_ = false;
    role_ = BufferRole::Idle;
}

bool OIStream::ReadByte(std::uint8_t& out) {
    return Read(&out, 1) == 1;
}

std::size_t OIStream::Read(std::uint8_t* dst, std::size_t n) {
    ensureOpen();
    ensureModeRead();
    if (n == 0) return 0;
    if (dst == nullptr) throw VfsException(ErrorCode::InvalidArg);

    std::size_t total = 0;
    while (total < n) {
        if (bufPos_ >= bufSizeUsed_) {
            std::size_t nextFilePos = bufFilePos_ + bufSizeUsed_;
            fillBufferForRead(nextFilePos);
            if (bufSizeUsed_ == 0) break;
        }
        std::size_t available = bufSizeUsed_ - bufPos_;
        std::size_t chunk = std::min(available, n - total);
        std::memcpy(dst + total, buffer_.data() + bufPos_, chunk);
        bufPos_ += chunk;
        total += chunk;
    }
    if (total == 0 && bufSizeUsed_ == 0) eof_ = true;
    return total;
}

bool OIStream::ReadChar(char& c) {
    std::uint8_t byte = 0;
    if (!ReadByte(byte)) return false;
    c = static_cast<char>(byte);
    return true;
}

std::string OIStream::ReadLine() {
    ensureOpen();
    ensureModeRead();
    std::string line;
    char ch;
    while (ReadChar(ch)) {
        if (ch == '\n') break;
        line.push_back(ch);
    }
    return line;
}

void OIStream::WriteByte(std::uint8_t b) {
    Write(&b, 1);
}

std::size_t OIStream::Write(const std::uint8_t* src, std::size_t n) {
    ensureOpen();
    ensureModeWrite();
    if (n == 0) return 0;
    if (src == nullptr) throw VfsException(ErrorCode::InvalidArg);

    prepareForWrite();

    std::size_t total = 0;
    while (total < n) {
        if (bufPos_ >= bufCapacity_) {
            flushBufferForWrite();
        }
        std::size_t space = bufCapacity_ - bufPos_;
        std::size_t chunk = std::min(space, n - total);
        std::memcpy(buffer_.data() + bufPos_, src + total, chunk);
        bufPos_ += chunk;
        bufSizeUsed_ = std::max(bufSizeUsed_, bufPos_);
        dirty_ = true;
        total += chunk;
        if (bufPos_ == bufCapacity_) {
            flushBufferForWrite();
        }
    }
    return total;
}

void OIStream::WriteChar(char c) {
    WriteByte(static_cast<std::uint8_t>(c));
}

void OIStream::WriteString(const std::string& s) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(s.data());
    Write(data, s.size());
}

void OIStream::Flush() {
    ensureOpen();
    if (!canWrite()) return;
    flushBufferForWrite();
}

bool OIStream::CanSeek() const noexcept {
    return true;
}

std::size_t OIStream::Tell() const {
    ensureOpen();
    return bufFilePos_ + bufPos_;
}

std::size_t OIStream::Seek(std::size_t newPos) {
    ensureOpen();
    if (!CanSeek()) throw VfsException(ErrorCode::IOError);

    if (canWrite() && dirty_) {
        flushBufferForWrite();
    }

    if (canRead() && role_ == BufferRole::Read) {
        std::size_t start = bufFilePos_;
        std::size_t end = bufFilePos_ + bufSizeUsed_;
        if (newPos >= start && newPos <= end) {
            bufPos_ = newPos - start;
            eof_ = false;
            return Tell();
        }
    }

    bufFilePos_ = newPos;
    bufPos_ = 0;
    bufSizeUsed_ = 0;
    eof_ = false;
    role_ = BufferRole::Idle;

    if (canRead()) {
        fillBufferForRead(newPos);
        return Tell();
    }

    if (canWrite()) role_ = BufferRole::Write;
    return Tell();
}

bool OIStream::IsOpen() const noexcept {
    return opened_;
}

bool OIStream::Eof() const noexcept {
    return eof_;
}

void OIStream::fillBufferForRead(std::size_t filePos) {
    if (!canRead()) return;
    std::size_t fileSize = file_.size();
    bufFilePos_ = filePos;
    if (filePos >= fileSize) {
        bufSizeUsed_ = 0;
        bufPos_ = 0;
        eof_ = true;
        role_ = BufferRole::Read;
        return;
    }
    std::size_t toRead = std::min(bufCapacity_, fileSize - filePos);
    auto data = file_.read(filePos, toRead);
    std::copy(data.begin(), data.end(), buffer_.begin());
    bufSizeUsed_ = data.size();
    bufPos_ = 0;
    eof_ = false;
    role_ = BufferRole::Read;
}

void OIStream::flushBufferForWrite() {
    if (!dirty_) return;
    std::vector<std::uint8_t> chunk(buffer_.begin(), buffer_.begin() + bufSizeUsed_);
    file_.write(bufFilePos_, chunk);
    bufFilePos_ += bufSizeUsed_;
    bufPos_ = 0;
    bufSizeUsed_ = 0;
    dirty_ = false;
    role_ = BufferRole::Write;
}

void OIStream::ensureOpen() const {
    if (!opened_) throw VfsException(ErrorCode::InvalidArg);
}

void OIStream::ensureModeRead() const {
    if (!canRead()) throw VfsException(ErrorCode::InvalidArg);
}

void OIStream::ensureModeWrite() const {
    if (!canWrite()) throw VfsException(ErrorCode::InvalidArg);
}

bool OIStream::canRead() const noexcept {
    return mode_ == StreamMode::ReadOnly || mode_ == StreamMode::ReadWrite;
}

bool OIStream::canWrite() const noexcept {
    return mode_ == StreamMode::WriteOnly || mode_ == StreamMode::ReadWrite;
}

void OIStream::prepareForWrite() {
    if (role_ == BufferRole::Write) return;
    std::size_t absolutePos = bufFilePos_ + bufPos_;
    bufFilePos_ = absolutePos;
    bufPos_ = 0;
    bufSizeUsed_ = 0;
    dirty_ = false;
    role_ = BufferRole::Write;
}
