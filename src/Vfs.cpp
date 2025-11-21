#include "Vfs.hpp"
#include "Path.hpp"
#include "JsonIO.hpp"
#include "Compression.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
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
    initFileProps(f);
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

void Vfs::cp(const std::string& srcPath, const std::string& dstPath) {
    auto src = resolve(srcPath);
    if (!src) throw VfsException(ErrorCode::PathError);
    if (src == root_) throw VfsException(ErrorCode::RootError);

    NodePtr targetDir;
    std::string desiredName;

    auto dstNode = resolve(dstPath);
    if (dstNode) {
        if (dstNode->isFile) {
            targetDir = dstNode->parent.lock();
            if (!targetDir) throw VfsException(ErrorCode::PathError);
            desiredName = dstNode->name;
        } else {
            targetDir = dstNode;
            desiredName = src->name;
        }
    } else {
        std::string leaf;
        auto parent = resolveParent(dstPath, leaf);
        if (!parent) throw VfsException(ErrorCode::PathError);
        if (parent->isFile) throw VfsException(ErrorCode::InvalidArg);
        if (leaf.empty() || leaf=="." || leaf==".." || leaf.find('/')!=std::string::npos)
            throw VfsException(ErrorCode::InvalidArg);
        targetDir = parent;
        desiredName = leaf;
    }

    if (!targetDir || targetDir->isFile)
        throw VfsException(ErrorCode::InvalidArg);

    if (!src->isFile && isSubtreeOf(targetDir, src))
        throw VfsException(ErrorCode::Conflict);

    auto finalName = makeUniqueName(targetDir, desiredName);
    copyNodeRec(src, targetDir, finalName);
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

std::vector<Vfs::NodePtr> Vfs::findFilesByName(const std::string& name) const {
    std::vector<NodePtr> out;
    auto opt = fileIndex_.find(name);
    if (!opt) return out;
    auto bucket = *opt;
    if (!bucket) return out;
    for (const auto& weak : *bucket) {
        if (auto node = weak.lock()) out.push_back(node);
    }
    return out;
}

void Vfs::saveJson(const std::string& jsonPath) {
    if (jsonPath.empty()) throw VfsException(ErrorCode::InvalidArg);
    std::string leaf;
    auto parent = resolveParent(jsonPath, leaf);
    if (!parent) throw VfsException(ErrorCode::PathError);
    if (parent->isFile) throw VfsException(ErrorCode::InvalidArg);
    if (leaf.empty() || leaf=="." || leaf==".." || leaf.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);

    NodePtr target;
    auto it = parent->children.find(leaf);
    if (it != parent->children.end()) {
        if (!it->second->isFile) throw VfsException(ErrorCode::InvalidArg);
        target = it->second;
    } else {
        target = std::make_shared<FSNode>(leaf, true);
        target->parent = parent;
        parent->children.emplace(leaf, target);
        initFileProps(target);
        indexInsertIfFile(target);
    }

    auto json = JsonIO::treeToJson(root_);
    target->content.assignText(json);
    touchFile(target);
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

std::string Vfs::makeUniqueName(const NodePtr& parent, const std::string& base) const {
    if (!parent) throw VfsException(ErrorCode::PathError);
    if (!parent->children.count(base)) return base;

    auto dotPos = base.find_last_of('.');
    std::string stem = base;
    std::string ext;
    if (dotPos != std::string::npos && dotPos != 0) {
        stem = base.substr(0, dotPos);
        ext = base.substr(dotPos);
    } else {
        ext.clear();
    }

    for (int idx = 1;; ++idx) {
        auto candidate = stem + "(" + std::to_string(idx) + ")" + ext;
        if (!parent->children.count(candidate))
            return candidate;
    }
}

Vfs::NodePtr Vfs::copyNodeRec(const NodePtr& src, const NodePtr& destParent, const std::string& name) {
    auto clone = std::make_shared<FSNode>(name, src->isFile);
    clone->parent = destParent;
    destParent->children.emplace(name, clone);
    if (clone->isFile) {
        clone->content.replaceAll(src->content.bytes());
        initFileProps(clone);
        indexInsertIfFile(clone);
    } else {
        for (auto& [childName, childNode] : src->children) {
            copyNodeRec(childNode, clone, childName);
        }
    }
    return clone;
}

void Vfs::compressNode(const NodePtr& node) {
    if (!node) return;
    if (node->isFile) {
        compressInplace(node->content, CompAlgo::LZW);
        touchFile(node);
        return;
    }
    for (auto& [_, child] : node->children) {
        compressNode(child);
    }
}

void Vfs::decompressNode(const NodePtr& node) {
    if (!node) return;
    if (node->isFile) {
        if (isCompressed(node->content)) {
            uncompressInplace(node->content);
            touchFile(node);
        }
        return;
    }
    for (auto& [_, child] : node->children) {
        decompressNode(child);
    }
}

void Vfs::initFileProps(const NodePtr& node) {
    if (!node || !node->isFile) return;
    auto now = std::time(nullptr);
    node->fileProps.createdAt = now;
    node->fileProps.modifiedAt = now;
    node->fileProps.byteSize = node->content.size();
    node->fileProps.charCount = node->content.asText().size();
}

void Vfs::touchFile(const NodePtr& node) {
    if (!node || !node->isFile) return;
    auto now = std::time(nullptr);
    if (node->fileProps.createdAt == 0) node->fileProps.createdAt = now;
    node->fileProps.modifiedAt = now;
    node->fileProps.byteSize = node->content.size();
    node->fileProps.charCount = node->content.asText().size();
}

void Vfs::indexInsertIfFile(const std::shared_ptr<FSNode>& n) {
    if (!n->isFile) return;
    auto bucketOpt = fileIndex_.find(n->name);
    if (bucketOpt) {
        auto bucket = *bucketOpt;
        if (bucket) bucket->push_back(std::weak_ptr<FSNode>(n));
        return;
    }
    auto bucket = std::make_shared<std::vector<WNodePtr>>();
    bucket->push_back(std::weak_ptr<FSNode>(n));
    fileIndex_.insert(n->name, bucket);
}

void Vfs::indexEraseIfFile(const std::shared_ptr<FSNode>& n) {
    if (!n->isFile) return;
    auto bucketOpt = fileIndex_.find(n->name);
    if (!bucketOpt) return;
    auto bucket = *bucketOpt;
    if (!bucket) return;
    auto& vec = *bucket;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const WNodePtr& w){
        auto sp = w.lock();
        return !sp || sp == n;
    }), vec.end());
    if (vec.empty()) fileIndex_.erase(n->name);
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
    touchFile(f);
}

void Vfs::compress(const std::string& path) {
    auto node = resolve(path);
    if (!node) throw VfsException(ErrorCode::PathError);
    compressNode(node);
}

void Vfs::decompress(const std::string& path) {
    auto node = resolve(path);
    if (!node) throw VfsException(ErrorCode::PathError);
    decompressNode(node);
}

void Vfs::refreshFileStats(const NodePtr& node) {
    touchFile(node);
}
