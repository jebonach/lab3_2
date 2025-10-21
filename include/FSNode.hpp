#pragma once
#include <memory>
#include <map>
#include <string>
#include <vector>

struct FSNode : std::enable_shared_from_this<FSNode> {
    std::string name;
    bool isFile = false;

    std::weak_ptr<FSNode> parent;

    std::map<std::string, std::shared_ptr<FSNode>> children;

    std::string data;

    explicit FSNode(std::string n, bool file)
        : name(std::move(n)), isFile(file) {}
};
