#include "BStarTree.hpp"
#include <cassert>
#include <cctype>
#include <iostream>
#include <random>
#include <string>
#include <stdexcept>
#include <vector>

template <typename T>
static void expect_true(bool cond, const T& msg) {
    if (!cond) {
        std::cerr << "FAILED: " << msg << "\n";
        std::abort();
    }
}

static void test_contains_size_and_clear() {
    BStarTree<int,int> t(5);
    expect_true(t.size() == 0, "size initially zero");
    for (int i=0;i<20;++i) t.insert(i, i*2);
    expect_true(t.size() == 20, "size after inserts");
    for (int i=0;i<20;++i) expect_true(t.contains(i), "contains inserted key");
    t.clear();
    expect_true(t.size() == 0, "size after clear");
    for (int i=0;i<20;++i) expect_true(!t.contains(i), "cleared tree missing key");
}

static void test_erase_missing_returns_false() {
    BStarTree<int,int> t(5);
    t.insert(1, 100);
    bool removed = t.erase(2);
    expect_true(!removed, "erase missing returns false");
    expect_true(t.contains(1), "existing key survives");
    t.validate();
}

static void test_invalid_branching_factor_rejected() {
    bool threw = false;
    try {
        BStarTree<int,int> t(2);
        (void)t;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect_true(threw, "constructor rejects M<3");
}

struct CaseInsensitiveLess {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        auto normalize = [](const std::string& s) {
            std::string out = s;
            for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        };
        return normalize(lhs) < normalize(rhs);
    }
};

static void test_custom_comparator() {
    BStarTree<std::string, int, CaseInsensitiveLess> t(5);
    t.insert("Key", 1);
    t.insert("key", 2); // updates existing logically equal entry
    auto v = t.find("KEY");
    expect_true(v && *v == 2, "custom comparator equality");
    t.validate();
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
    test_contains_size_and_clear();
    test_erase_missing_returns_false();
    test_invalid_branching_factor_rejected();
    test_custom_comparator();
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
