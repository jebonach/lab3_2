#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

// Категории ошибок VFS
enum class ErrorType {
    PathError,
    InvalidArg,
    Conflict,
    RootError,
    IOError,
    Unknown
};

class VfsException {
public:
    VfsException(ErrorType t, int code) noexcept : type_(t), code_(code) {}
    const char* what() const noexcept { return "VfsException"; }
    ErrorType type() const noexcept { return type_; }
    int code() const noexcept { return code_; }
private:
    ErrorType type_;
    int code_;
};

struct ErrorInfo { int code; const char* message; };

inline const std::vector<ErrorInfo> g_errorTable = {
    {0, "Path not found"},
    {1, "Not a directory"},
    {2, "Already exists"},
    {3, "Invalid name"},
    {4, "Operation not allowed on root"},
    {5, "Destination is a subdirectory of source"},
    {6, "I/O error while saving"},
    {7, "Parent missing (internal)"},
    {8, "Destination path not found"},
    {9, "Destination is not a directory"}
};

inline const char* getErrorMessage(int code) {
    for (auto& e : g_errorTable) if (e.code == code) return e.message;
    return "Unknown error code";
}

inline const char* typeName(ErrorType t) {
    switch (t) {
        case ErrorType::PathError:  return "PathError";
        case ErrorType::InvalidArg: return "InvalidArg";
        case ErrorType::Conflict:   return "Conflict";
        case ErrorType::RootError:  return "RootError";
        case ErrorType::IOError:    return "IOError";
        default:                    return "Unknown";
    }
}

inline void handleException(const VfsException& ex) {
    std::cout << "[" << typeName(ex.type()) << "] "
              << "code=" << ex.code() << " => "
              << getErrorMessage(ex.code()) << "\n";

    if (ex.code() == 6) {
        std::cout << "hint: check path/permissions for save target\n";
    }
}
