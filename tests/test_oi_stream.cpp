#include "OIStream.hpp"
#include "FileContent.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

static void test_read_empty_stream() {
    FileContent file;
    OIStream stream(file, StreamMode::ReadOnly, 4);
    stream.Open();
    std::uint8_t byte = 0;
    assert(!stream.ReadByte(byte));
    assert(stream.Eof());
    stream.Close();
}

static void test_buffered_reading() {
    FileContent file;
    file.assignText("HelloBufferedWorld");
    OIStream stream(file, StreamMode::ReadOnly, 5);
    stream.Open();
    std::vector<std::uint8_t> out(file.size());
    auto count = stream.Read(out.data(), out.size());
    assert(count == file.size());
    std::string reconstructed(out.begin(), out.end());
    assert(reconstructed == "HelloBufferedWorld");
    stream.Close();
}

static void test_read_line_and_seek() {
    FileContent file;
    file.assignText("line1\nline2\n");
    OIStream stream(file, StreamMode::ReadOnly, 4);
    stream.Open();
    auto first = stream.ReadLine();
    assert(first == "line1");
    assert(stream.Tell() == 6);
    auto second = stream.ReadLine();
    assert(second == "line2");
    stream.Seek(0);
    auto again = stream.ReadLine();
    assert(again == "line1");
    stream.Close();
}

static void test_write_and_flush() {
    FileContent file;
    OIStream stream(file, StreamMode::WriteOnly, 4);
    stream.Open();
    stream.WriteString("abc");
    stream.Flush();
    assert(file.asText() == "abc");
    stream.Close();
}

static void test_write_multiple_buffers() {
    FileContent file;
    std::string payload(25, 'Z');
    OIStream stream(file, StreamMode::WriteOnly, 4);
    stream.Open();
    stream.WriteString(payload);
    stream.Close();
    assert(file.asText() == payload);
}

static void test_seek_and_overwrite() {
    FileContent file;
    OIStream stream(file, StreamMode::WriteOnly, 3);
    stream.Open();
    stream.WriteString("AAAAA");
    stream.Flush();
    stream.Seek(2);
    stream.WriteString("BB");
    stream.Close();
    assert(file.asText() == "AABBA");
}

static void test_seek_beyond_file_reports_eof() {
    FileContent file;
    file.assignText("xyz");
    OIStream stream(file, StreamMode::ReadOnly, 2);
    stream.Open();
    auto pos = stream.Seek(10);
    assert(pos == 10);
    std::uint8_t value = 0;
    assert(!stream.ReadByte(value));
    assert(stream.Eof());
    stream.Close();
}

int main() {
    try {
        test_read_empty_stream();
        test_buffered_reading();
        test_read_line_and_seek();
        test_write_and_flush();
        test_write_multiple_buffers();
        test_seek_and_overwrite();
        test_seek_beyond_file_reports_eof();
        std::cout << "OIStream tests passed\n";
    } catch (const VfsException& ex) {
        handleException(ex);
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Standard exception: " << ex.what() << "\n";
        return 2;
    }
    return 0;
}
