#include "Vfs.hpp"
#include "Path.hpp"
#include "JsonIO.hpp"
#include <iostream>
#include <vector>
#include "FileContent.hpp"

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
    if (!dest)          throw VfsException(ErrorCode::PathError);
    if (dest->isFile)   throw VfsException(ErrorCode::InvalidArg);
    cwd_ = dest;
}

void Vfs::mkdir(const std::string& path) {
    if (path.empty()) throw VfsException(ErrorCode::InvalidArg);
    std::string name;
    auto parent = resolveParent(path, name);
    if (!parent)             throw VfsException(ErrorCode::PathError);
    if (parent->isFile)      throw VfsException(ErrorCode::InvalidArg);
    if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);
    if (parent->children.count(name)) throw VfsException(ErrorCode::InvalidArg);
    auto dir = std::make_shared<FSNode>(name, false);
    dir->parent = parent;
    parent->children.emplace(name, dir);
}

void Vfs::createFile(const std::string& path) {
    if (path.empty()) throw VfsException(ErrorCode::InvalidArg);
    std::string name;
    auto parent = resolveParent(path, name);
    if (!parent)        throw VfsException(ErrorCode::PathError);
    if (parent->isFile) throw VfsException(ErrorCode::InvalidArg);
    if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);
    if (parent->children.count(name)) throw VfsException(ErrorCode::InvalidArg);
    auto f = std::make_shared<FSNode>(name, true);
    f->parent = parent;
    parent->children.emplace(name, f);
    indexInsertIfFile(f);
}

void Vfs::renameNode(const std::string& path, const std::string& newName) {
    if (newName.empty() || newName=="." || newName==".." || newName.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);

    auto n = resolve(path);
    if (!n)            throw VfsException(ErrorCode::PathError);
    if (n==root_)      throw VfsException(ErrorCode::RootError);
    auto p = n->parent.lock();
    if (!p)            throw VfsException(ErrorCode::PathError);
    if (p->children.count(newName)) throw VfsException(ErrorCode::InvalidArg);

    if (n->isFile) indexEraseIfFile(n);
    p->children.erase(n->name);
    n->name = newName;
    p->children.emplace(newName, n);
    if (n->isFile) indexInsertIfFile(n);
}

void Vfs::mv(const std::string& src, const std::string& dstDir) {
    auto node = resolve(src);
    if (!node) throw VfsException(ErrorCode::PathError);
    if (node==root_) throw VfsException(ErrorCode::RootError);

    auto dst = resolve(dstDir);
    if (!dst)         throw VfsException(ErrorCode::PathError);
    if (dst->isFile)  throw VfsException(ErrorCode::InvalidArg);
    if (!node->isFile && isSubtreeOf(dst, node))
        throw VfsException(ErrorCode::Conflict);

    auto p = node->parent.lock();
    if (!p) throw VfsException(ErrorCode::PathError);
    if (dst->children.count(node->name)) throw VfsException(ErrorCode::InvalidArg);

    p->children.erase(node->name);
    node->parent = dst;
    dst->children.emplace(node->name, node);
}

void Vfs::rm(const std::string& path) {
    auto node = resolve(path);
    if (!node)   throw VfsException(ErrorCode::PathError);
    if (node==root_) throw VfsException(ErrorCode::RootError);
    auto p = node->parent.lock();
    if (!p) throw VfsException(ErrorCode::PathError);

    indexEraseSubtree(node);
    p->children.erase(node->name);
}

void Vfs::ls(const std::string& path) const {
    auto n = path.empty() ? cwd_ : resolve(path);
    if (!n)         throw VfsException(ErrorCode::PathError);
    if (n->isFile)  throw VfsException(ErrorCode::InvalidArg);
    for (auto& [name, child] : n->children) {
        std::cout << (child->isFile? "  📄 ":"  📁 ") << name << (child->isFile? "" : "/") << "\n";
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
        throw VfsException(ErrorCode::IOError);
}

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
    std::cout << (n->isFile? "📄 " : "📁 ") << n->name << (n->isFile? "" : "/") << "\n";
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

std::string Vfs::readFile(const std::string& path) const {
    auto f = resolve(path);
    if (!f) throw VfsException(ErrorCode::PathError);
    if (!f->isFile) throw VfsException(ErrorCode::InvalidArg);
    return f->content.asText();
}

void Vfs::writeFile(const std::string& path, const std::string& content, bool append) {
    auto f = resolve(path);
    if (!f) throw VfsException(ErrorCode::PathError);
    if (!f->isFile) throw VfsException(ErrorCode::InvalidArg);
    if (append) f->content.append(std::vector<uint8_t>(content.begin(), content.end()));
    else        f->content.assignText(content);
}

void Vfs::compressFile(const std::string& path) {
    auto f = resolve(path);
    if (!f) throw VfsException(ErrorCode::PathError);
    if (!f->isFile) throw VfsException(ErrorCode::InvalidArg);

    const auto& data = f->content.bytes();
    std::vector<uint8_t> encoded;
    for (size_t i = 0; i < data.size(); ) {
        uint8_t b = data[i];
        size_t j = i + 1;
        while (j < data.size() && data[j] == b && j - i < 255) ++j;
        encoded.push_back(static_cast<uint8_t>(j - i));
        encoded.push_back(b);
        i = j;
    }
    f->content.replaceAll(encoded);
}

void Vfs::decompressFile(const std::string& path) {
    auto f = resolve(path);
    if (!f) throw VfsException(ErrorCode::PathError);
    if (!f->isFile) throw VfsException(ErrorCode::InvalidArg);

    const auto& data = f->content.bytes();
    std::vector<uint8_t> decoded;
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint8_t count = data[i], value = data[i + 1];
        decoded.insert(decoded.end(), count, value);
    }
    f->content.replaceAll(decoded);
}
