#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class GroupPrefetch_Linear {
public:

    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;

    template <typename NodeType>
    static inline int linear_lower_bound(const NodeType* n, const Key& key) {
        for (int s = 0; s < n->slotuse; ++s) {
            if (key <= n->slotkey[s]) return s;
        }
        return n->slotuse;
    }

    template<size_t GROUP_SIZE = 32>
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());
        
        auto root = btree.get_root();
        if (!root || queries.empty()) return found;

        using inner_node = typename BTree::inner_node;
        using leaf_node = typename BTree::leaf_node;
        using node = typename BTree::node;

        const node* curr_nodes[GROUP_SIZE];
        size_t key_indices[GROUP_SIZE];
        int slots[GROUP_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += GROUP_SIZE) {
            size_t this_batch_size = std::min(GROUP_SIZE, queries.size() - batch_start);

            for (size_t i = 0; i < this_batch_size; ++i) {
                curr_nodes[i] = root;
                key_indices[i] = batch_start + i;
                __builtin_prefetch(curr_nodes[i], 0, 3);
            }

            // --- Inner Nodes（核心修改：交错二分 → 线性lower_bound）---
            while (!curr_nodes[0]->isleafnode()) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                    const Key& key = queries[key_indices[i]];
                    slots[i] = linear_lower_bound(inner, key);
                }

                for (size_t i = 0; i < this_batch_size; ++i) {
                    const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                    curr_nodes[i] = inner->childid[slots[i]];
                    __builtin_prefetch(curr_nodes[i], 0, 3);
                }
            }

            for (size_t i = 0; i < this_batch_size; ++i) {
                const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                const Key& key = queries[key_indices[i]];
                slots[i] = linear_lower_bound(leaf, key);
            }

            for (size_t i = 0; i < this_batch_size; ++i) {
                const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                int slot = slots[i];
                size_t idx = key_indices[i];
                
                if (slot < leaf->slotuse && queries[idx] == leaf->slotkey[slot]) {
                    results[idx] = leaf->slotdata[slot];
                    found[idx] = true;
                } else {
                    found[idx] = false;
                }
            }
        }
        return found;
    }
    
    static std::string name() { return "Group Prefetch (Linear)"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures