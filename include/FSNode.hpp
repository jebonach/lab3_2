#pragma once
#include <memory>
#include <map>
#include <string>

struct FSNode : std::enable_shared_from_this<FSNode> {
    std::string name;
    bool isFile = false;

    std::weak_ptr<FSNode> parent;
    std::map<std::string, std::shared_ptr<FSNode>> children; // только для каталогов
    std::string data; // содержимое файла (опционально)

    explicit FSNode(std::string n, bool file) : name(std::move(n)), isFile(file) {}
};
