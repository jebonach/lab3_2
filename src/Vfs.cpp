#include "Vfs.hpp"
#include "Path.hpp"
#include "JsonIO.hpp"
#include "Compression.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include "FileContent.hpp"

namespace {

using NodePtr = std::shared_ptr<FSNode>;

NodePtr pickChild(const FSNode::ChildSet& entry, Vfs::ResolveKind pref) {
    switch (pref) {
        case Vfs::ResolveKind::Directory:
            if (entry.dir)  return entry.dir;
            return nullptr;
        case Vfs::ResolveKind::File:
            if (entry.file) return entry.file;
            return nullptr;
        case Vfs::ResolveKind::Any:
        default:
            if (entry.file) return entry.file;
            return entry.dir;
    }
}

template<class Fn>
void forEachChild(const NodePtr& parent, Fn&& fn) {
    if (!parent) return;
    for (auto& [name, bucket] : parent->children) {
        if (bucket.dir)  fn(bucket.dir);
        if (bucket.file) fn(bucket.file);
    }
}

}

Vfs::Vfs() : nameIndex_(7) {
    root_ = std::make_shared<FSNode>("/", false);
    cwd_  = root_;
    initNodeProps(root_);
    indexInsert(root_);
}

std::string Vfs::pwd() const noexcept {
    return fullPathOf(cwd_);
}

std::shared_ptr<FSNode> Vfs::resolve(const std::string& path, ResolveKind preference) const {
    auto applyPreference = [&](const NodePtr& node, ResolveKind pref) -> NodePtr {
        if (!node) return nullptr;
        if (pref == ResolveKind::Directory && node->isFile) return nullptr;
        if (pref == ResolveKind::File && !node->isFile) return nullptr;
        return node;
    };

    if (path.empty()) return applyPreference(cwd_, preference);

    bool explicitDirHint = (!path.empty() && path.back() == '/');
    auto parts = splitPath(path);
    NodePtr cur = (path[0]=='/') ? root_ : cwd_;
    if (parts.empty()) {
        return applyPreference((path[0]=='/') ? root_ : cwd_,
                               explicitDirHint && preference == ResolveKind::Any ? ResolveKind::Directory : preference);
    }

    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& name = parts[i];
        if (name=="." ) continue;
        if (name=="..") {
            if (auto p = cur->parent.lock()) cur = p;
            continue;
        }
        auto it = cur->children.find(name);
        if (it==cur->children.end()) return nullptr;
        bool last = (i+1 == parts.size());
        if (!last) {
            cur = it->second.dir;
            if (!cur) return nullptr;
        } else {
            auto pref = preference;
            if (pref == ResolveKind::Any && explicitDirHint)
                pref = ResolveKind::Directory;
            cur = pickChild(it->second, pref);
            if (!cur) return nullptr;
        }
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
    auto parent = resolve(parentPath, ResolveKind::Directory);
    if (parent) return parent;
    // if a node exists but isn't a directory, allow caller to detect it
    return resolve(parentPath, ResolveKind::Any);
}

void Vfs::cd(const std::string& path) {
    auto dest = resolve(path, ResolveKind::Directory);
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
    std::string finalName = parent->hasChild(name, false) ? makeUniqueName(parent, name, false) : name;
    auto dir = std::make_shared<FSNode>(finalName, false);
    dir->parent = parent;
    parent->setChild(dir);
    initNodeProps(dir);
    indexInsert(dir);
    touchNode(parent);
}

void Vfs::createFile(const std::string& path) {
    if (path.empty()) throw VfsException(ErrorCode::InvalidArg);
    std::string name;
    auto parent = resolveParent(path, name);
    if (!parent)        throw VfsException(ErrorCode::PathError);
    if (parent->isFile) throw VfsException(ErrorCode::InvalidArg);
    if (name.empty() || name=="." || name==".." || name.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);
    std::string finalName = parent->hasChild(name, true) ? makeUniqueName(parent, name, true) : name;
    auto f = std::make_shared<FSNode>(finalName, true);
    f->parent = parent;
    parent->setChild(f);
    initNodeProps(f);
    indexInsert(f);
    touchNode(parent);
}

void Vfs::renameNode(const std::string& path, const std::string& newName) {
    if (newName.empty() || newName=="." || newName==".." || newName.find('/')!=std::string::npos)
        throw VfsException(ErrorCode::InvalidArg);

    auto n = resolve(path);
    if (!n)            throw VfsException(ErrorCode::PathError);
    if (n==root_)      throw VfsException(ErrorCode::RootError);
    auto p = n->parent.lock();
    if (!p)            throw VfsException(ErrorCode::PathError);
    if (newName == n->name) return;
    if (p->hasChild(newName, n->isFile)) throw VfsException(ErrorCode::InvalidArg);

    indexErase(n);
    p->removeChild(n->name, n->isFile);
    n->name = newName;
    p->setChild(n);
    indexInsert(n);
    touchNode(p);
}

void Vfs::mv(const std::string& src, const std::string& dstDir) {
    auto node = resolve(src);
    if (!node) throw VfsException(ErrorCode::PathError);
    if (node==root_) throw VfsException(ErrorCode::RootError);

    auto dst = resolve(dstDir, ResolveKind::Directory);
    if (!dst)         throw VfsException(ErrorCode::PathError);
    if (dst->isFile)  throw VfsException(ErrorCode::InvalidArg);
    if (!node->isFile && isSubtreeOf(dst, node))
        throw VfsException(ErrorCode::Conflict);

    auto p = node->parent.lock();
    if (!p) throw VfsException(ErrorCode::PathError);
    if (dst->hasChild(node->name, node->isFile)) throw VfsException(ErrorCode::InvalidArg);

    p->removeChild(node->name, node->isFile);
    node->parent = dst;
    dst->setChild(node);
    touchNode(p);
    touchNode(dst);
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

    auto finalName = makeUniqueName(targetDir, desiredName, src->isFile);
    copyNodeRec(src, targetDir, finalName);
    touchNode(targetDir);
}

void Vfs::rm(const std::string& path) {
    auto node = resolve(path);
    if (!node)   throw VfsException(ErrorCode::PathError);
    if (node==root_) throw VfsException(ErrorCode::RootError);
    auto p = node->parent.lock();
    if (!p) throw VfsException(ErrorCode::PathError);

    indexEraseSubtree(node);
    p->removeChild(node->name, node->isFile);
    touchNode(p);
}

void Vfs::ls(const std::string& path) const {
    auto n = path.empty() ? cwd_ : resolve(path, ResolveKind::Directory);
    if (!n)         throw VfsException(ErrorCode::PathError);
    if (n->isFile)  throw VfsException(ErrorCode::InvalidArg);
    for (auto& [name, bucket] : n->children) {
        if (bucket.dir)  std::cout << "  📁 " << name << "/\n";
        if (bucket.file) std::cout << "  📄 " << name << "\n";
    }
}

void Vfs::printTree() const { printTreeRec(root_, 0); }

std::vector<Vfs::NodePtr> Vfs::findNodesByName(const std::string& name) const {
    std::vector<NodePtr> out;
    auto opt = nameIndex_.find(name);
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

    NodePtr target = parent->getChild(leaf, true);
    if (!target) {
        target = std::make_shared<FSNode>(leaf, true);
        target->parent = parent;
        parent->setChild(target);
        initNodeProps(target);
        indexInsert(target);
        touchNode(parent);
    }

    auto json = JsonIO::treeToJson(root_);
    target->content.assignText(json);
    touchNode(target);
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
    if (!n->isFile) {
        for (auto& [_, bucket] : n->children) {
            if (bucket.dir)  printTreeRec(bucket.dir, depth+1);
            if (bucket.file) printTreeRec(bucket.file, depth+1);
        }
    }
}

bool Vfs::isSubtreeOf(const std::shared_ptr<FSNode>& a, const std::shared_ptr<FSNode>& b) {
    auto cur = a;
    while (cur) { if (cur==b) return true; cur = cur->parent.lock(); }
    return false;
}

std::string Vfs::makeUniqueName(const NodePtr& parent, const std::string& base, bool isFile) const {
    if (!parent) throw VfsException(ErrorCode::PathError);
    if (!parent->hasChild(base, isFile)) return base;

    auto dotPos = base.find_last_of('.');
    std::string stem = base;
    std::string ext;
    if (isFile && dotPos != std::string::npos && dotPos != 0) {
        stem = base.substr(0, dotPos);
        ext = base.substr(dotPos);
    } else {
        ext.clear();
    }

    for (int idx = 1;; ++idx) {
        auto candidate = stem + "(" + std::to_string(idx) + ")" + ext;
        if (!parent->hasChild(candidate, isFile))
            return candidate;
    }
}

Vfs::NodePtr Vfs::copyNodeRec(const NodePtr& src, const NodePtr& destParent, const std::string& name) {
    auto clone = std::make_shared<FSNode>(name, src->isFile);
    clone->parent = destParent;
    if (clone->isFile) {
        clone->content.replaceAll(src->content.bytes());
    }
    destParent->setChild(clone);
    touchNode(destParent);
    initNodeProps(clone);
    indexInsert(clone);
    if (!clone->isFile) {
        forEachChild(src, [&](const NodePtr& child) {
            copyNodeRec(child, clone, child->name);
        });
    }
    return clone;
}

void Vfs::compressNode(const NodePtr& node) {
    if (!node) return;
    if (node->isFile) {
        compressInplace(node->content, CompAlgo::LZW);
        touchNode(node);
        return;
    }
    forEachChild(node, [&](const NodePtr& child){ compressNode(child); });
}

void Vfs::decompressNode(const NodePtr& node) {
    if (!node) return;
    if (node->isFile) {
        if (isCompressed(node->content)) {
            uncompressInplace(node->content);
            touchNode(node);
        }
        return;
    }
    forEachChild(node, [&](const NodePtr& child){ decompressNode(child); });
}

void Vfs::initNodeProps(const NodePtr& node) {
    if (!node) return;
    auto now = std::time(nullptr);
    node->fileProps.createdAt = now;
    node->fileProps.modifiedAt = now;
    if (node->isFile) {
        node->fileProps.byteSize = node->content.size();
        node->fileProps.charCount = node->content.asText().size();
    } else {
        node->fileProps.byteSize = 0;
        node->fileProps.charCount = 0;
    }
}

void Vfs::touchNode(const NodePtr& node) {
    if (!node) return;
    auto now = std::time(nullptr);
    if (node->fileProps.createdAt == 0) node->fileProps.createdAt = now;
    node->fileProps.modifiedAt = now;
    if (node->isFile) {
        node->fileProps.byteSize = node->content.size();
        node->fileProps.charCount = node->content.asText().size();
    } else {
        node->fileProps.byteSize = 0;
        node->fileProps.charCount = 0;
    }
}

void Vfs::indexInsert(const std::shared_ptr<FSNode>& n) {
    if (!n) return;
    auto bucketOpt = nameIndex_.find(n->name);
    if (bucketOpt) {
        auto bucket = *bucketOpt;
        if (bucket) bucket->push_back(std::weak_ptr<FSNode>(n));
        return;
    }
    auto bucket = std::make_shared<std::vector<WNodePtr>>();
    bucket->push_back(std::weak_ptr<FSNode>(n));
    nameIndex_.insert(n->name, bucket);
}

void Vfs::indexErase(const std::shared_ptr<FSNode>& n) {
    if (!n) return;
    auto bucketOpt = nameIndex_.find(n->name);
    if (!bucketOpt) return;
    auto bucket = *bucketOpt;
    if (!bucket) return;
    auto& vec = *bucket;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const WNodePtr& w){
        auto sp = w.lock();
        return !sp || sp == n;
    }), vec.end());
    if (vec.empty()) nameIndex_.erase(n->name);
}

void Vfs::indexEraseSubtree(const std::shared_ptr<FSNode>& n) {
    if (!n) return;
    indexErase(n);
    if (!n->isFile) {
        forEachChild(n, [&](const NodePtr& child){ indexEraseSubtree(child); });
    }
}

std::string Vfs::readFile(const std::string& path) const {
    auto f = resolve(path, ResolveKind::File);
    if (!f) {
        auto alt = resolve(path, ResolveKind::Any);
        if (alt && !alt->isFile) throw VfsException(ErrorCode::InvalidArg);
        throw VfsException(ErrorCode::PathError);
    }
    return f->content.asText();
}

void Vfs::writeFile(const std::string& path, const std::string& content, bool append) {
    auto f = resolve(path, ResolveKind::File);
    if (!f) {
        auto alt = resolve(path, ResolveKind::Any);
        if (alt && !alt->isFile) throw VfsException(ErrorCode::InvalidArg);
        throw VfsException(ErrorCode::PathError);
    }
    if (append) f->content.append(std::vector<uint8_t>(content.begin(), content.end()));
    else        f->content.assignText(content);
    touchNode(f);
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

void Vfs::refreshNodeStats(const NodePtr& node) {
    touchNode(node);
}
