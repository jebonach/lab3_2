#include "BStarTree.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

template <typename T>
static void expect_true(bool cond, const T& msg) {
    if (!cond) {
        std::cerr << "FAILED: " << msg << "\n";
        std::abort();
    }
}

static void test_basic_sorted() {
    BStarTree<int,int> t(7);
    for (int i=0;i<100;i++) t.insert(i, i*10);
    for (int i=0;i<100;i++) {
        auto v = t.find(i);
        expect_true(v.has_value() && *v == i*10, "basic_sorted find "+std::to_string(i));
    }
    t.validate();
}

static void test_basic_reverse() {
    BStarTree<int,int> t(7);
    for (int i=99;i>=0;i--) t.insert(i, i+1);
    for (int i=0;i<100;i++) {
        auto v = t.find(i);
        expect_true(v && *v == i+1, "basic_reverse find");
    }
    t.validate();
}

static void test_update_duplicates() {
    BStarTree<std::string, int> t(5);
    t.insert("a", 1);
    t.insert("a", 2);
    t.insert("a", 3);
    auto v = t.find("a");
    expect_true(v && *v == 3, "update duplicates");
    t.validate();
}

static void test_erase_simple() {
    BStarTree<int,int> t(5);
    for (int i=0;i<30;i++) t.insert(i, i);
    for (int i=0;i<30;i+=2) {
        bool ok = t.erase(i);
        expect_true(ok, "erase existing");
        auto v = t.find(i);
        expect_true(!v.has_value(), "erased should be missing");
    }
    for (int i=1;i<30;i+=2) {
        auto v = t.find(i);
        expect_true(v && *v == i, "survivor must remain");
    }
    t.validate();
}

static void test_erase_all() {
    BStarTree<int,int> t(7);
    for (int i=0;i<200;i++) t.insert(i, i);
    for (int i=0;i<200;i++) {
        bool ok = t.erase(i);
        expect_true(ok, "erase all step");
    }
    for (int i=0;i<200;i++) {
        expect_true(!t.contains(i), "all erased");
    }
    t.validate();
}

static void test_triple_pressure_smallM() {
    BStarTree<int,int> t(3);
    for (int i=0;i<200;i++) t.insert(i, i);
    t.validate();
    for (int i=0;i<200;i+=3) {
        bool ok = t.erase(i);
        expect_true(ok, "erase under triple pressure");
    }
    t.validate();
}

static void test_randomized() {
    BStarTree<int,int> t(6);
    std::mt19937 rng(123456);
    std::uniform_int_distribution<int> dist(0, 999);

    std::vector<bool> exists(1000,false);

    for (int step = 0; step < 5000; ++step) {
        int x = dist(rng);
        if (rng()%2) {
            t.insert(x, x*7);
            exists[x] = true;
        } else {
            bool ok = t.erase(x);
            if (ok) exists[x] = false;
        }
        if (step % 200 == 0) {
            for (int k=0;k<1000;k+=111) {
                auto v = t.find(k);
                if (exists[k]) expect_true(v && *v == k*7, "rand find present");
                else           expect_true(!v.has_value(), "rand find absent");
            }
            t.validate();
        }
    }
    t.validate();
}

int main() {
    test_basic_sorted();
    test_basic_reverse();
    test_update_duplicates();
    test_erase_simple();
    test_erase_all();
    test_triple_pressure_smallM();
    test_randomized();
    std::cout << "OK: all B*-tree tests passed\n";
    return 0;
}
