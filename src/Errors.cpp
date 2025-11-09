#include "Errors.hpp"

const std::unordered_map<Errc, ErrorInfo> g_errorTable = {
    {Errc::NotFound,         {"Not found",                     "Проверьте путь/имя"} },
    {Errc::NotFile,          {"Is not a file",                 "Ожидался файл"} },
    {Errc::NotDir,           {"Is not a directory",            "Ожидалась директория"} },
    {Errc::AlreadyExists,    {"Already exists",                "Используйте другое имя"} },
    {Errc::InvalidArg,       {"Invalid argument",              "Проверьте параметры"} },
    {Errc::IOError,          {"I/O error",                     "Повторите операцию"} },
    {Errc::OutOfRange,       {"Offset/length out of range",    "Сместите позицию"} },
    {Errc::Unsupported,      {"Unsupported",                   "Не реализовано"} },
    {Errc::Corrupted,        {"Corrupted data",                "Файл повреждён"} },
    {Errc::CompressionError, {"Compression/Decompression err", "Попробуйте другой алгоритм"} },
};
