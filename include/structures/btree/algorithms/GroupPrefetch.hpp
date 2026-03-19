// include/structures/btree/algorithms/GroupPrefetch.hpp
#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class GroupPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;

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

        // 必须引用 BTree 内部定义的节点类型
        using inner_node = typename BTree::inner_node;
        using leaf_node = typename BTree::leaf_node;
        using node = typename BTree::node;

        // 状态数组
        const node* curr_nodes[GROUP_SIZE];
        size_t key_indices[GROUP_SIZE];
        int lo[GROUP_SIZE];
        int hi[GROUP_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += GROUP_SIZE) {
            size_t this_batch_size = std::min(GROUP_SIZE, queries.size() - batch_start);

            // --- Init & Prefetch Root ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                curr_nodes[i] = root;
                key_indices[i] = batch_start + i;
                __builtin_prefetch(curr_nodes[i], 0, 3);
            }

            // --- Inner Nodes ---
            while (!curr_nodes[0]->isleafnode()) {
                // A. Init Binary Search Range
                for (size_t i = 0; i < this_batch_size; ++i) {
                    lo[i] = 0; 
                    hi[i] = curr_nodes[i]->slotuse;
                }
                
                // B. Interleaved Binary Search
                for (int step = 0; step < 8; ++step) {
                    for (size_t i = 0; i < this_batch_size; ++i) {
                        if (lo[i] < hi[i]) {
                            const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                            int mid = (lo[i] + hi[i]) >> 1;
                            if (queries[key_indices[i]] <= inner->slotkey[mid]) hi[i] = mid;
                            else lo[i] = mid + 1;
                        }
                    }
                }

                // C. Advance & Prefetch
                for (size_t i = 0; i < this_batch_size; ++i) {
                    const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                    curr_nodes[i] = inner->childid[lo[i]];
                    __builtin_prefetch(curr_nodes[i], 0, 3);
                }
            }

            // --- Leaf Nodes (Logic copied same as Inner) ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                lo[i] = 0; 
                hi[i] = curr_nodes[i]->slotuse;
            }
            for (int step = 0; step < 8; ++step) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                        int mid = (lo[i] + hi[i]) >> 1;
                        if (queries[key_indices[i]] <= leaf->slotkey[mid]) hi[i] = mid;
                        else lo[i] = mid + 1;
                    }
                }
            }

            // --- Collect Results ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                int slot = lo[i];
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
    
    // Helper for name string...
};

} // namespace algorithms
} // namespace btree
} // namespace structures