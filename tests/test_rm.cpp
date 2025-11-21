#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>

static void test_remove_file_twice() {
    Vfs v;
    v.mkdir("/a");
    v.createFile("/a/f");
    v.rm("/a/f");
    expectThrows(ErrorCode::PathError, [&]{ v.rm("/a/f"); });
}

static void test_recursive_directory_removal() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/a/b");
    v.createFile("/a/b/c.txt");
    v.createFile("/a/b/d.txt");

    v.rm("/a/b");

    expectThrows(ErrorCode::PathError, [&]{ v.cd("/a/b"); });
    assert(v.findNodesByName("c.txt").empty());
    assert(v.findNodesByName("d.txt").empty());
}

static void test_remove_root_is_forbidden() {
    Vfs v;
    expectThrows(ErrorCode::RootError, [&]{ v.rm("/"); });
}

static void test_remove_nonexistent_path() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ v.rm("/missing"); });
}

static void test_remove_using_relative_paths() {
    Vfs v;
    v.mkdir("/dir");
    v.cd("/dir");
    v.createFile("note.txt");
    v.rm("note.txt");
    expectThrows(ErrorCode::PathError, [&]{ v.rm("note.txt"); });
}

int main() {
    test_remove_file_twice();
    test_recursive_directory_removal();
    test_remove_root_is_forbidden();
    test_remove_nonexistent_path();
    test_remove_using_relative_paths();
    std::cout << "[OK] test_rm\n";
}
