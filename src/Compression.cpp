#include "Compression.hpp"
#include "Errors.hpp"
#include <cstring>
#include <string>
#include <unordered_map>

static void put64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        out.push_back((v >> (8 * i)) & 0xFF);
}

static std::uint64_t get64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (std::uint64_t)p[i] << (8 * i);
    return v;
}

static constexpr std::size_t kLzwDictLimit = 4096;

static std::vector<std::uint16_t> lzw_encode(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint16_t> codes;
    if (input.empty()) return codes;

    std::unordered_map<std::string, std::uint16_t> dict;
    dict.reserve(kLzwDictLimit * 2);
    for (int i = 0; i < 256; ++i) {
        dict.emplace(std::string(1, static_cast<char>(i)), static_cast<std::uint16_t>(i));
    }

    std::string w(1, static_cast<char>(input[0]));
    std::uint16_t nextCode = 256;

    for (std::size_t i = 1; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
        std::string wc = w;
        wc.push_back(c);
        auto it = dict.find(wc);
        if (it != dict.end()) {
            w = std::move(wc);
        } else {
            codes.push_back(dict.at(w));
            if (nextCode < kLzwDictLimit) {
                dict.emplace(std::move(wc), nextCode++);
            }
            w.assign(1, c);
        }
    }
    codes.push_back(dict.at(w));
    return codes;
}

static std::vector<std::uint8_t> lzw_pack(const std::vector<std::uint16_t>& codes) {
    std::vector<std::uint8_t> out;
    if (codes.empty()) return out;

    std::uint32_t bitbuf = 0;
    int bits = 0;
    for (auto code : codes) {
        bitbuf |= static_cast<std::uint32_t>(code & 0x0FFFu) << bits;
        bits += 12;
        while (bits >= 8) {
            out.push_back(static_cast<std::uint8_t>(bitbuf & 0xFFu));
            bitbuf >>= 8;
            bits -= 8;
        }
    }
    if (bits > 0) {
        out.push_back(static_cast<std::uint8_t>(bitbuf & 0xFFu));
    }
    return out;
}

static std::vector<std::uint16_t> lzw_unpack(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint16_t> codes;
    if (data.empty()) return codes;

    std::uint32_t bitbuf = 0;
    int bits = 0;
    for (auto byte : data) {
        bitbuf |= static_cast<std::uint32_t>(byte) << bits;
        bits += 8;
        while (bits >= 12) {
            codes.push_back(static_cast<std::uint16_t>(bitbuf & 0x0FFFu));
            bitbuf >>= 12;
            bits -= 12;
        }
    }
    return codes;
}

static std::vector<std::uint8_t> lzw_decode(const std::vector<std::uint16_t>& codes) {
    if (codes.empty()) return {};

    std::vector<std::vector<std::uint8_t>> dict(kLzwDictLimit);
    for (int i = 0; i < 256; ++i) {
        dict[i] = {static_cast<std::uint8_t>(i)};
    }

    std::uint16_t nextCode = 256;
    auto prevCode = codes.front();
    if (prevCode >= kLzwDictLimit)
        throw VfsException(ErrorCode::Corrupted);
    auto prevEntry = dict[prevCode];
    std::vector<std::uint8_t> output = prevEntry;

    for (std::size_t i = 1; i < codes.size(); ++i) {
        auto code = codes[i];
        std::vector<std::uint8_t> entry;
        if (code < nextCode && !dict[code].empty()) {
            entry = dict[code];
        } else if (code == nextCode && !prevEntry.empty()) {
            entry = prevEntry;
            entry.push_back(prevEntry.front());
        } else {
            throw VfsException(ErrorCode::Corrupted);
        }

        output.insert(output.end(), entry.begin(), entry.end());

        if (nextCode < kLzwDictLimit && !prevEntry.empty()) {
            auto newEntry = prevEntry;
            newEntry.push_back(entry.front());
            dict[nextCode++] = std::move(newEntry);
        }

        prevEntry = std::move(entry);
    }

    return output;
}

static std::vector<std::uint8_t> lzw_compress_data(const std::vector<std::uint8_t>& input) {
    auto codes = lzw_encode(input);
    return lzw_pack(codes);
}

static std::vector<std::uint8_t> lzw_decompress_data(const std::vector<std::uint8_t>& input) {
    auto codes = lzw_unpack(input);
    return lzw_decode(codes);
}

bool isCompressed(const FileContent& f) {
    const auto& b = f.bytes();
    return b.size() >= 13 && b[0] == 'C' && b[1] == 'M' && b[2] == 'P' && b[3] == 1;
}

void compressInplace(FileContent& f, CompAlgo algo) {
    if (isCompressed(f)) return;

    const auto& raw = f.bytes();

    std::vector<std::uint8_t> hdr;
    hdr.push_back('C');
    hdr.push_back('M');
    hdr.push_back('P');
    hdr.push_back(1);
    hdr.push_back(static_cast<std::uint8_t>(algo));
    put64(hdr, static_cast<std::uint64_t>(raw.size()));

    std::vector<std::uint8_t> payload;
    switch (algo) {
        case CompAlgo::LZW:
            payload = lzw_compress_data(raw);
            break;
        default:
            throw VfsException(ErrorCode::Unsupported);
    }

    std::vector<std::uint8_t> out;
    out.reserve(hdr.size() + payload.size());
    out.insert(out.end(), hdr.begin(), hdr.end());
    out.insert(out.end(), payload.begin(), payload.end());
    f.replaceAll(out);
}

void uncompressInplace(FileContent& f) {
    if (!isCompressed(f))
        throw VfsException(ErrorCode::InvalidArg);

    const auto& b = f.bytes();
    if (b.size() < 13)
        throw VfsException(ErrorCode::Corrupted);

    auto algo = static_cast<CompAlgo>(b[4]);
    std::uint64_t origSize = get64(&b[5]);

    std::vector<std::uint8_t> payload(b.begin() + 13, b.end());
    std::vector<std::uint8_t> raw;

    switch (algo) {
        case CompAlgo::LZW:
            raw = lzw_decompress_data(payload);
            break;
        default:
            throw VfsException(ErrorCode::Unsupported);
    }

    if (raw.size() != origSize)
        throw VfsException(ErrorCode::Corrupted);

    f.replaceAll(raw);
}
