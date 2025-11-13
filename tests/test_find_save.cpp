#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iterator>
#include <string>

static std::filesystem::path makeTempJsonPath() {
    auto dir = std::filesystem::temp_directory_path();
    auto stamp = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto path = dir / ("vfs_state_" + std::to_string(stamp) + ".json");
    int attempts = 0;
    while (std::filesystem::exists(path) && attempts < 5) {
        ++attempts;
        path = dir / ("vfs_state_" + std::to_string(stamp + attempts) + ".json");
    }
    return path;
}

static void test_find_file_by_name_basic() {
    Vfs v;
    v.createFile("/f1.txt");
    v.mkdir("/dir");

    auto file = v.findFileByName("f1.txt");
    assert(file && file->isFile);

    auto dir = v.findFileByName("dir");
    assert(!dir); // directories are not indexed
}

static void test_find_updates_after_rename_and_move() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/b");
    v.createFile("/a/file.txt");

    auto file = v.findFileByName("file.txt");
    assert(file);

    v.renameNode("/a/file.txt", "renamed.txt");
    assert(!v.findFileByName("file.txt"));
    auto renamed = v.findFileByName("renamed.txt");
    assert(renamed);

    v.mv("/a/renamed.txt", "/b");
    auto moved = v.findFileByName("renamed.txt");
    assert(moved);
    assert(moved->parent.lock()->name == "b");
}

static void test_find_clears_after_rm() {
    Vfs v;
    v.createFile("/temp.log");
    assert(v.findFileByName("temp.log"));
    v.rm("/temp.log");
    assert(!v.findFileByName("temp.log"));
}

static void test_save_json_creates_file() {
    Vfs v;
    v.mkdir("/data");
    v.createFile("/data/a.txt");
    v.createFile("/data/b.txt");

    auto path = makeTempJsonPath();
    v.saveJson(path.string());

    std::ifstream in(path);
    std::string contents((std::istreambuf_iterator<char>(in)), {});
    assert(!contents.empty());
    assert(contents.find("a.txt") != std::string::npos);
    assert(contents.find("b.txt") != std::string::npos);
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

static void test_save_json_failure() {
    Vfs v;
    expectThrows(ErrorCode::IOError, [&]{ v.saveJson("/nope/dir/state.json"); });
}

int main() {
    test_find_file_by_name_basic();
    test_find_updates_after_rename_and_move();
    test_find_clears_after_rm();
    test_save_json_creates_file();
    test_save_json_failure();
    std::cout << "[OK] test_find_save\n";
}
