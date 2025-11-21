#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_basic_creation() {
    Vfs v;
    v.mkdir("/dir");
    v.createFile("/dir/file.txt");

    auto matches = v.findNodesByName("file.txt");
    assert(matches.size() == 1);
    auto f = matches[0];
    assert(f->isFile);
    assert(f->parent.lock()->name == "dir");
    assert(v.resolve("/dir/file.txt") == f);
}

static void test_relative_creation_and_pwd() {
    Vfs v;
    v.mkdir("docs");
    v.cd("docs");
    v.mkdir("reports");
    v.cd("reports");
    v.createFile("q1.txt");
    v.cd("..");
    v.mkdir("logs");

    assert(v.pwd() == "/docs");
    auto q1 = v.resolve("/docs/reports/q1.txt");
    assert(q1 && q1->isFile);
    auto logs = v.resolve("/docs/logs");
    assert(logs && !logs->isFile);
}

static void test_invalid_names_are_rejected() {
    Vfs v;
    expectThrows(ErrorCode::InvalidArg, [&]{ v.mkdir(""); });
    expectThrows(ErrorCode::PathError, [&]{ v.mkdir("/bad/name"); });
    expectThrows(ErrorCode::PathError, [&]{ v.createFile("/bad/.."); });
    v.mkdir("/alpha");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.createFile("/alpha/.."); });
}

static void test_missing_parent_errors() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ v.mkdir("/unknown/child"); });
    expectThrows(ErrorCode::PathError, [&]{ v.createFile("/missing/file.txt"); });

    v.mkdir("/root");
    v.createFile("/root/file.txt");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.mkdir("/root/file.txt/child"); });
}

static void test_bulk_creation_keeps_index_consistent() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/a/b");
    v.mkdir("/a/b/c");
    v.createFile("/a/b/c/file1.txt");
    v.createFile("/a/b/c/file2.txt");

    auto f1 = v.findNodesByName("file1.txt");
    auto f2 = v.findNodesByName("file2.txt");
    assert(f1.size() == 1 && f2.size() == 1);

    v.rm("/a/b/c/file1.txt");
    auto missingNode = v.resolve("/a/b/c/file1.txt");
    assert(!missingNode);
    assert(v.findNodesByName("file1.txt").empty());
}

static void test_duplicate_names_generate_suffixes() {
    Vfs v;
    v.createFile("/note.txt");
    v.createFile("/note.txt");
    auto original = v.resolve("/note.txt", Vfs::ResolveKind::File);
    auto duplicate = v.resolve("/note(1).txt", Vfs::ResolveKind::File);
    assert(original && duplicate && original != duplicate);

    v.mkdir("/dir");
    v.mkdir("/dir");
    auto dirOriginal = v.resolve("/dir", Vfs::ResolveKind::Directory);
    auto dirDuplicate = v.resolve("/dir(1)", Vfs::ResolveKind::Directory);
    assert(dirOriginal && dirDuplicate && dirOriginal != dirDuplicate);

    v.createFile("/shared");
    v.mkdir("/shared");
    auto sharedFile = v.resolve("/shared", Vfs::ResolveKind::File);
    auto sharedDir = v.resolve("/shared/", Vfs::ResolveKind::Directory);
    assert(sharedFile && sharedDir);
}

int main() {
    test_basic_creation();
    test_relative_creation_and_pwd();
    test_invalid_names_are_rejected();
    test_missing_parent_errors();
    test_bulk_creation_keeps_index_consistent();
    test_duplicate_names_generate_suffixes();
    std::cout << "[OK] test_create\n";
}
