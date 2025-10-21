#pragma once
#include "FSNode.hpp"
#include "BStarTree.hpp"
#include <string>
#include <memory>

class Vfs {
public:
    Vfs();

    // навигация / информация
    std::string pwd() const noexcept;
    void cd(const std::string& path);                 // throws

    // модификации
    void mkdir(const std::string& path);              // throws
    void createFile(const std::string& path);         // throws
    void rm(const std::string& path);                 // throws
    void renameNode(const std::string& path, const std::string& newName); // throws
    void mv(const std::string& src, const std::string& dstDir);           // throws

    // вывод
    void ls(const std::string& path = "") const;      // throws (если путь некорректен)
    void printTree() const;

    // индекс поиска
    std::shared_ptr<FSNode> findFileByName(const std::string& name) const; // nullptr если нет

    // сериализация
    void saveJson(const std::string& jsonPath) const; // throws

private:
    std::shared_ptr<FSNode> root_;
    std::shared_ptr<FSNode> cwd_;

    // имя файла -> узел (храним weak_ptr)
    BStarTree<std::string, std::weak_ptr<FSNode>> fileIndex_;

    // вспомогательные
    std::shared_ptr<FSNode> resolve(const std::string& path) const; // nullptr если нет
    std::shared_ptr<FSNode> resolveParent(const std::string& path, std::string& leafName) const;

    static std::string fullPathOf(const std::shared_ptr<FSNode>& n);
    static void printTreeRec(const std::shared_ptr<FSNode>& n, int depth);
    static bool isSubtreeOf(const std::shared_ptr<FSNode>& a, const std::shared_ptr<FSNode>& b);

    void indexInsertIfFile(const std::shared_ptr<FSNode>& n);
    void indexEraseIfFile(const std::shared_ptr<FSNode>& n);
    void indexEraseSubtree(const std::shared_ptr<FSNode>& n);
};
