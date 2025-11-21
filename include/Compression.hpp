#pragma once
#include "FileContent.hpp"

enum class CompAlgo : std::uint8_t { LZW = 2 };

bool isCompressed(const FileContent& f);
void compressInplace(FileContent& f, CompAlgo algo = CompAlgo::LZW);
void uncompressInplace(FileContent& f);
