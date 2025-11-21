#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>

static void test_copy_file_into_directory() {
    Vfs v;
    v.createFile("/note.txt");
    v.writeFile("/note.txt", "hello", false);
    v.mkdir("/dest");

    v.cp("/note.txt", "/dest");
    auto copy = v.resolve("/dest/note.txt");
    assert(copy && copy->isFile);
    assert(v.readFile("/dest/note.txt") == "hello");
    assert(v.readFile("/note.txt") == "hello");
}

static void test_copy_directory_new_location() {
    Vfs v;
    v.mkdir("/src");
    v.mkdir("/src/docs");
    v.createFile("/src/docs/info.txt");
    v.writeFile("/src/docs/info.txt", "payload", false);

    v.cp("/src", "/copy");
    auto dir = v.resolve("/copy/docs");
    assert(dir && !dir->isFile);
    assert(v.readFile("/copy/docs/info.txt") == "payload");
}

static void test_copy_name_conflict() {
    Vfs v;
    v.mkdir("/docs");
    v.createFile("/docs/report.txt");
    v.writeFile("/docs/report.txt", "data", false);

    v.cp("/docs/report.txt", "/docs/report.txt");
    auto dup = v.resolve("/docs/report(1).txt");
    assert(dup && dup->isFile);
    assert(v.readFile("/docs/report(1).txt") == "data");
}

static void test_copy_error_cases() {
    Vfs v;
    v.mkdir("/loop");
    v.mkdir("/loop/sub");
    v.createFile("/file.txt");

    expectThrows(ErrorCode::PathError, [&]{ v.cp("/missing", "/dst"); });
    expectThrows(ErrorCode::RootError,  [&]{ v.cp("/", "/clone"); });
    expectThrows(ErrorCode::Conflict,   [&]{ v.cp("/loop", "/loop/sub"); });
    expectThrows(ErrorCode::InvalidArg, [&]{ v.cp("/file.txt", "/file.txt/data"); });
}

int main() {
    test_copy_file_into_directory();
    test_copy_directory_new_location();
    test_copy_name_conflict();
    test_copy_error_cases();
    std::cout << "[OK] test_cp\n";
}
