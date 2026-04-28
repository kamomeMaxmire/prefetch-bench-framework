#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

// Vectorized Search 
template<typename BTree>
class VectorizedSearch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    static constexpr int calc_steps(int max_slots) {
        int steps = 0, capacity = 1;
        while (capacity <= max_slots) {
            capacity *= 2;
            steps++;
        }
        return steps;
    }
    
    static constexpr int INNER_STEPS = calc_steps(BTree::innerslotmax);
    static constexpr int LEAF_STEPS  = calc_steps(BTree::leafslotmax);

    template <typename NodeType>
    static inline int find_lower(const NodeType* n, const Key& key) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            bool cmp = key > n->slotkey[mid];
            lo = cmp ? mid + 1 : lo;
            hi = cmp ? hi : mid;
        }
        return lo;
    }

    template<size_t VECTOR_SIZE = 64>
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        auto root = btree.get_root();
        if (!root || queries.empty()) return found;

        const size_t total_queries = queries.size();
        
        const Node* curr_nodes[VECTOR_SIZE];
        int lo[VECTOR_SIZE];
        int hi[VECTOR_SIZE];

        for (size_t batch_start = 0; batch_start < total_queries; batch_start += VECTOR_SIZE) {
            
            size_t current_batch_size = std::min(VECTOR_SIZE, total_queries - batch_start);
            
            // 提取裸指针，保证内存访问绝对连续，帮助编译器生成最紧凑的加载指令
            const Key* batch_queries = queries.data() + batch_start;

            // 1. init vector with root pointers
            for (size_t i = 0; i < current_batch_size; ++i) {
                curr_nodes[i] = root;
            }

            // 2. traverse inner nodes
            while (!curr_nodes[0]->isleafnode()) {
                for (size_t i = 0; i < current_batch_size; ++i) {
                    lo[i] = 0;
                    hi[i] = curr_nodes[i]->slotuse;
                }
                for (int step = 0; step < INNER_STEPS; ++step) {
                    for (size_t i = 0; i < current_batch_size; ++i) {
                        if (lo[i] < hi[i]) {
                            const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]);
                            int mid = (lo[i] + hi[i]) >> 1;
                            bool cmp = batch_queries[i] > inner->slotkey[mid];
                            lo[i] = cmp ? mid + 1 : lo[i];
                            hi[i] = cmp ? hi[i] : mid;
                        }
                    }
                }
                for (size_t i = 0; i < current_batch_size; ++i) {
                    const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]);
                    curr_nodes[i] = inner->childid[lo[i]]; 
                }
            }

            // 3. handle leaf nodes and gather results
            for (size_t i = 0; i < current_batch_size; ++i) {
                lo[i] = 0;
                hi[i] = curr_nodes[i]->slotuse;
            }
            for (int step = 0; step < LEAF_STEPS; ++step) {
                for (size_t i = 0; i < current_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                        int mid = (lo[i] + hi[i]) >> 1;
                        bool cmp = batch_queries[i] > leaf->slotkey[mid];
                        lo[i] = cmp ? mid + 1 : lo[i];
                        hi[i] = cmp ? hi[i] : mid;
                    }
                }
            }
            for (size_t i = 0; i < current_batch_size; ++i) {
                const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                int slot = lo[i];
                Key key = batch_queries[i];
                
                if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                    if (!BTree::traits::selfverify) 
                        results[batch_start + i] = leaf->slotdata[slot];
                    found[batch_start + i] = true;
                } else {
                    found[batch_start + i] = false;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "Vectorized";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures