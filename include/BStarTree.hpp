#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * B*-дерево с B+-семантикой:
 *  - Значения храним ТОЛЬКО в листьях.
 *  - Внутренние узлы содержат ключи-разделители и детей.
 *  - Вставка: redistribute (обе стороны >= ceil(2M/3)), иначе triple split, иначе обычный split.
 *  - Удаление: заём у соседей, иначе слияние; починка вверх до корня.
 *
 * Параметр M — максимально допустимое число ключей в узле.
 * Минимальное заполнение (для не-корня): minFill = ceil(2M/3).
 */
template <typename K, typename V, typename Less = std::less<K>>
class BStarTree {
public:
    explicit BStarTree(std::size_t maxKeys = 7, Less cmp = Less{})
        : M_(maxKeys ? maxKeys : 7), less_(std::move(cmp)) {
        if (M_ < 3) throw std::invalid_argument("BStarTree: M must be >= 3");
        root_ = std::make_shared<Node>(true);
    }

    // Найти значение по ключу.
    [[nodiscard("проверьте результат: может быть std::nullopt")]]
    std::optional<V> find(const K& key) const {
        auto n = root_;
        while (n) {
            auto it = lowerBound(n->keys, key);
            std::size_t idx = static_cast<std::size_t>(it - n->keys.begin());
            if (n->leaf) {
                if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key))
                    return n->values[idx];
                return std::nullopt;
            } else {
                if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key)) ++idx; // равные направо
                n = n->children[idx];
            }
        }
        return std::nullopt;
    }

    bool contains(const K& key) const { return find(key).has_value(); }

    // Вставка/обновление
    void insert(const K& key, const V& val) {
        if (root_->keys.size() >= M_) {
            auto newRoot = std::make_shared<Node>(false);
            newRoot->children.push_back(root_);
            splitOrRebalanceChild(newRoot, 0);
            root_ = newRoot;
        }
        insertNonFull(root_, key, val);
    }

    // Удаление: true если ключ существовал и был удалён
    bool erase(const K& key) {
        bool ok = eraseImpl(root_, key);
        // схлопнуть корень
        if (!root_->leaf && root_->keys.empty() && !root_->children.empty())
            root_ = root_->children.front();
        return ok;
    }

    void clear() { root_ = std::make_shared<Node>(true); }

    // Кол-во ключей (только в листьях)
    std::size_t size() const {
        std::size_t s = 0;
        inorderLeaves([&](const Node* leaf){
            s += leaf->keys.size();
        });
        return s;
    }

    // Проверка инвариантов. Бросает std::logic_error при нарушении.
    void validate() const {
        if (!root_) throw std::logic_error("root is null");
        validateNode(root_.get(), /*isRoot*/ true);
        // Проверка разделителей родителя == первый ключ правого ребёнка (для B+)
        validateSeparators(root_.get());
    }

private:
    struct Node {
        bool leaf;
        std::vector<K> keys;                                   // <= M_
        std::vector<std::shared_ptr<Node>> children;           // !leaf: size = keys+1
        std::vector<V> values;                                 // leaf: size = keys
        explicit Node(bool isLeaf) : leaf(isLeaf) {}
    };

    std::shared_ptr<Node> root_;
    std::size_t M_;
    Less less_;

    static std::size_t ceil2div3(std::size_t x) { return (2 * x + 2) / 3; } // ceil(2x/3)
    std::size_t minFill() const { return ceil2div3(M_); }

    // ----- utils -----
    typename std::vector<K>::const_iterator lowerBound(const std::vector<K>& vec, const K& key) const {
        return std::lower_bound(vec.begin(), vec.end(), key, less_);
    }
    typename std::vector<K>::iterator lowerBound(std::vector<K>& vec, const K& key) {
        return std::lower_bound(vec.begin(), vec.end(), key, less_);
    }

    // ----- insert path -----
    void insertNonFull(const std::shared_ptr<Node>& n, const K& key, const V& val) {
        if (n->leaf) {
            auto it = lowerBound(n->keys, key);
            std::size_t idx = static_cast<std::size_t>(it - n->keys.begin());
            if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key)) {
                // update
                n->values[idx] = val;
                return;
            }
            n->keys.insert(it, key);
            n->values.insert(n->values.begin() + static_cast<std::ptrdiff_t>(idx), val);
            return;
        }

        auto it = lowerBound(n->keys, key);
        std::size_t ci = static_cast<std::size_t>(it - n->keys.begin());
        if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key)) ++ci;

        if (n->children[ci]->keys.size() >= M_) {
            splitOrRebalanceChild(n, ci);
            // выбрать ветку снова (ключи могли поменяться)
            it = lowerBound(n->keys, key);
            ci = static_cast<std::size_t>(it - n->keys.begin());
            if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key)) ++ci;
        }
        insertNonFull(n->children[ci], key, val);
    }

    void splitOrRebalanceChild(const std::shared_ptr<Node>& parent, std::size_t idx) {
        auto child = parent->children[idx];

        if (!child->leaf) {
            if (idx + 1 < parent->children.size() &&
                tryRedistributeInternal(parent, idx, /*right=*/true)) return;
            if (idx > 0 &&
                tryRedistributeInternal(parent, idx, /*right=*/false)) return;

            // triple split с правым, иначе с левым
            if (idx + 1 < parent->children.size() &&
                tryTripleSplitInternal(parent, idx, idx + 1)) return;
            if (idx > 0 &&
                tryTripleSplitInternal(parent, idx - 1, idx)) return;

                
            splitInternal(parent, idx);
            return;
        }

        if (idx + 1 < parent->children.size() && tryRedistributeLeaves(parent, idx, /*useRight=*/true)) return;
        if (idx > 0 && tryRedistributeLeaves(parent, idx - 1, /*useRight=*/false)) return;

        if (idx + 1 < parent->children.size() &&
            tryTripleSplitLeaves(parent, idx, idx + 1)) return;
        if (idx > 0 &&
            tryTripleSplitLeaves(parent, idx - 1, idx)) return;

        splitLeaf(parent, idx);
    }

    bool tryRedistributeLeaves(const std::shared_ptr<Node>& parent, std::size_t leftIdx, bool useRight) {
        auto left  = parent->children[leftIdx];
        auto right = parent->children[leftIdx + 1];
        if (!(left->leaf && right->leaf)) return false;

        const std::size_t total = left->keys.size() + right->keys.size();
        const std::size_t minf  = minFill();
        if (total < 2 * minf) return false;
        
        std::size_t targetL = total / 2;
        if (targetL < minf) targetL = minf;
        if (targetL > total - minf) targetL = total - minf;

        if (left->keys.size() > targetL) {
            std::size_t move = left->keys.size() - targetL;
            right->keys.insert(right->keys.begin(),
                               left->keys.end() - static_cast<std::ptrdiff_t>(move),
                               left->keys.end());
            right->values.insert(right->values.begin(),
                                 left->values.end() - static_cast<std::ptrdiff_t>(move),
                                 left->values.end());
            left->keys.resize(left->keys.size() - move);
            left->values.resize(left->values.size() - move);
        } else if (left->keys.size() < targetL) {
            std::size_t move = targetL - left->keys.size();
            left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.begin() + static_cast<std::ptrdiff_t>(move));
            left->values.insert(left->values.end(), right->values.begin(), right->values.begin() + static_cast<std::ptrdiff_t>(move));
            right->keys.erase(right->keys.begin(), right->keys.begin() + static_cast<std::ptrdiff_t>(move));
            right->values.erase(right->values.begin(), right->values.begin() + static_cast<std::ptrdiff_t>(move));
        }

        // Обновляем разделитель
        parent->keys[leftIdx] = right->keys.front();
        return true;
    }

    
    bool tryTripleSplitLeaves(const std::shared_ptr<Node>& parent, std::size_t iLeft, std::size_t iRight) {
        auto A = parent->children[iLeft];
        auto B = parent->children[iRight];
        if (!(A->leaf && B->leaf)) return false;

        if (A->keys.size() < M_ || B->keys.size() < M_) return false;

        auto C = std::make_shared<Node>(true);

        std::vector<K> keys; keys.reserve(A->keys.size() + B->keys.size());
        std::vector<V> vals; vals.reserve(A->values.size() + B->values.size());
        keys.insert(keys.end(), A->keys.begin(), A->keys.end());
        keys.insert(keys.end(), B->keys.begin(), B->keys.end());
        vals.insert(vals.end(), A->values.begin(), A->values.end());
        vals.insert(vals.end(), B->values.begin(), B->values.end());

        const std::size_t t = keys.size();
        const std::size_t a = t / 3;
        const std::size_t b = (2 * t) / 3;

        A->keys.assign(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(a));
        B->keys.assign(keys.begin() + static_cast<std::ptrdiff_t>(a), keys.begin() + static_cast<std::ptrdiff_t>(b));
        C->keys.assign(keys.begin() + static_cast<std::ptrdiff_t>(b), keys.end());

        A->values.assign(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(a));
        B->values.assign(vals.begin() + static_cast<std::ptrdiff_t>(a), vals.begin() + static_cast<std::ptrdiff_t>(b));
        C->values.assign(vals.begin() + static_cast<std::ptrdiff_t>(b), vals.end());

        // Обновить разделител
        parent->keys[iLeft] = B->keys.front();
        parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(iRight), C->keys.front());
        parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(iRight + 1), C);
        return true;
    }

    void splitLeaf(const std::shared_ptr<Node>& parent, std::size_t idx) {
        auto y = parent->children[idx];
        auto z = std::make_shared<Node>(true);
        const std::size_t mid = y->keys.size() / 2;

        z->keys.assign(y->keys.begin() + static_cast<std::ptrdiff_t>(mid), y->keys.end());
        z->values.assign(y->values.begin() + static_cast<std::ptrdiff_t>(mid), y->values.end());
        y->keys.resize(mid);
        y->values.resize(mid);

        parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(idx), z->keys.front());
        parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(idx + 1), z);
    }

    bool tryRedistributeInternal(const std::shared_ptr<Node>& parent, std::size_t idx, bool right) {
        std::size_t edge = right ? idx : idx - 1;
        auto L = parent->children[edge];
        auto R = parent->children[edge + 1];
        if (L->leaf || R->leaf) return false;

        std::vector<K>   allKeys;
        std::vector<std::shared_ptr<Node>> allCh;

        allCh.reserve(L->children.size() + R->children.size());
        allKeys.reserve(L->keys.size() + R->keys.size() + 1);

        // L
        allCh.insert(allCh.end(), L->children.begin(), L->children.end());
        allKeys.insert(allKeys.end(), L->keys.begin(), L->keys.end());

        allKeys.push_back(parent->keys[edge]);
        // R
        allCh.insert(allCh.end(), R->children.begin(), R->children.end());
        allKeys.insert(allKeys.end(), R->keys.begin(), R->keys.end());

        const std::size_t totalKeys = allKeys.size();
        const std::size_t minf      = minFill();
        if (totalKeys < 2 * minf) return false;

        std::size_t leftKeys = totalKeys / 2;
        if (leftKeys < minf) leftKeys = minf;
        if (leftKeys > totalKeys - minf) leftKeys = totalKeys - minf;

        L->keys.assign(allKeys.begin(), allKeys.begin() + static_cast<std::ptrdiff_t>(leftKeys - 1));
        R->keys.assign(allKeys.begin() + static_cast<std::ptrdiff_t>(leftKeys), allKeys.end());
        parent->keys[edge] = allKeys[leftKeys - 1];

        L->children.assign(allCh.begin(), allCh.begin() + static_cast<std::ptrdiff_t>(leftKeys));
        R->children.assign(allCh.begin() + static_cast<std::ptrdiff_t>(leftKeys), allCh.end());

        return true;
    }

    bool tryTripleSplitInternal(const std::shared_ptr<Node>& parent, std::size_t iLeft, std::size_t iRight) {
        auto A = parent->children[iLeft];
        auto B = parent->children[iRight];
        if (A->leaf || B->leaf) return false;
        if (A->keys.size() < M_ || B->keys.size() < M_) return false;

        std::vector<K> allKeys;
        std::vector<std::shared_ptr<Node>> allCh;
        allKeys.reserve(A->keys.size() + B->keys.size() + 1);
        allCh.reserve(A->children.size() + B->children.size());

        allCh.insert(allCh.end(), A->children.begin(), A->children.end());
        allKeys.insert(allKeys.end(), A->keys.begin(), A->keys.end());
        allKeys.push_back(parent->keys[iLeft]);
        allCh.insert(allCh.end(), B->children.begin(), B->children.end());
        allKeys.insert(allKeys.end(), B->keys.begin(), B->keys.end());

        auto C = std::make_shared<Node>(false);

        const std::size_t t  = allKeys.size();
        const std::size_t aK = t / 3;
        const std::size_t bK = (2 * t) / 3;

        // A'
        A->keys.assign(allKeys.begin(), allKeys.begin() + static_cast<std::ptrdiff_t>(aK - 1));
        A->children.assign(allCh.begin(), allCh.begin() + static_cast<std::ptrdiff_t>(aK));
        // B' (середина)
        K up1 = allKeys[aK - 1];
        K up2 = allKeys[bK - 1];

        B->keys.assign(allKeys.begin() + static_cast<std::ptrdiff_t>(aK), allKeys.begin() + static_cast<std::ptrdiff_t>(bK - 1));
        B->children.assign(allCh.begin() + static_cast<std::ptrdiff_t>(aK), allCh.begin() + static_cast<std::ptrdiff_t>(bK));
        // C'
        C->keys.assign(allKeys.begin() + static_cast<std::ptrdiff_t>(bK), allKeys.end());
        C->children.assign(allCh.begin() + static_cast<std::ptrdiff_t>(bK), allCh.end());

        // Обновляем parent
        parent->keys[iLeft] = up1;
        parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(iRight), up2);
        parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(iRight + 1), C);
        return true;
    }

    void splitInternal(const std::shared_ptr<Node>& parent, std::size_t idx) {
        auto y = parent->children[idx];
        auto z = std::make_shared<Node>(false);
        const std::size_t mid = y->keys.size() / 2;

        K upKey = y->keys[mid];

        z->keys.assign(y->keys.begin() + static_cast<std::ptrdiff_t>(mid + 1), y->keys.end());
        y->keys.resize(mid);

        z->children.assign(y->children.begin() + static_cast<std::ptrdiff_t>(mid + 1), y->children.end());
        y->children.resize(mid + 1);

        parent->keys.insert(parent->keys.begin() + static_cast<std::ptrdiff_t>(idx), upKey);
        parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(idx + 1), z);
    }

    bool eraseImpl(const std::shared_ptr<Node>& n, const K& key) {
        auto it = lowerBound(n->keys, key);
        std::size_t idx = static_cast<std::size_t>(it - n->keys.begin());

        if (n->leaf) {
            if (it == n->keys.end() || less_(key, *it) || less_(*it, key)) return false;
            // удалить из листа
            n->keys.erase(n->keys.begin() + static_cast<std::ptrdiff_t>(idx));
            n->values.erase(n->values.begin() + static_cast<std::ptrdiff_t>(idx));
            // починка вверх будет сделана вызывающим (через fixUnderflowFrom)
            fixUnderflowFrom(n);
            return true;
        }

        // спускаемся вправо при равенстве
        if (it != n->keys.end() && !less_(key, *it) && !less_(*it, key)) ++idx;
        bool res = eraseImpl(n->children[idx], key);

        // после удаления возможно обновление разделителя слева
        if (res) {
            if (idx > 0) {
                auto leftChild = n->children[idx];
                if (leftChild->leaf && !leftChild->keys.empty())
                    n->keys[idx - 1] = leftChild->keys.front();
            }
            fixUnderflowFrom(n);
        }
        return res;
    }

    void fixUnderflowFrom(const std::shared_ptr<Node>& node) {
        if (node == root_) return;

        std::vector<std::shared_ptr<Node>> path;
        std::vector<std::size_t> idxs;
        if (!findParentPath(root_, node.get(), path, idxs)) return;

        for (int level = static_cast<int>(path.size()) - 1; level >= 0; --level) {
            auto parent = path[level];
            std::size_t i = idxs[level];
            auto child = parent->children[i];

            const std::size_t need = (parent == root_) ? 0 : minFill();
            if (child->keys.size() >= need) continue;

            if (i > 0 && borrowFromLeft(parent, i)) {
                // разделитель обновится внутри
            }
            
            else if (i + 1 < parent->children.size() && borrowFromRight(parent, i)) {
                // ok
            }
            else if (i > 0 && mergeWithLeft(parent, i)) {
                // parent уменьшился на один ключ/ребёнка
            } else if (i + 1 < parent->children.size() && mergeWithRight(parent, i)) {
                // parent уменьшился
            }

            if (parent != root_ && parent->keys.size() < minFill()) {
                continue;
            } else {
                break;
            }
        }
    }

    bool borrowFromLeft(const std::shared_ptr<Node>& parent, std::size_t i) {
        auto child = parent->children[i];
        auto left  = parent->children[i - 1];
        const std::size_t need = (parent == root_) ? 0 : minFill();
        if (left->keys.size() <= need) return false;

        if (child->leaf) {
            child->keys.insert(child->keys.begin(), left->keys.back());
            child->values.insert(child->values.begin(), left->values.back());
            left->keys.pop_back();
            left->values.pop_back();
            parent->keys[i - 1] = child->keys.front();
        } else {
            child->keys.insert(child->keys.begin(), parent->keys[i - 1]);
            child->children.insert(child->children.begin(), left->children.back());
            parent->keys[i - 1] = left->keys.back();
            left->keys.pop_back();
            left->children.pop_back();
        }
        return true;
    }

    bool borrowFromRight(const std::shared_ptr<Node>& parent, std::size_t i) {
        auto child = parent->children[i];
        auto right = parent->children[i + 1];
        const std::size_t need = (parent == root_) ? 0 : minFill();
        if (right->keys.size() <= need) return false;

        if (child->leaf) {
            child->keys.push_back(right->keys.front());
            child->values.push_back(right->values.front());
            right->keys.erase(right->keys.begin());
            right->values.erase(right->values.begin());

            parent->keys[i] = right->keys.front();
        } else {

            child->keys.push_back(parent->keys[i]);
            child->children.push_back(right->children.front());
            parent->keys[i] = right->keys.front();
            right->keys.erase(right->keys.begin());
            right->children.erase(right->children.begin());
        }
        return true;
    }

    bool mergeWithLeft(const std::shared_ptr<Node>& parent, std::size_t i) {
        auto left  = parent->children[i - 1];
        auto child = parent->children[i];
        if (child->leaf != left->leaf) return false;

        if (child->leaf) {
            left->keys.insert(left->keys.end(), child->keys.begin(), child->keys.end());
            left->values.insert(left->values.end(), child->values.begin(), child->values.end());
            parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(i - 1));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            left->keys.push_back(parent->keys[i - 1]);
            left->keys.insert(left->keys.end(), child->keys.begin(), child->keys.end());
            left->children.insert(left->children.end(), child->children.begin(), child->children.end());
            parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(i - 1));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(i));
        }
        return true;
    }

    bool mergeWithRight(const std::shared_ptr<Node>& parent, std::size_t i) {
        auto child = parent->children[i];
        auto right = parent->children[i + 1];
        if (child->leaf != right->leaf) return false;

        if (child->leaf) {
            child->keys.insert(child->keys.end(), right->keys.begin(), right->keys.end());
            child->values.insert(child->values.end(), right->values.begin(), right->values.end());
            parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(i));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(i + 1));
        } else {
            child->keys.push_back(parent->keys[i]);
            child->keys.insert(child->keys.end(), right->keys.begin(), right->keys.end());
            child->children.insert(child->children.end(), right->children.begin(), right->children.end());
            parent->keys.erase(parent->keys.begin() + static_cast<std::ptrdiff_t>(i));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(i + 1));
        }
        return true;
    }

    bool findParentPath(const std::shared_ptr<Node>& cur,
                        const Node* target,
                        std::vector<std::shared_ptr<Node>>& parents,
                        std::vector<std::size_t>& idxs,
                        bool isRoot = true) const
    {
        if (cur.get() == target) return true;
        if (cur->leaf) return false;
        for (std::size_t i = 0; i < cur->children.size(); ++i) {
            parents.push_back(cur);
            idxs.push_back(i);
            if (findParentPath(cur->children[i], target, parents, idxs, false)) return true;
            parents.pop_back();
            idxs.pop_back();
        }
        return false;
    }

    void validateNode(const Node* n, bool isRoot) const {
        if (!n) throw std::logic_error("null node");
        if (!isRoot) {
            if (n->keys.size() > M_) throw std::logic_error("node overflow");
            const std::size_t need = minFill();
            if (n->keys.size() < need)
                throw std::logic_error("node underflow (< 2/3 full)");
        } else {
            if (n->keys.size() > M_) throw std::logic_error("root overflow");
        }

        if (n->leaf) {
            if (n->values.size() != n->keys.size())
                throw std::logic_error("leaf values size mismatch");
        } else {
            if (n->children.size() != n->keys.size() + 1)
                throw std::logic_error("internal arity mismatch");
            for (auto& ch : n->children) validateNode(ch.get(), /*isRoot*/ false);
        }
    }

    void inorderLeaves(auto visitor) const {
        std::vector<const Node*> st;
        st.push_back(root_.get());
        while (!st.empty()) {
            auto n = st.back(); st.pop_back();
            if (!n) continue;
            if (n->leaf) {
                visitor(n);
            } else {
                for (std::size_t i = 0; i < n->children.size(); ++i) {
                    st.push_back(n->children[n->children.size() - 1 - i].get());
                }
            }
        }
    }

    void validateSeparators(const Node* n) const {
        if (n->leaf) return;
        for (std::size_t i = 0; i + 1 < n->children.size(); ++i) {
            const Node* right = n->children[i + 1].get();
            if (right->keys.empty()) throw std::logic_error("empty right child");
            const K& sep = n->keys[i];
            const K& want = right->keys.front();
            if (less_(sep, want) || less_(want, sep)) {
                throw std::logic_error("separator != first key of right child");
            }
        }
        for (auto& ch : n->children) validateSeparators(ch.get());
    }
};
