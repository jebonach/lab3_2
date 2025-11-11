#include "Compression.hpp"
#include "Errors.hpp"
#include <cstring>

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

static std::vector<std::uint8_t> rle_compress(const std::vector<std::uint8_t>& in) {
    std::vector<std::uint8_t> out;
    if (in.empty()) return out;
    std::uint8_t cur = in[0], cnt = 1;
    for (std::size_t i = 1; i < in.size(); ++i) {
        if (in[i] == cur && cnt < 255) {
            ++cnt;
        } else {
            out.push_back(cnt);
            out.push_back(cur);
            cur = in[i];
            cnt = 1;
        }
    }
    out.push_back(cnt);
    out.push_back(cur);
    return out;
}

static std::vector<std::uint8_t> rle_decompress(const std::vector<std::uint8_t>& in) {
    std::vector<std::uint8_t> out;
    if (in.size() % 2 != 0)
        throw VfsException(ErrorCode::Corrupted);

    for (std::size_t i = 0; i < in.size(); i += 2) {
        std::uint8_t cnt = in[i];
        std::uint8_t b = in[i + 1];
        out.insert(out.end(), cnt, b);
    }
    return out;
}

bool isCompressed(const FileContent& f) {
    const auto& b = f.bytes();
    return b.size() >= 6 && b[0] == 'C' && b[1] == 'M' && b[2] == 'P' && b[3] == 1;
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
        case CompAlgo::RLE1:
            payload = rle_compress(raw);
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
    if (b.size() < 14)
        throw VfsException(ErrorCode::Corrupted);

    auto algo = static_cast<CompAlgo>(b[4]);
    std::uint64_t origSize = get64(&b[5]);

    std::vector<std::uint8_t> payload(b.begin() + 13, b.end());
    std::vector<std::uint8_t> raw;

    switch (algo) {
        case CompAlgo::RLE1:
            raw = rle_decompress(payload);
            break;
        default:
            throw VfsException(ErrorCode::Unsupported);
    }

    if (raw.size() != origSize)
        throw VfsException(ErrorCode::Corrupted);

    f.replaceAll(raw);
}
