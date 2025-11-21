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
    struct ChildSet {
        std::shared_ptr<FSNode> file;
        std::shared_ptr<FSNode> dir;
    };

    std::string name;
    bool isFile;
    std::weak_ptr<FSNode> parent;
    std::map<std::string, ChildSet> children;

    FileContent content;
    FileProperties fileProps;

    std::shared_ptr<FSNode> getChild(const std::string& name, bool wantFile) const {
        auto it = children.find(name);
        if (it == children.end()) return nullptr;
        return wantFile ? it->second.file : it->second.dir;
    }

    bool hasChild(const std::string& name, bool wantFile) const {
        auto it = children.find(name);
        if (it == children.end()) return false;
        return wantFile ? static_cast<bool>(it->second.file)
                        : static_cast<bool>(it->second.dir);
    }

    void setChild(const std::shared_ptr<FSNode>& child) {
        auto& entry = children[child->name];
        if (child->isFile) entry.file = child;
        else               entry.dir  = child;
    }

    void removeChild(const std::string& name, bool wasFile) {
        auto it = children.find(name);
        if (it == children.end()) return;
        if (wasFile) it->second.file.reset();
        else         it->second.dir.reset();
        if (!it->second.file && !it->second.dir)
            children.erase(it);
    }
};
