#include "Compression.hpp"
#include "Errors.hpp"
#include "FileContent.hpp"
#include "TestUtils.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using ByteVec = std::vector<std::uint8_t>;

ByteVec toBytes(const std::string& s) {
    return ByteVec(s.begin(), s.end());
}

void assertRoundtrip(const ByteVec& data) {
    FileContent f;
    f.replaceAll(data);
    compressInplace(f, CompAlgo::LZW);
    assert(isCompressed(f));
    try {
        uncompressInplace(f);
    } catch (...) {
        std::cerr << "roundtrip failed, input bytes=" << data.size() << "\n";
        throw;
    }
    assert(f.bytes() == data);
}

void assertRoundtrip(const std::string& s) {
    assertRoundtrip(toBytes(s));
}

void test_empty_roundtrip() {
    FileContent f;
    compressInplace(f);
    assert(isCompressed(f));
    uncompressInplace(f);
    assert(f.size() == 0);
}

void test_single_symbol() {
    assertRoundtrip(std::string(32, 'A'));
}

void test_classic_phrase() {
    assertRoundtrip("TOBEORNOTTOBEORTOBEORNOT");
}

void test_utf8_text() {
    const std::string text = "Привет, LZW! Всё хорошо?";
    assertRoundtrip(text);
}

void test_random_bytes() {
    ByteVec data(1000);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& b : data)
        b = static_cast<std::uint8_t>(dist(rng));
    assertRoundtrip(data);
}

void test_large_repeating_block() {
    std::string block(4096, 'Z');
    std::string payload;
    payload.reserve(block.size() * 80);
    for (int i = 0; i < 80; ++i)
        payload += block;
    assertRoundtrip(payload);
}

void test_codebit_growth_sequence() {
    ByteVec data;
    data.reserve(180000);
    for (int i = 0; i < 180000; ++i)
        data.push_back(static_cast<std::uint8_t>(i & 0xFF));
    assertRoundtrip(data);
}

void test_kwkwk_pattern() {
    assertRoundtrip("ABAABABAABAABABAABAA");
}

void test_payload_corruption_detection() {
    FileContent f;
    f.assignText("payload corruption guard data");
    compressInplace(f);
    auto corrupted = f.bytes();
    assert(corrupted.size() > 13);
    corrupted[13] ^= 0xFF;
    FileContent broken;
    broken.replaceAll(corrupted);
    expectThrows(ErrorCode::Corrupted, [&]{ uncompressInplace(broken); });
}

void test_length_mismatch_detection() {
    FileContent f;
    const std::string text = "original length mismatch sample";
    f.assignText(text);
    compressInplace(f);
    auto mutated = f.bytes();
    assert(mutated.size() >= 13);
    std::uint64_t wrongLen = static_cast<std::uint64_t>(text.size() + 1);
    for (int i = 0; i < 8; ++i)
        mutated[5 + i] = static_cast<std::uint8_t>((wrongLen >> (8 * i)) & 0xFFu);
    FileContent tampered;
    tampered.replaceAll(mutated);
    expectThrows(ErrorCode::Corrupted, [&]{ uncompressInplace(tampered); });
}

void test_functional_roundtrip_multiple() {
    std::vector<std::string> samples = {
        "short text",
        std::string(1024, 'B'),
        "mixed ASCII and utf8: данные",
    };
    for (auto& s : samples)
        assertRoundtrip(s);
}

} // namespace

struct NamedTest {
    const char* name;
    void (*fn)();
};

int main(int argc, char** argv) {
    std::vector<NamedTest> tests = {
        {"empty", &test_empty_roundtrip},
        {"single_symbol", &test_single_symbol},
        {"classic_phrase", &test_classic_phrase},
        {"utf8", &test_utf8_text},
        {"random_bytes", &test_random_bytes},
        {"large_repeating", &test_large_repeating_block},
        {"codebit_growth", &test_codebit_growth_sequence},
        {"kwkwk", &test_kwkwk_pattern},
        {"payload_corruption", &test_payload_corruption_detection},
        {"length_mismatch", &test_length_mismatch_detection},
        {"functional_multi", &test_functional_roundtrip_multiple}
    };

    auto runNamed = [](const NamedTest& t) {
        try {
            t.fn();
        } catch (...) {
            std::cerr << "Test failed: " << t.name << "\n";
            throw;
        }
    };

    try {
        if (argc > 1) {
            std::string target = argv[1];
            auto it = std::find_if(tests.begin(), tests.end(), [&](const NamedTest& t){ return target == t.name; });
            if (it == tests.end()) {
                std::cerr << "Unknown test: " << target << "\n";
                return 2;
            }
            runNamed(*it);
        } else {
            for (const auto& t : tests)
                runNamed(t);
        }
        std::cout << "Compression tests passed!\n";
    } catch (const VfsException& ex) {
        handleException(ex);
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Standard exception: " << ex.what() << "\n";
        return 2;
    }
    return 0;
}
