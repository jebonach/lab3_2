#include "Vfs.hpp"
#include "Errors.hpp"
#include "TestUtils.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_initial_pwd() {
    Vfs v;
    assert(v.pwd() == "/");
}

static void test_absolute_and_relative_navigation() {
    Vfs v;
    v.mkdir("/a");
    v.mkdir("/a/b");
    v.mkdir("/a/b/c");

    v.cd("/a");
    assert(v.pwd() == "/a");
    v.cd("b");
    v.cd("c");
    assert(v.pwd() == "/a/b/c");

    v.cd("..");
    assert(v.pwd() == "/a/b");
    v.cd("/");
    assert(v.pwd() == "/");
}

static void test_dot_and_dotdot_segments() {
    Vfs v;
    v.mkdir("/dir");
    v.cd("/dir");
    v.mkdir("inner");

    v.cd(".");
    assert(v.pwd() == "/dir");
    v.cd("./inner");
    assert(v.pwd() == "/dir/inner");
    v.cd("../inner");
    assert(v.pwd() == "/dir/inner");
    v.cd("..");
    v.cd(".."); // stay at root
    assert(v.pwd() == "/");
}

static void test_cd_errors() {
    Vfs v;
    expectThrows(ErrorCode::PathError, [&]{ v.cd("/nope"); });

    v.createFile("/file.txt");
    expectThrows(ErrorCode::InvalidArg, [&]{ v.cd("/file.txt"); });
}

static void test_ls_and_print_tree_outputs() {
    Vfs v;
    v.mkdir("/a");
    v.createFile("/a/file.txt");
    v.mkdir("/a/sub");
    v.cd("/a");

    {
        CoutCapture cap;
        v.ls();
        auto out = cap.str();
        assert(out.find("file.txt") != std::string::npos);
        assert(out.find("sub/") != std::string::npos);
    }

    {
        CoutCapture cap;
        v.printTree();
        auto out = cap.str();
        assert(out.find("a/") != std::string::npos);
        assert(out.find("file.txt") != std::string::npos);
    }
}

int main() {
    test_initial_pwd();
    test_absolute_and_relative_navigation();
    test_dot_and_dotdot_segments();
    test_cd_errors();
    test_ls_and_print_tree_outputs();
    std::cout << "[OK] test_nav\n";
}
