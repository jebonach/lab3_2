#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_find_file_by_name_basic() {
    Vfs v;
    v.createFile("/f1.txt");
    v.mkdir("/dir");

    auto files = v.findNodesByName("f1.txt");
    assert(files.size() == 1 && files[0]->isFile);

    auto dir = v.findNodesByName("dir");
    assert(dir.size() == 1 && !dir[0]->isFile);
}

static void test_find_updates_after_rename_and_move() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/b");
    v.createFile("/a/file.txt");

    auto file = v.findNodesByName("file.txt");
    assert(file.size() == 1);

    v.renameNode("/a/file.txt", "renamed.txt");
    assert(v.findNodesByName("file.txt").empty());
    auto renamed = v.findNodesByName("renamed.txt");
    assert(renamed.size() == 1);

    v.mv("/a/renamed.txt", "/b");
    auto moved = v.findNodesByName("renamed.txt");
    assert(moved.size() == 1);
    assert(moved[0]->parent.lock()->name == "b");
}

static void test_find_clears_after_rm() {
    Vfs v;
    v.createFile("/temp.log");
    assert(v.findNodesByName("temp.log").size() == 1);
    v.rm("/temp.log");
    assert(v.findNodesByName("temp.log").empty());
}

static void test_find_returns_all_matches() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/b");
    v.createFile("/a/shared.txt");
    v.createFile("/b/shared.txt");

    auto matches = v.findNodesByName("shared.txt");
    assert(matches.size() == 2);
    bool hasA = false, hasB = false;
    for (const auto& node : matches) {
        auto parent = node->parent.lock();
        if (!parent) continue;
        if (parent->name == "a") hasA = true;
        if (parent->name == "b") hasB = true;
    }
    assert(hasA && hasB);
}

static void test_save_json_creates_file() {
    Vfs v;
    v.mkdir("/data");
    v.createFile("/data/a.txt");
    v.createFile("/data/b.txt");

    v.saveJson("/snapshot.json");

    auto file = v.resolve("/snapshot.json");
    assert(file && file->isFile);
    auto contents = file->content.asText();
    assert(!contents.empty());
    assert(contents.find("a.txt") != std::string::npos);
    assert(contents.find("b.txt") != std::string::npos);
}

static void test_save_json_failure() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ v.saveJson("/nope/dir/state.json"); });
    v.mkdir("/dir");
    v.saveJson("/dir");
    auto dirFile = v.resolve("/dir", Vfs::ResolveKind::File);
    assert(dirFile && dirFile->isFile);
    v.mkdir("/existing");
    v.createFile("/existing/file.txt");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.saveJson("/existing/file.txt/sub.json"); });
}

int main() {
    test_find_file_by_name_basic();
    test_find_updates_after_rename_and_move();
    test_find_clears_after_rm();
    test_find_returns_all_matches();
    test_save_json_creates_file();
    test_save_json_failure();
    std::cout << "[OK] test_find_save\n";
}
