#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>

static void test_basic_file_rename() {
    Vfs v;
    v.mkdir("/a");
    v.createFile("/a/f.txt");
    v.renameNode("/a/f.txt", "g.txt");

    auto fOld = v.findFileByName("f.txt");
    auto fNew = v.findFileByName("g.txt");
    assert(!fOld);
    assert(fNew && fNew->name == "g.txt");
}

static void test_directory_rename() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/a/dir");
    v.renameNode("/a/dir", "renamed");
    auto dir = v.resolve("/a/renamed");
    assert(dir && !dir->isFile);
}

static void test_rename_errors() {
    Vfs v;
    v.mkdir("/a");
    v.createFile("/a/f");
    v.createFile("/a/g");

    expectThrows(ErrorCode::InvalidArg, [&]{ v.renameNode("/a/g", "f"); });
    expectThrows(ErrorCode::InvalidArg, [&]{ v.renameNode("/a/g", ""); });
    expectThrows(ErrorCode::InvalidArg, [&]{ v.renameNode("/a/g", ".."); });
    expectThrows(ErrorCode::RootError,   [&]{ v.renameNode("/", "root"); });
}

static void test_move_file_between_directories() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/b");
    v.createFile("/a/file");
    v.mv("/a/file", "/b");
    auto f = v.findFileByName("file");
    assert(f && f->parent.lock()->name == "b");
}

static void test_move_directory_into_descendant_prohibited() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/a/b");
    expectThrows(ErrorCode::Conflict, [&]{ v.mv("/a", "/a/b"); });
}

static void test_move_error_cases() {
    Vfs v;
    v.mkdir("/a");
    v.createFile("/a/file");
    v.createFile("/x");
    expectThrows(ErrorCode::PathError, [&]{ v.mv("/a/file", "/nope"); });
    expectThrows(ErrorCode::InvalidArg, [&]{ v.mv("/a", "/x"); });
}

static void test_move_with_relative_paths() {
    Vfs v;
    v.mkdir("/home");
    v.mkdir("/home/user");
    v.createFile("/home/user/log.txt");
    v.cd("/home");
    v.mv("user/log.txt", "/");
    auto file = v.findFileByName("log.txt");
    assert(file && file->parent.lock()->name == "/");
}

int main() {
    test_basic_file_rename();
    test_directory_rename();
    test_rename_errors();
    test_move_file_between_directories();
    test_move_directory_into_descendant_prohibited();
    test_move_error_cases();
    test_move_with_relative_paths();
    std::cout << "[OK] test_rename_mv\n";
}
