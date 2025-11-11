#include "FileCommands.hpp"
#include "Vfs.hpp"
#include "FSNode.hpp"
#include "FileContent.hpp"
#include "OIStream.hpp"
#include "Errors.hpp"

#include <iostream>
#include <sstream>

namespace FileCommands {

static std::shared_ptr<FSNode> resolveFile(Vfs& vfs, const std::string& path) {
    auto node = vfs.resolve(path);
    if (!node) throw VfsException(ErrorCode::NotFound);
    if (!node->isFile) throw VfsException(ErrorCode::FileExpected);
    return node;
}

void cat(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cout << "usage: cat <file>\n";
        return;
    }
    auto file = resolveFile(vfs, args[0]);
    std::cout << file->content.asText();
}

void echo(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() < 3 || args[args.size()-2] != ">") {
        std::cout << "usage: echo <text> > <file>\n";
        return;
    }
    std::ostringstream oss;
    for (size_t i = 0; i + 2 < args.size(); ++i)
        oss << args[i] << (i + 3 < args.size() ? " " : "");
    auto file = resolveFile(vfs, args.back());
    file->content.assignText(oss.str());
}

void echoAppend(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() < 3 || args[args.size()-2] != ">>") {
        std::cout << "usage: echo <text> >> <file>\n";
        return;
    }
    std::ostringstream oss;
    for (size_t i = 0; i + 2 < args.size(); ++i)
        oss << args[i] << (i + 3 < args.size() ? " " : "");
    auto file = resolveFile(vfs, args.back());
    file->content.append(std::vector<uint8_t>(oss.str().begin(), oss.str().end()));
}

void nano(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cout << "usage: nano <file>\n";
        return;
    }
    auto file = resolveFile(vfs, args[0]);
    std::cout << "Enter text. End with a single '.' on a line.\n";
    std::ostringstream oss;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == ".") break;
        oss << line << '\n';
    }
    file->content.assignText(oss.str());
}

void read(Vfs& vfs, const std::vector<std::string>& args) {
    if (args.size() < 1 || args.size() > 3) {
        std::cout << "usage: read <file> [offset] [count]\n";
        return;
    }
    auto file = resolveFile(vfs, args[0]);
    std::size_t off = args.size() > 1 ? std::stoull(args[1]) : 0;
    std::size_t cnt = args.size() > 2 ? std::stoull(args[2]) : file->content.size() - off;
    auto bytes = file->content.read(off, cnt);
    for (auto b : bytes) {
        std::cout << std::hex << std::uppercase << "0x" << static_cast<int>(b) << " ";
    }
    std::cout << "\n";
}

}
