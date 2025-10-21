#pragma once
#include <stdexcept>
#include <string>

struct VfsError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct PathError       : VfsError { using VfsError::VfsError; };
struct NotFound        : PathError { using PathError::PathError; };
struct NotDirectory    : PathError { using PathError::PathError; };
struct IsDirectory     : PathError { using PathError::PathError; };
struct IsFile          : PathError { using PathError::PathError; };
struct AlreadyExists   : PathError { using PathError::PathError; };
struct InvalidName     : PathError { using PathError::PathError; };
struct RootOperation   : VfsError  { using VfsError::VfsError; };
struct Conflict        : VfsError  { using VfsError::VfsError; };
struct SaveError       : VfsError  { using VfsError::VfsError; };
