#pragma once
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>

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
    WriteError,
    OutOfRange,
    PathError,
    InvalidArg,
    RootError,
    IOError,
    Conflict,
    Corrupted,
    Unsupported
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
    {ErrorCode::WriteError, "Ошибка при записи в файл"},
    {ErrorCode::OutOfRange, "Выход за пределы файла"},
    {ErrorCode::PathError, "Ошибка пути"},
    {ErrorCode::InvalidArg, "Неверный аргумент"},
    {ErrorCode::RootError, "Операция запрещена с корневым узлом"},
    {ErrorCode::IOError, "Ошибка ввода/вывода"},
    {ErrorCode::Conflict, "Конфликт узлов"},
    {ErrorCode::Corrupted, "Повреждённые данные"},
    {ErrorCode::Unsupported, "Формат или алгоритм не поддерживается"}
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

inline void handleException(const VfsException& ex) {
    std::cerr << "Ошибка: " << ex.what() << "\n";
}
