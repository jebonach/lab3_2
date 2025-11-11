#include "Vfs.hpp"
#include "Errors.hpp"
#include <cassert>
#include <iostream>

void test_compression_roundtrip() {
    Vfs vfs;
    vfs.createFile("a.txt");
    vfs.writeFile("a.txt", "aaaabbbcccddeeeee", false);
    vfs.compressFile("a.txt");
    std::string compressed = vfs.readFile("a.txt");
    assert(compressed.size() < 18);
    vfs.decompressFile("a.txt");
    std::string result = vfs.readFile("a.txt");
    assert(result == "aaaabbbcccddeeeee");
}

void test_empty_compression() {
    Vfs vfs;
    vfs.createFile("empty.txt");
    vfs.compressFile("empty.txt");
    vfs.decompressFile("empty.txt");
    assert(vfs.readFile("empty.txt") == "");
}

void test_append_and_read_offset() {
    Vfs vfs;
    vfs.createFile("data.txt");
    vfs.writeFile("data.txt", "Hello", false);
    vfs.writeFile("data.txt", ", World!", true);
    std::string full = vfs.readFile("data.txt");
    assert(full == "Hello, World!");
}

void test_truncate_and_overwrite() {
    FileContent f;
    f.assignText("This is a test");
    f.truncate(4);
    assert(f.asText() == "This");
    f.assignText("Data");
    assert(f.asText() == "Data");
}

void test_binary_read_write() {
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

int main() {
    try {
        test_compression_roundtrip();
        test_empty_compression();
        test_append_and_read_offset();
        test_truncate_and_overwrite();
        test_binary_read_write();
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
