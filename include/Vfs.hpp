#pragma once

#include "FSNode.hpp"
#include "BStarTree.hpp"
#include "Errors.hpp"

#include <memory>
#include <string>

class Vfs {
public:
    Vfs();

    [[nodiscard("не игнорирь тварь возвращаемый путь")]]
    std::string pwd() const noexcept;

    void cd(const std::string& path);

    void mkdir(const std::string& path);
    void createFile(const std::string& path);
    void rm(const std::string& path);
    void renameNode(const std::string& path,
                    const std::string& newName);
    void mv(const std::string& src, const std::string& dstDir);

    void ls(const std::string& path = "") const;
    void printTree() const;


    [[nodiscard("ты забыл проверить на nullptr")]]
    std::shared_ptr<FSNode> findFileByName(const std::string& name) const;

    void saveJson(const std::string& jsonPath) const;

private:
    using NodePtr  = std::shared_ptr<FSNode>;
    using WNodePtr = std::weak_ptr<FSNode>;

    NodePtr root_;
    NodePtr cwd_;

    BStarTree<std::string, WNodePtr> fileIndex_;

    [[nodiscard("и туут тоже nullptr забыл")]]
    NodePtr resolve(const std::string& path) const;

    [[nodiscard("тут думать надо nullptr или нет")]]
    NodePtr resolveParent(const std::string& path, std::string& leafName) const;

    static std::string fullPathOf(const NodePtr& n);
    static void        printTreeRec(const NodePtr& n, int depth);
    static bool        isSubtreeOf(const NodePtr& a, const NodePtr& b);

    void indexInsertIfFile(const NodePtr& n);
    void indexEraseIfFile(const NodePtr& n);
    void indexEraseSubtree(const NodePtr& n);
};
