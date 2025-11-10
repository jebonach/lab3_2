// File: include/commands.hpp
#pragma once

#include "Vfs.hpp"
#include "Errors.hpp"
#include "IOStream.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

inline void cmd_cat(Vfs& vfs, const std::string& path) {
    auto node = vfs.resolve(path);
    if (!node || !node->isFile) throw VfsException(ErrorCode::NotAFile);
    IStream stream;
    stream.load(node->content);
    while (!stream.eof()) {
        std::cout << stream.readLine() << '\n';
    }
}

inline void cmd_nano(Vfs& vfs, const std::string& path) {
    auto node = vfs.resolve(path);
    if (!node) {
        vfs.createFile(path);
        node = vfs.resolve(path);
    }
    if (!node->isFile) throw VfsException(ErrorCode::NotAFile);

    std::cout << "Enter file content. End with a single dot on a line:\n";
    std::ostringstream oss;
    std::string line;
    while (true) {
        std::getline(std::cin, line);
        if (line == ".") break;
        oss << line << '\n';
    }
    node->content = oss.str();
}

inline void cmd_echo(Vfs& vfs, const std::string& path, const std::string& text, bool append = false) {
    auto node = vfs.resolve(path);
    if (!node) {
        vfs.createFile(path);
        node = vfs.resolve(path);
    }
    if (!node->isFile) throw VfsException(ErrorCode::NotAFile);
    if (append) node->content += text + "\n";
    else node->content = text + "\n";
}

inline void cmd_read(Vfs& vfs, const std::string& path) {
    auto node = vfs.resolve(path);
    if (!node || !node->isFile) throw VfsException(ErrorCode::NotAFile);
    IStream stream;
    stream.load(node->content);
    auto bytes = stream.readAllBytes();
    for (uint8_t b : bytes) {
        std::cout << int(b) << ' ';
    }
    std::cout << '\n';
}
