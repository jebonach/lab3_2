#pragma once
#include <cstdint>
#include <vector>

class FileContent;

enum class CompAlgo : std::uint8_t {
    LZW_VAR_ALL   = 2,
    LZW_VAR_ALPHA = 3
};

bool isCompressed(const FileContent& f);
void compressInplace(FileContent& f, CompAlgo algo);
void uncompressInplace(FileContent& f);
