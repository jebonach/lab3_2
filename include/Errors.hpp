#pragma once
#include <map>
#include <string>
#include <stdexcept>

enum class ErrorCode {
    NotFound,
    AlreadyExists,
    NotADirectory,
    IsADirectory,
    InvalidPath,
    PermissionDenied,
    EmptyFileName,
    FileExpected,
    DirectoryExpected,
    ReadError,
    WriteError
};

inline const std::map<ErrorCode, std::string> g_errorTable = {
    {ErrorCode::NotFound, "Файл или директория не найдены"},
    {ErrorCode::AlreadyExists, "Файл или директория уже существуют"},
    {ErrorCode::NotADirectory, "Ожидалась директория"},
    {ErrorCode::IsADirectory, "Ожидался файл, но передана директория"},
    {ErrorCode::InvalidPath, "Неверный путь"},
    {ErrorCode::PermissionDenied, "Отказано в доступе"},
    {ErrorCode::EmptyFileName, "Имя файла не может быть пустым"},
    {ErrorCode::FileExpected, "Ожидался файл"},
    {ErrorCode::DirectoryExpected, "Ожидалась директория"},
    {ErrorCode::ReadError, "Ошибка при чтении файла"},
    {ErrorCode::WriteError, "Ошибка при записи в файл"}
};

class VfsException : public std::runtime_error {
public:
    ErrorCode code;
    explicit VfsException(ErrorCode code)
        : std::runtime_error(g_errorTable.at(code)), code(code) {}
};

inline void throwIf(bool cond, ErrorCode code) {
    if (cond) throw VfsException(code);
}
