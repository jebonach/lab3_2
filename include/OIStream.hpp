#pragma once
#include "FileContent.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class StreamMode {
    ReadOnly,
    WriteOnly,
    ReadWrite
};

class OIStream {
public:
    OIStream(FileContent& file, StreamMode mode, std::size_t bufSize);

    void Open();
    void Close();

    bool ReadByte(std::uint8_t& out);
    std::size_t Read(std::uint8_t* dst, std::size_t n);
    bool ReadChar(char& c);
    std::string ReadLine();

    void WriteByte(std::uint8_t b);
    std::size_t Write(const std::uint8_t* src, std::size_t n);
    void WriteChar(char c);
    void WriteString(const std::string& s);

    void Flush();

    bool CanSeek() const noexcept;
    std::size_t Tell() const;
    std::size_t Seek(std::size_t newPos);

    bool IsOpen() const noexcept;
    bool Eof() const noexcept;

private:
    enum class BufferRole { Idle, Read, Write };

    void fillBufferForRead(std::size_t filePos);
    void flushBufferForWrite();
    void ensureOpen() const;
    void ensureModeRead() const;
    void ensureModeWrite() const;
    bool canRead() const noexcept;
    bool canWrite() const noexcept;
    void prepareForWrite();

    FileContent& file_;
    StreamMode mode_;
    const std::size_t bufCapacity_;
    std::vector<std::uint8_t> buffer_;

    bool opened_{false};
    std::size_t bufFilePos_{0};
    std::size_t bufSizeUsed_{0};
    std::size_t bufPos_{0};
    bool dirty_{false};
    bool eof_{false};
    BufferRole role_{BufferRole::Idle};
};
