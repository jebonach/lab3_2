#pragma once

#include "FSNode.hpp"
#include "BStarTree.hpp"
#include "Errors.hpp"
#include <memory>
#include <string>
#include <vector>

class Vfs {
public:
    using NodePtr = std::shared_ptr<FSNode>;
    using WNodePtr = std::weak_ptr<FSNode>;
    using IndexBucket = std::shared_ptr<std::vector<WNodePtr>>;

    Vfs();

    [[nodiscard("use result")]] std::string pwd() const noexcept;
    void cd(const std::string& path);

    void mkdir(const std::string& path);
    void createFile(const std::string& path);
    void rm(const std::string& path);
    void renameNode(const std::string& path, const std::string& newName);
    void mv(const std::string& src, const std::string& dstDir);
    void cp(const std::string& src, const std::string& dstPath);
    void writeFile(const std::string& path, const std::string& content, bool append);
    void compress(const std::string& path);
    void decompress(const std::string& path);

    void writeToFile(const std::string& path, const std::string& content);
    [[nodiscard("check file content")]] std::string readFile(const std::string& path) const;
    [[nodiscard("check node")]] NodePtr resolve(const std::string& path) const;  // теперь знает про NodePtr
    void refreshFileStats(const NodePtr& node);

    void ls(const std::string& path = "") const;
    void printTree() const;

    [[nodiscard("check if files found")]] std::vector<NodePtr> findFilesByName(const std::string& name) const;

    void saveJson(const std::string& jsonPath);
    void loadJson(const std::string& jsonPath);

private:
    NodePtr root_;
    NodePtr cwd_;
    BStarTree<std::string, IndexBucket> fileIndex_;

    [[nodiscard("check parent")]] NodePtr resolveParent(const std::string& path, std::string& leafName) const;

    static std::string fullPathOf(const NodePtr& n);
    static void printTreeRec(const NodePtr& n, int depth);
    static bool isSubtreeOf(const NodePtr& a, const NodePtr& b);

    std::string makeUniqueName(const NodePtr& parent, const std::string& base) const;
    NodePtr copyNodeRec(const NodePtr& src, const NodePtr& destParent, const std::string& name);
    void compressNode(const NodePtr& node);
    void decompressNode(const NodePtr& node);
    void initFileProps(const NodePtr& node);
    void touchFile(const NodePtr& node);

    void indexInsertIfFile(const NodePtr& n);
    void indexEraseIfFile(const NodePtr& n);
    void indexEraseSubtree(const NodePtr& n);
};
