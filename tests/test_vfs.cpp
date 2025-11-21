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
    vfs.compress("a.txt");
    std::string compressed = vfs.readFile("a.txt");
    assert(!compressed.empty());
    vfs.decompress("a.txt");
    std::string result = vfs.readFile("a.txt");
    assert(result == "aaaabbbcccddeeeee");
}

static void test_empty_compression() {
    Vfs vfs;
    vfs.createFile("empty.txt");
    vfs.compress("empty.txt");
    vfs.decompress("empty.txt");
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
    v.compress("/huge.bin");
    v.decompress("/huge.bin");
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

static void test_compress_directory_recursive() {
    Vfs v;
    v.mkdir("/docs");
    v.createFile("/docs/a.txt");
    v.createFile("/docs/b.txt");
    v.mkdir("/docs/reports");
    v.createFile("/docs/reports/q1.txt");
    v.writeFile("/docs/a.txt", "alpha", false);
    v.writeFile("/docs/b.txt", "beta", false);
    v.writeFile("/docs/reports/q1.txt", "inner", false);

    v.compress("/docs");

    auto a = v.resolve("/docs/a.txt");
    assert(a && a->isFile);
    const auto& bytes = a->content.bytes();
    assert(bytes.size() >= 13);
    assert(bytes[0] == 'C' && bytes[1] == 'M' && bytes[2] == 'P');

    v.decompress("/docs");
    assert(v.readFile("/docs/a.txt") == "alpha");
    assert(v.readFile("/docs/b.txt") == "beta");
    assert(v.readFile("/docs/reports/q1.txt") == "inner");
}

static void test_decompress_skips_plain_files() {
    Vfs v;
    v.createFile("/plain.txt");
    v.writeFile("/plain.txt", "sample", false);
    v.decompress("/plain.txt");
    assert(v.readFile("/plain.txt") == "sample");
}

static void test_compress_decompress_errors() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ v.compress("/missing"); });
    expectThrows(ErrorCode::PathError, [&]{ v.decompress("/missing"); });
}

static void test_file_properties_tracking() {
    Vfs v;
    v.createFile("/stats.txt");
    auto node = v.resolve("/stats.txt");
    assert(node && node->isFile);
    auto created = node->fileProps.createdAt;
    assert(node->fileProps.byteSize == 0);
    assert(node->fileProps.charCount == 0);

    v.writeFile("/stats.txt", "hello", false);
    node = v.resolve("/stats.txt");
    assert(node->fileProps.charCount == 5);
    assert(node->fileProps.byteSize == 5);
    assert(node->fileProps.modifiedAt >= created);
    auto modified = node->fileProps.modifiedAt;

    v.compress("/stats.txt");
    node = v.resolve("/stats.txt");
    assert(node->fileProps.byteSize == node->content.size());
    assert(node->fileProps.modifiedAt >= modified);

    v.decompress("/stats.txt");
    node = v.resolve("/stats.txt");
    assert(node->fileProps.charCount == 5);
    assert(node->fileProps.byteSize == 5);
    assert(node->fileProps.createdAt == created);
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
        test_compress_directory_recursive();
        test_decompress_skips_plain_files();
        test_compress_decompress_errors();
        test_file_properties_tracking();
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
