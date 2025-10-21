#include "Vfs.hpp"
#include "Path.hpp"
#include "JsonIO.hpp"
#include "Error.hpp"
#include <iostream>

Vfs::Vfs() : fileIndex_(7) {
    root_ = std::make_shared<FSNode>("/", false);
    cwd_  = root_;
}

std::string Vfs::pwd() const noexcept {
    return fullPathOf(cwd_);
}

std::shared_ptr<FSNode> Vfs::resolve(const std::string& path) const {
    if (path.empty()) return cwd_;
    std::shared_ptr<FSNode> cur = (path[0]=='/') ? root_ : cwd_;
    for (auto& name : splitPath(path)) {
        if (name=="." ) continue;
        if (name=="..") {
            if (auto p = cur->parent.lock()) cur = p;
            continue;
        }
        auto it = cur->children.find(name);
        if (it==cur->children.end()) return nullptr;
        cur = it->second;
    }
    return cur;
}

std::shared_ptr<FSNode> Vfs::resolveParent(const std::string& path, std::string& leafName) const {
    auto parts = splitPath(path);
    if (parts.empty()) return nullptr;
    leafName = parts.back();
    parts.pop_back();
    std::string parentPath = "/";
    for (size_t i=0;i<parts.size();++i) {
        parentPath += parts[i];
        if (i+1<parts.size()) parentPath += "/";
    }
    if (path[0] != '/' && parentPath=="/") parentPath = "";
    return resolve(parentPath);
}

void Vfs::cd(const std::string& path) {
    auto dest = resolve(path);
    if (!dest)          throw NotFound("cd: path not found");
    if (dest->isFile)   throw NotDirectory("cd: not a directory");
    cwd_ = dest;
}

void Vfs::mkdir(const std::string& path) {
    if (path.empty()) throw InvalidName("mkdir: empty name");
    std::string name;
    auto parent = resolveParent(path, name);
    if (!parent)             throw NotFound("mkdir: parent not found");
    if (parent->isFile)      throw NotDirectory("mkdir: parent is file");
    if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos)
        throw InvalidName("mkdir: invalid name");
    if (parent->children.count(name)) throw AlreadyExists("mkdir: already exists");
    auto dir = std::make_shared<FSNode>(name, false);
    dir->parent = parent;
    parent->children.emplace(name, dir);
}

void Vfs::createFile(const std::string& path) {
    if (path.empty()) throw InvalidName("create: empty name");
    std::string name;
    auto parent = resolveParent(path, name);
    if (!parent)        throw NotFound("create: parent not found");
    if (parent->isFile) throw NotDirectory("create: parent is file");
    if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos)
        throw InvalidName("create: invalid name");
    if (parent->children.count(name)) throw AlreadyExists("create: already exists");
    auto f = std::make_shared<FSNode>(name, true);
    f->parent = parent;
    parent->children.emplace(name, f);
    indexInsertIfFile(f);
}

void Vfs::renameNode(const std::string& path, const std::string& newName) {
    if (newName.empty() || newName=="." || newName==".." || newName.find('/')!=std::string::npos)
        throw InvalidName("rename: invalid new name");

    auto n = resolve(path);
    if (!n)            throw NotFound("rename: path not found");
    if (n==root_)      throw RootOperation("rename: cannot rename root");
    auto p = n->parent.lock();
    if (!p)            throw PathError("rename: parent missing");
    if (p->children.count(newName)) throw AlreadyExists("rename: target exists");

    if (n->isFile) indexEraseIfFile(n);
    p->children.erase(n->name);
    n->name = newName;
    p->children.emplace(newName, n);
    if (n->isFile) indexInsertIfFile(n);
}

void Vfs::mv(const std::string& src, const std::string& dstDir) {
    auto node = resolve(src);
    if (!node) throw NotFound("mv: source not found");
    if (node==root_) throw RootOperation("mv: cannot move root");

    auto dst = resolve(dstDir);
    if (!dst)         throw NotFound("mv: destination not found");
    if (dst->isFile)  throw NotDirectory("mv: destination is file");
    if (!node->isFile && isSubtreeOf(dst, node))
        throw Conflict("mv: cannot move directory into its subtree");

    auto p = node->parent.lock();
    if (!p) throw PathError("mv: parent missing");
    if (dst->children.count(node->name)) throw AlreadyExists("mv: name conflict in destination");

    p->children.erase(node->name);
    node->parent = dst;
    dst->children.emplace(node->name, node);
}

void Vfs::rm(const std::string& path) {
    auto node = resolve(path);
    if (!node)   throw NotFound("rm: path not found");
    if (node==root_) throw RootOperation("rm: cannot remove root");
    auto p = node->parent.lock();
    if (!p) throw PathError("rm: parent missing");

    indexEraseSubtree(node);
    p->children.erase(node->name);
}

void Vfs::ls(const std::string& path) const {
    auto n = path.empty() ? cwd_ : resolve(path);
    if (!n)         throw NotFound("ls: path not found");
    if (n->isFile)  throw NotDirectory("ls: not a directory");
    for (auto& [name, child] : n->children) {
        std::cout << (child->isFile? "  📄 ":"  📁 ") << name << (child->isFile? "":"\/") << "\n";
    }
}

void Vfs::printTree() const { printTreeRec(root_, 0); }

std::shared_ptr<FSNode> Vfs::findFileByName(const std::string& name) const {
    auto opt = fileIndex_.find(name);
    if (!opt) return nullptr;
    if (auto sp = opt->lock()) return sp;
    return nullptr;
}

void Vfs::saveJson(const std::string& jsonPath) const {
    if (!JsonIO::saveTreeToJsonFile(root_, jsonPath))
        throw SaveError("save: cannot write json");
}

// ===== helpers =====
std::string Vfs::fullPathOf(const std::shared_ptr<FSNode>& n) {
    if (!n) return "/";
    std::vector<std::string> parts;
    auto cur = n;
    while (cur) { parts.push_back(cur->name); cur = cur->parent.lock(); }
    std::string out;
    for (auto it = parts.rbegin(); it!=parts.rend(); ++it) {
        if (it==parts.rbegin() && *it=="/") { out = "/"; continue; }
        if (out.size()>1) out += "/";
        out += *it;
    }
    if (out.empty()) out = "/";
    return out;
}

void Vfs::printTreeRec(const std::shared_ptr<FSNode>& n, int depth) {
    for (int i=0;i<depth;++i) std::cout << "  ";
    std::cout << (n->isFile? "📄 ":"📁 ") << n->name << (n->isFile? "":"\/") << "\n";
    if (!n->isFile) for (auto& [_, ch] : n->children) printTreeRec(ch, depth+1);
}

bool Vfs::isSubtreeOf(const std::shared_ptr<FSNode>& a, const std::shared_ptr<FSNode>& b) {
    auto cur = a;
    while (cur) { if (cur==b) return true; cur = cur->parent.lock(); }
    return false;
}

void Vfs::indexInsertIfFile(const std::shared_ptr<FSNode>& n) {
    if (n->isFile) fileIndex_.insert(n->name, std::weak_ptr<FSNode>(n));
}
void Vfs::indexEraseIfFile(const std::shared_ptr<FSNode>& n) {
    if (n->isFile) fileIndex_.erase(n->name);
}
void Vfs::indexEraseSubtree(const std::shared_ptr<FSNode>& n) {
    if (n->isFile) indexEraseIfFile(n);
    else for (auto& [_, ch] : n->children) indexEraseSubtree(ch);
}
