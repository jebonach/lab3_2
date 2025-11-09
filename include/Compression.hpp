#pragma once
#include "FileContent.hpp"

// very simple lossless format:
// header: 4 bytes 'C','M','P',1, 1 byte algo, 8 bytes orig_size (LE)
// payload: depends on algo
enum class CompAlgo : std::uint8_t { RLE1 = 1 };

bool isCompressed(const FileContent& f);
void compressInplace(FileContent& f, CompAlgo algo = CompAlgo::RLE1);
void uncompressInplace(FileContent& f);
