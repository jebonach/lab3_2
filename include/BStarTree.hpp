#pragma once
#include <vector>
#include <memory>
#include <optional>
#include <algorithm>

template<typename K, typename V>
class BStarTree {
public:
    explicit BStarTree(size_t maxKeys = 7)
        : M_(maxKeys ? maxKeys : 7) {
        root_ = std::make_shared<Node>(true);
    }

    std::optional<V> find(const K& key) const {
        auto n = root_;
        while (n) {
            auto it = std::lower_bound(n->keys.begin(), n->keys.end(), key);
            size_t idx = size_t(it - n->keys.begin());
            if (n->leaf) {
                if (it != n->keys.end() && *it == key) return n->values[idx];
                return std::nullopt;
            } else {
                if (it != n->keys.end() && *it == key) ++idx;
                n = n->children[idx];
            }
        }
        return std::nullopt;
    }

    void insert(const K& key, const V& val) {
        if (root_->keys.size() >= M_) {
            auto newRoot = std::make_shared<Node>(false);
            newRoot->children.push_back(root_);
            splitOrRedistributeChild(newRoot, 0);
            root_ = newRoot;
        }
        insertNonFull(root_, key, val);
    }

    bool erase(const K& key) {
        bool ok = eraseImpl(root_, key, nullptr, 0);
        if (!root_->leaf && root_->keys.empty() && !root_->children.empty()) {
            root_ = root_->children[0];
        }
        return ok;
    }

private:
    struct Node {
        bool leaf;
        std::vector<K> keys;
        std::vector<V> values;
        std::vector<std::shared_ptr<Node>> children;
        explicit Node(bool isLeaf) : leaf(isLeaf) {}
    };

    std::shared_ptr<Node> root_;
    size_t M_;

    static size_t minFill(size_t M) { return (M * 2 + 2) / 3; } // ≈ 2/3

    void insertNonFull(const std::shared_ptr<Node>& n, const K& key, const V& val) {
        if (n->leaf) {
            auto it = std::lower_bound(n->keys.begin(), n->keys.end(), key);
            size_t idx = size_t(it - n->keys.begin());
            if (it != n->keys.end() && *it == key) { n->values[idx] = val; return; }
            n->keys.insert(it, key);
            n->values.insert(n->values.begin() + idx, val);
            return;
        }
        auto it = std::lower_bound(n->keys.begin(), n->keys.end(), key);
        size_t ci = size_t(it - n->keys.begin());
        if (it != n->keys.end() && *it == key) ++ci;
        if (n->children[ci]->keys.size() >= M_) {
            splitOrRedistributeChild(n, ci);

            it = std::lower_bound(n->keys.begin(), n->keys.end(), key);
            ci = size_t(it - n->keys.begin());
            if (it != n->keys.end() && *it == key) ++ci;
        }
        insertNonFull(n->children[ci], key, val);
    }

    void splitOrRedistributeChild(const std::shared_ptr<Node>& parent, size_t idx) {
        auto child = parent->children[idx];

        if (!child->leaf) {
            splitChild(parent, idx);
            return;
        }

        // лев
        if (idx + 1 < parent->children.size()) {
            auto right = parent->children[idx + 1];
            if (child->keys.size() >= M_ && right->keys.size() < M_) {
                redistribute(child, right, parent, idx);
                return;
            }
        }
        // прав
        if (idx > 0) {
            auto left = parent->children[idx - 1];
            if (child->keys.size() >= M_ && left->keys.size() < M_) {
                redistribute(left, child, parent, idx - 1);
                return;
            }
        }
        // 3 split
        if (idx + 1 < parent->children.size() && parent->children[idx + 1]->keys.size() >= M_) {
            tripleSplit(parent, idx, idx + 1);
            return;
        }
        if (idx > 0 && parent->children[idx - 1]->keys.size() >= M_) {
            tripleSplit(parent, idx - 1, idx);
            return;
        }
        // split
        splitChild(parent, idx);
    }

    void splitChild(const std::shared_ptr<Node>& parent, size_t idx) {
        auto y = parent->children[idx];
        auto z = std::make_shared<Node>(y->leaf);
        size_t mid = y->keys.size() / 2;

        if (y->leaf) {
            z->keys.assign(y->keys.begin() + mid, y->keys.end());
            z->values.assign(y->values.begin() + mid, y->values.end());
            y->keys.resize(mid);
            y->values.resize(mid);
            parent->keys.insert(parent->keys.begin() + idx, z->keys.front());
        } else {
            auto upKey = y->keys[mid];
            z->keys.assign(y->keys.begin() + mid + 1, y->keys.end());
            y->keys.resize(mid);
            z->children.assign(y->children.begin() + mid + 1, y->children.end());
            y->children.resize(mid + 1);
            parent->keys.insert(parent->keys.begin() + idx, upKey);
        }
        parent->children.insert(parent->children.begin() + idx + 1, z);
    }

    static void redistribute(const std::shared_ptr<Node>& left,
                             const std::shared_ptr<Node>& right,
                             const std::shared_ptr<Node>& parent,
                             size_t parentKeyIdx) {
        if (!(left->leaf && right->leaf)) return;

        const size_t total = left->keys.size() + right->keys.size();
        const size_t targetL = total / 2;

        if (left->keys.size() > targetL) {
            size_t move = left->keys.size() - targetL;
            right->keys.insert(right->keys.begin(),
                               left->keys.end() - move, left->keys.end());
            right->values.insert(right->values.begin(),
                                 left->values.end() - move, left->values.end());
            left->keys.resize(left->keys.size() - move);
            left->values.resize(left->values.size() - move);
        } else if (left->keys.size() < targetL) {
            size_t move = targetL - left->keys.size();
            left->keys.insert(left->keys.end(),
                              right->keys.begin(), right->keys.begin() + move);
            left->values.insert(left->values.end(),
                                right->values.begin(), right->values.begin() + move);
            right->keys.erase(right->keys.begin(), right->keys.begin() + move);
            right->values.erase(right->values.begin(), right->values.begin() + move);
        }

        parent->keys[parentKeyIdx] = right->keys.front();
    }

    void tripleSplit(const std::shared_ptr<Node>& parent, size_t iLeft, size_t iRight) {
        auto A = parent->children[iLeft];
        auto B = parent->children[iRight];
        if (!(A->leaf && B->leaf)) {
            splitChild(parent, iLeft);
            return;
        }

        auto C = std::make_shared<Node>(true);

        std::vector<K> keysAll; keysAll.reserve(A->keys.size() + B->keys.size());
        keysAll.insert(keysAll.end(), A->keys.begin(), A->keys.end());
        keysAll.insert(keysAll.end(), B->keys.begin(), B->keys.end());

        std::vector<V> valsAll; valsAll.reserve(A->values.size() + B->values.size());
        valsAll.insert(valsAll.end(), A->values.begin(), A->values.end());
        valsAll.insert(valsAll.end(), B->values.begin(), B->values.end());

        const size_t t = keysAll.size();
        const size_t a = t / 3;
        const size_t b = (t * 2) / 3;

        A->keys.assign(keysAll.begin(), keysAll.begin() + a);
        B->keys.assign(keysAll.begin() + a, keysAll.begin() + b);
        C->keys.assign(keysAll.begin() + b, keysAll.end());

        A->values.assign(valsAll.begin(), valsAll.begin() + a);
        B->values.assign(valsAll.begin() + a, valsAll.begin() + b);
        C->values.assign(valsAll.begin() + b, valsAll.end());

        parent->keys[iLeft] = B->keys.front();
        parent->keys.insert(parent->keys.begin() + iRight, C->keys.front());
        parent->children.insert(parent->children.begin() + iRight + 1, C);
    }

    bool eraseImpl(const std::shared_ptr<Node>& n, const K& key,
                   const std::shared_ptr<Node>& parent, size_t parentChildIdx) {
        auto it = std::lower_bound(n->keys.begin(), n->keys.end(), key);
        size_t idx = size_t(it - n->keys.begin());

        if (n->leaf) {
            if (it != n->keys.end() && *it == key) {
                n->keys.erase(it);
                n->values.erase(n->values.begin() + idx);

                if (n == root_) return true;

                if (n->keys.size() < minFill(M_) && parent) {
                    fixLeafUnderflow(parent, parentChildIdx);
                } else {
                    if (parent && parentChildIdx > 0 && !n->keys.empty()) {
                        parent->keys[parentChildIdx - 1] = n->keys.front();
                    }
                }
                return true;
            }
            return false;
        } else {
            if (it != n->keys.end() && *it == key) ++idx;
            bool res = eraseImpl(n->children[idx], key, n, idx);

            if (res && n->children[idx]->leaf && idx > 0 && !n->children[idx]->keys.empty()) {
                n->keys[idx - 1] = n->children[idx]->keys.front();
            }
            return res;
        }
    }

    void fixLeafUnderflow(const std::shared_ptr<Node>& parent, size_t idx) {
        auto child = parent->children[idx];
        if (!child->leaf) return;

        const size_t need = minFill(M_);
        // 1) Заём у левого
        if (idx > 0) {
            auto left = parent->children[idx - 1];
            if (left->leaf && left->keys.size() > need) {
                child->keys.insert(child->keys.begin(), left->keys.back());
                child->values.insert(child->values.begin(), left->values.back());
                left->keys.pop_back();
                left->values.pop_back();
                parent->keys[idx - 1] = child->keys.front();
                return;
            }
        }
        // 2) Заём у правого
        if (idx + 1 < parent->children.size()) {
            auto right = parent->children[idx + 1];
            if (right->leaf && right->keys.size() > need) {
                child->keys.push_back(right->keys.front());
                child->values.push_back(right->values.front());
                right->keys.erase(right->keys.begin());
                right->values.erase(right->values.begin());
                parent->keys[idx] = right->keys.front();
                return;
            }
        }
        // merge
        if (idx > 0) {
            auto left = parent->children[idx - 1];
            left->keys.insert(left->keys.end(), child->keys.begin(), child->keys.end());
            left->values.insert(left->values.end(), child->values.begin(), child->values.end());
            parent->children.erase(parent->children.begin() + idx);
            parent->keys.erase(parent->keys.begin() + (idx - 1));
            return;
        } else if (idx + 1 < parent->children.size()) {
            auto right = parent->children[idx + 1];
            child->keys.insert(child->keys.end(), right->keys.begin(), right->keys.end());
            child->values.insert(child->values.end(), right->values.begin(), right->values.end());
            parent->children.erase(parent->children.begin() + idx + 1);
            parent->keys.erase(parent->keys.begin() + idx);
            return;
        }
    }
};
