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

    static constexpr int calc_steps(int max_slots) {
        int steps = 0, capacity = 1;
        while (capacity <= max_slots) {
            capacity *= 2;
            steps++;
        }
        return steps;
    }
    
    // 利用 B 树暴露的槽位常量，让编译器直接算好这两个步数
    static constexpr int INNER_STEPS = calc_steps(BTree::innerslotmax);
    static constexpr int LEAF_STEPS  = calc_steps(BTree::leafslotmax);
    // =========================================================

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

        // using declarations for node types
        using inner_node = typename BTree::inner_node;
        using leaf_node = typename BTree::leaf_node;
        using node = typename BTree::node;

        // group prefetching: process queries in batches of GROUP_SIZE
        const node* curr_nodes[GROUP_SIZE];
        int lo[GROUP_SIZE];
        int hi[GROUP_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += GROUP_SIZE) {
            size_t this_batch_size = std::min(GROUP_SIZE, queries.size() - batch_start);

            // --- Init & Prefetch Root ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                curr_nodes[i] = root;
                __builtin_prefetch(curr_nodes[i], 0, 1);
                __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 64, 0, 1);
                __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 128, 0, 1);
                __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 192, 0, 1);

            }

            // --- Inner Nodes ---
            while (!curr_nodes[0]->isleafnode()) {
                // A. Init Binary Search Range
                for (size_t i = 0; i < this_batch_size; ++i) {
                    lo[i] = 0; 
                    hi[i] = curr_nodes[i]->slotuse;
                }
                
                // B. Interleaved Binary Search (执行除最后一步外的所有步骤)
                for (int step = 0; step < INNER_STEPS - 1; ++step) {
                    for (size_t i = 0; i < this_batch_size; ++i) {
                        if (lo[i] < hi[i]) {
                            const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                            int mid = (lo[i] + hi[i]) >> 1;
                            if (queries[batch_start + i] <= inner->slotkey[mid]) hi[i] = mid;
                            else lo[i] = mid + 1;
                        }
                    }
                }

                // C. 将最后一步二分查找和子节点预取合并，消灭一次完整的 for 循环开销
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                        int mid = (lo[i] + hi[i]) >> 1;
                        if (queries[batch_start + i] <= inner->slotkey[mid]) hi[i] = mid;
                        else lo[i] = mid + 1;
                    }
                    const inner_node* inner = static_cast<const inner_node*>(curr_nodes[i]);
                    curr_nodes[i] = inner->childid[lo[i]];
                    __builtin_prefetch(curr_nodes[i], 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 64, 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 128, 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(curr_nodes[i]) + 192, 0, 1);
                }
            }

            // --- Leaf Nodes (Logic copied same as Inner) ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                lo[i] = 0; 
                hi[i] = curr_nodes[i]->slotuse;
            }
            for (int step = 0; step < LEAF_STEPS; ++step) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                        int mid = (lo[i] + hi[i]) >> 1;
                        if (queries[batch_start + i] <= leaf->slotkey[mid]) hi[i] = mid;
                        else lo[i] = mid + 1;
                    }
                }
            }

            // --- Collect Results ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                const leaf_node* leaf = static_cast<const leaf_node*>(curr_nodes[i]);
                int slot = lo[i];
                size_t idx = batch_start + i;
                
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