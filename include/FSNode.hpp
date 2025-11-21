#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <ctime>
#include "FileContent.hpp"

struct FileProperties {
    std::time_t createdAt{0};
    std::time_t modifiedAt{0};
    std::size_t charCount{0};
    std::size_t byteSize{0};
};

struct FSNode {
    std::string name;
    bool isFile;
    std::weak_ptr<FSNode> parent;
    std::map<std::string, std::shared_ptr<FSNode>> children;

    FileContent content;
    FileProperties fileProps;

    std::shared_ptr<FSNode> getChild(const std::string& name) const;
    void addChild(const std::shared_ptr<FSNode>& child);
    void removeChild(const std::string& name);
    bool hasChild(const std::string& name) const;
};
