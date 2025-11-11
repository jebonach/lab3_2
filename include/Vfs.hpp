#pragma once

#include "FSNode.hpp"
#include "BStarTree.hpp"
#include "Errors.hpp"
#include <memory>
#include <string>

class Vfs {
public:
    using NodePtr = std::shared_ptr<FSNode>;
    using WNodePtr = std::weak_ptr<FSNode>;

    Vfs();

    [[nodiscard("use result")]] std::string pwd() const noexcept;
    void cd(const std::string& path);

    void mkdir(const std::string& path);
    void createFile(const std::string& path);
    void rm(const std::string& path);
    void renameNode(const std::string& path, const std::string& newName);
    void mv(const std::string& src, const std::string& dstDir);
    void writeFile(const std::string& path, const std::string& content, bool append);
    void compressFile(const std::string& path);
    void decompressFile(const std::string& path);

    void writeToFile(const std::string& path, const std::string& content);
    [[nodiscard("check file content")]] std::string readFile(const std::string& path) const;
    [[nodiscard("check node")]] NodePtr resolve(const std::string& path) const;  // теперь знает про NodePtr

    void ls(const std::string& path = "") const;
    void printTree() const;

    [[nodiscard("check if file found")]] std::shared_ptr<FSNode> findFileByName(const std::string& name) const;

    void saveJson(const std::string& jsonPath) const;
    void loadJson(const std::string& jsonPath);

private:
    NodePtr root_;
    NodePtr cwd_;
    BStarTree<std::string, WNodePtr> fileIndex_;

    [[nodiscard("check parent")]] NodePtr resolveParent(const std::string& path, std::string& leafName) const;

    static std::string fullPathOf(const NodePtr& n);
    static void printTreeRec(const NodePtr& n, int depth);
    static bool isSubtreeOf(const NodePtr& a, const NodePtr& b);

    void indexInsertIfFile(const NodePtr& n);
    void indexEraseIfFile(const NodePtr& n);
    void indexEraseSubtree(const NodePtr& n);
};
