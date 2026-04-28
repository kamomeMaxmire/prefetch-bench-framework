#pragma once
#include <vector>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class NoPrefetch_Linear {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;

    template <typename NodeType>
    static inline int linear_search(const NodeType* n, const Key& key) {
        for (int s = 0; s < n->slotuse; ++s) {
            if (key <= n->slotkey[s]) return s;
        }
        return n->slotuse;
    }

    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());
        
        auto root = btree.get_root();
        if (!root) return found;

        for (size_t i = 0; i < queries.size(); ++i) {
            const Key& key = queries[i];
            const typename BTree::node* curr = root;

            while (!curr->isleafnode()) {
                const typename BTree::inner_node* inner =
                    static_cast<const typename BTree::inner_node*>(curr);

                int slot = linear_search(inner, key);

                curr = inner->childid[slot];
            }

            const typename BTree::leaf_node* leaf =
                static_cast<const typename BTree::leaf_node*>(curr);

            int slot = linear_search(leaf, key);

            if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                if (!BTree::traits::selfverify) results[i] = leaf->slotdata[slot];
                found[i] = true;
            } else {
                found[i] = false;
            }
        }
        return found;
    }

    static std::string name() { return "No Prefetch (Linear)"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures