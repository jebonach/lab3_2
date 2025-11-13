#include "Vfs.hpp"
#include "Errors.hpp"
#include "FileContent.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static void test_compression_roundtrip() {
    Vfs vfs;
    vfs.createFile("a.txt");
    vfs.writeFile("a.txt", "aaaabbbcccddeeeee", false);
    vfs.compressFile("a.txt");
    std::string compressed = vfs.readFile("a.txt");
    assert(!compressed.empty());
    vfs.decompressFile("a.txt");
    std::string result = vfs.readFile("a.txt");
    assert(result == "aaaabbbcccddeeeee");
}

static void test_empty_compression() {
    Vfs vfs;
    vfs.createFile("empty.txt");
    vfs.compressFile("empty.txt");
    vfs.decompressFile("empty.txt");
    assert(vfs.readFile("empty.txt") == "");
}

static void test_append_and_read_offset() {
    Vfs vfs;
    vfs.createFile("data.txt");
    vfs.writeFile("data.txt", "Hello", false);
    vfs.writeFile("data.txt", ", World!", true);
    std::string full = vfs.readFile("data.txt");
    assert(full == "Hello, World!");
}

static void test_truncate_and_overwrite() {
    FileContent f;
    f.assignText("This is a test");
    f.truncate(4);
    assert(f.asText() == "This");
    f.assignText("Data");
    assert(f.asText() == "Data");
}

static void test_binary_read_write() {
    FileContent f;
    int x = 123456789;
    f.writeValue<int>(0, x);
    int y = f.readValue<int>(0);
    assert(x == y);

    f.write(4, {0xDE, 0xAD, 0xBE, 0xEF});
    auto bytes = f.read(4, 4);
    assert(bytes[0] == 0xDE);
    assert(bytes[3] == 0xEF);
}

static void test_filecontent_bounds_checks() {
    FileContent f;
    expectThrows(ErrorCode::OutOfRange, [&]{ f.read(1, 1); });
    f.assignText("abc");
    expectThrows(ErrorCode::OutOfRange, [&]{ f.write(5, {0x00}); });
}

static void test_write_modes() {
    Vfs v;
    v.createFile("/notes.txt");
    v.writeFile("/notes.txt", "one", false);
    assert(v.readFile("/notes.txt") == "one");
    v.writeFile("/notes.txt", "two", true);
    assert(v.readFile("/notes.txt") == "onetwo");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.writeFile("/", "data", false); });
}

static void test_read_errors() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ (void)v.readFile("/missing.txt"); });
    v.mkdir("/dir");
    expectThrows(ErrorCode::InvalidArg, [&]{ (void)v.readFile("/dir"); });
}

static void test_compress_long_runs() {
    Vfs v;
    v.createFile("/huge.bin");
    std::string payload(600, 'Z');
    v.writeFile("/huge.bin", payload, false);
    v.compressFile("/huge.bin");
    v.decompressFile("/huge.bin");
    assert(v.readFile("/huge.bin") == payload);
}

static void test_resolve_relative_paths() {
    Vfs v;
    v.mkdir("/dir");
    v.cd("/dir");
    v.createFile("file.txt");
    auto node = v.resolve("./file.txt");
    assert(node && node->isFile);
    auto missing = v.resolve("../missing");
    assert(!missing);
}

static void test_compress_decompress_errors() {
    Vfs v;
    v.mkdir("/dir");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.compressFile("/dir"); });
    expectThrows(ErrorCode::InvalidArg, [&]{ v.decompressFile("/dir"); });
    expectThrows(ErrorCode::PathError, [&]{ v.compressFile("/missing"); });
    expectThrows(ErrorCode::PathError, [&]{ v.decompressFile("/missing"); });
}

int main() {
    try {
        test_compression_roundtrip();
        test_empty_compression();
        test_append_and_read_offset();
        test_truncate_and_overwrite();
        test_binary_read_write();
        test_filecontent_bounds_checks();
        test_write_modes();
        test_read_errors();
        test_compress_long_runs();
        test_resolve_relative_paths();
        test_compress_decompress_errors();
        std::cout << "All tests passed!\n";
    } catch (const VfsException& ex) {
        handleException(ex);
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Standard exception: " << ex.what() << "\n";
        return 2;
    }
    return 0;
}
