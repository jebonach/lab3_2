#include "Compression.hpp"
#include "Errors.hpp"
#include "FileContent.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <cctype>

namespace {

// ======= Общие утилиты контейнера =======

void put64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}
std::uint64_t get64(const std::uint8_t* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (std::uint64_t)p[i] << (8 * i);
    return v;
}

// ======= Писатель/читатель битов с выравниванием при смене разрядности =======

class BitWriter {
public:
    BitWriter() : bitbuf_(0), bitCount_(0) {}

    void put(std::uint32_t value, int nbits) {
        if (nbits <= 0) return;
        std::uint32_t mask = (nbits == 32 ? 0xFFFFFFFFu : ((1u << nbits) - 1u));
        value &= mask;
        bitbuf_ |= (value << bitCount_);
        bitCount_ += nbits;
        while (bitCount_ >= 8) {
            out_.push_back(static_cast<std::uint8_t>(bitbuf_ & 0xFFu));
            bitbuf_ >>= 8;
            bitCount_ -= 8;
        }
    }

    void alignToByte() {
        if (bitCount_ > 0) {
            out_.push_back(static_cast<std::uint8_t>(bitbuf_ & 0xFFu));
            bitbuf_ = 0;
            bitCount_ = 0;
        }
    }

    std::vector<std::uint8_t> finish() {
        alignToByte();
        return std::move(out_);
    }

private:
    std::uint32_t bitbuf_;
    int bitCount_;
    std::vector<std::uint8_t> out_;
};

class BitReader {
public:
    explicit BitReader(const std::vector<std::uint8_t>& data)
        : data_(data), pos_(0), bitbuf_(0), bitCount_(0) {}

    std::optional<std::uint32_t> get(int nbits) {
        if (nbits <= 0) return std::optional<std::uint32_t>(0);
        while (bitCount_ < nbits) {
            if (pos_ >= data_.size()) return std::nullopt;
            bitbuf_ |= (std::uint32_t)data_[pos_++] << bitCount_;
            bitCount_ += 8;
        }
        std::uint32_t mask = (nbits == 32 ? 0xFFFFFFFFu : ((1u << nbits) - 1u));
        std::uint32_t value = bitbuf_ & mask;
        bitbuf_ >>= nbits;
        bitCount_ -= nbits;
        return value;
    }

    void alignToByte() {
        bitbuf_ = 0;
        bitCount_ = 0;
    }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t pos_;
    std::uint32_t bitbuf_;
    int bitCount_;
};

// ======= Общие константы LZW (переменная разрядность) =======

int LZW_MIN_BITS() { return 9; }
int LZW_MAX_BITS() { return 16; }
std::uint32_t LZW_FIRST_FREE() { return 256u; }
std::uint32_t LZW_DICT_LIMIT() { return 1u << LZW_MAX_BITS(); }

// ======= Вспомогательная проверка «буквенности» ASCII-байта =======

bool is_ascii_letter(std::uint8_t b) {
    return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z');
}

// ======= LZW: универсальная кодировка (ALL/ALPHA управляется политикой добавления) =======

std::vector<std::uint8_t> lzw_varwidth_compress_generic(
        const std::vector<std::uint8_t>& input,
        bool alpha_only_dictionary)
{
    BitWriter writer;
    if (input.empty()) return writer.finish();

    // Словарь: строка байтов -> код
    std::unordered_map<std::string, std::uint32_t> dict;
    dict.reserve(1 << 15);
    for (int i = 0; i < 256; ++i) {
        std::string s(1, static_cast<char>(i));
        dict.emplace(std::move(s), (std::uint32_t)i);
    }

    std::string w;
    w.push_back(static_cast<char>(input[0]));

    std::uint32_t nextCode = LZW_FIRST_FREE();
    int codeBits = LZW_MIN_BITS();

    for (std::size_t i = 1; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
        std::string wc = w;
        wc.push_back(c);

        auto it = dict.find(wc);
        if (it != dict.end()) {
            // Фраза расширяется
            w = std::move(wc);
            continue;
        }

        // Выводим код текущей фразы
        std::uint32_t code = dict.at(w);
        writer.put(code, codeBits);

// Политика добавления в словарь:
        // ALL: добавляем всегда, пока есть место;
        // ALPHA: добавляем ТОЛЬКО если и w, и c состоят из буквенных ASCII.
        bool can_add = (nextCode < LZW_DICT_LIMIT());
        if (can_add) {
            bool allow = true;
            if (alpha_only_dictionary) {
                // проверим, что все байты в wc — «буквы»
                for (char ch : wc) {
                    if (!is_ascii_letter(static_cast<std::uint8_t>(ch))) {
                        allow = false;
                        break;
                    }
                }
            }
            if (allow) {
                // перед добавлением проверяем, не требуется ли увеличить разрядность
                if (nextCode == (1u << codeBits) && codeBits < LZW_MAX_BITS()) {
                    ++codeBits;
                    writer.alignToByte();
                }
                dict.emplace(std::move(wc), nextCode++);
            }
        }

        // Если ALPHA и текущий символ «не буква», разорвать фразу,
        // т.е. w = текущий символ; иначе обычное правило
        w.assign(1, c);
    }

    // финальный код
    std::uint32_t finalCode = dict.at(w);
    writer.put(finalCode, codeBits);

    return writer.finish();
}

std::vector<std::uint8_t> lzw_varwidth_decompress_generic(
        const std::vector<std::uint8_t>& payload,
        bool alpha_only_dictionary)
{
    if (payload.empty()) return {};

    BitReader reader(payload);

    // Первый код всегда читается на мин.разрядности
    auto firstCodeOpt = reader.get(LZW_MIN_BITS());
    if (!firstCodeOpt) throw VfsException(ErrorCode::Corrupted);
    std::uint32_t first = *firstCodeOpt;
    if (first >= 256u) throw VfsException(ErrorCode::Corrupted);

    // Словарь: код -> строка байтов
    std::vector<std::string> dict;
    dict.reserve(LZW_DICT_LIMIT());
    for (int i = 0; i < 256; ++i) dict.emplace_back(1, static_cast<char>(i));

    std::string prev = dict[first];
    std::vector<std::uint8_t> out(prev.begin(), prev.end());

    std::uint32_t nextCode = LZW_FIRST_FREE();
    int codeBits = LZW_MIN_BITS();

    while (true) {
        auto codeOpt = reader.get(codeBits);
        if (!codeOpt) break;
        std::uint32_t code = *codeOpt;

        std::string entry;
        if (code < dict.size()) {
            entry = dict[code];
        } else if (code == nextCode && !prev.empty()) {
            // «Квирк» LZW: K = prev + first(prev)
            entry = prev;
            entry.push_back(prev.front());
        } else {
            throw VfsException(ErrorCode::Corrupted);
        }

        out.insert(out.end(), entry.begin(), entry.end());

        // Политика добавления в словарь в декодере должна зеркалить энкодер
        bool can_add = (nextCode < LZW_DICT_LIMIT());
        if (can_add) {
            bool allow = true;
            if (alpha_only_dictionary) {
                // добавляем prev + first(entry) только если все байты «буквы»
                std::string newEntry = prev;
                if (!entry.empty()) newEntry.push_back(entry.front());
                for (char ch : newEntry) {
                    if (!is_ascii_letter(static_cast<std::uint8_t>(ch))) {
                        allow = false;
                        break;
                    }
                }
                if (allow) {
                    if (nextCode == (1u << codeBits) && codeBits < LZW_MAX_BITS()) {
                        ++codeBits;
                        reader.alignToByte();
                    }
                    dict.push_back(std::move(newEntry));
                    ++nextCode;
                }
            } else {
                // ALL
                if (!prev.empty() && !entry.empty()) {
                    std::string newEntry = prev;
                    newEntry.push_back(entry.front());

if (nextCode == (1u << codeBits) && codeBits < LZW_MAX_BITS()) {
                        ++codeBits;
                        reader.alignToByte();
                    }
                    dict.push_back(std::move(newEntry));
                    ++nextCode;
                }
            }
        }

        prev = std::move(entry);
    }

    return out;
}

// Конкретные «обёртки» двух режимов

std::vector<std::uint8_t> lzw_var_all_compress(const std::vector<std::uint8_t>& input) {
    return lzw_varwidth_compress_generic(input, /*alpha_only_dictionary=*/false);
}
std::vector<std::uint8_t> lzw_var_alpha_compress(const std::vector<std::uint8_t>& input) {
    return lzw_varwidth_compress_generic(input, /*alpha_only_dictionary=*/true);
}
std::vector<std::uint8_t> lzw_var_all_decompress(const std::vector<std::uint8_t>& payload) {
    return lzw_varwidth_decompress_generic(payload, /*alpha_only_dictionary=*/false);
}
std::vector<std::uint8_t> lzw_var_alpha_decompress(const std::vector<std::uint8_t>& payload) {
    return lzw_varwidth_decompress_generic(payload, /*alpha_only_dictionary=*/true);
}

} // namespace


bool isCompressed(const FileContent& f) {
    const std::vector<std::uint8_t>& b = f.bytes();
    if (b.size() < 13) return false;
    if (b[0] != 'C' || b[1] != 'M' || b[2] != 'P') return false;
    if (b[3] != 3) return false;
    std::uint8_t algo = b[4];
    return algo == static_cast<std::uint8_t>(CompAlgo::LZW_VAR_ALL) ||
           algo == static_cast<std::uint8_t>(CompAlgo::LZW_VAR_ALPHA);
}

void compressInplace(FileContent& f, CompAlgo algo) {
    if (isCompressed(f)) return;

    const std::vector<std::uint8_t>& raw = f.bytes();

    std::vector<std::uint8_t> hdr;
    hdr.push_back('C'); hdr.push_back('M'); hdr.push_back('P');
    hdr.push_back(3);
    hdr.push_back(static_cast<std::uint8_t>(algo));
    put64(hdr, static_cast<std::uint64_t>(raw.size()));

    std::vector<std::uint8_t> payload;
    switch (algo) {
        case CompAlgo::LZW_VAR_ALL:
            payload = lzw_var_all_compress(raw);
            break;
        case CompAlgo::LZW_VAR_ALPHA:
            payload = lzw_var_alpha_compress(raw);
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
    const std::vector<std::uint8_t>& b = f.bytes();
    if (b.size() < 13) throw VfsException(ErrorCode::InvalidArg);
    if (b[0] != 'C' || b[1] != 'M' || b[2] != 'P') throw VfsException(ErrorCode::InvalidArg);
    if (b[3] != 3) throw VfsException(ErrorCode::Unsupported);

    CompAlgo algo = static_cast<CompAlgo>(b[4]);
    std::uint64_t origSize = get64(&b[5]);

    std::vector<std::uint8_t> payload(b.begin() + 13, b.end());
    std::vector<std::uint8_t> raw;

    switch (algo) {
        case CompAlgo::LZW_VAR_ALL:
            raw = lzw_var_all_decompress(payload);
            break;
        case CompAlgo::LZW_VAR_ALPHA:
            raw = lzw_var_alpha_decompress(payload);
            break;
        default:
            throw VfsException(ErrorCode::Unsupported);
    }

    if (raw.size() != origSize) throw VfsException(ErrorCode::Corrupted);
    f.replaceAll(raw);
}