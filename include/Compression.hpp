#pragma once
#include "FileContent.hpp"

enum class CompAlgo : std::uint8_t { RLE1 = 1 };

bool isCompressed(const FileContent& f);
void compressInplace(FileContent& f, CompAlgo algo = CompAlgo::RLE1);
void uncompressInplace(FileContent& f);
