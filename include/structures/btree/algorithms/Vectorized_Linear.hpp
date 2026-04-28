#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

// Vectorized Search (Linear)
template<typename BTree>
class VectorizedSearch_Linear {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

private:
    // find_lower（二分）→ linear_lower_bound（线性）
    template <typename NodeType>
    static inline int linear_lower_bound(const NodeType* n, const Key& key) {
        for (int s = 0; s < n->slotuse; ++s) {
            if (key <= n->slotkey[s]) return s;
        }
        return n->slotuse;
    }

public:
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

        for (size_t batch_start = 0; batch_start < total_queries; batch_start += VECTOR_SIZE) {
            
            size_t current_batch_size = std::min(VECTOR_SIZE, total_queries - batch_start);

            // 1. init vector with root pointers
            for (size_t i = 0; i < current_batch_size; ++i) {
                curr_nodes[i] = root;
            }

            // 2. traverse inner nodes
            while (!curr_nodes[0]->isleafnode()) {
                for (size_t i = 0; i < current_batch_size; ++i) {
                    const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]); 
                    Key key = queries[batch_start + i]; 
                    
                    int slot = linear_lower_bound(inner, key);
                    curr_nodes[i] = inner->childid[slot]; 
                }
            }

            // 3. handle leaf nodes and gather results（仅替换find_lower → linear_lower_bound）
            for (size_t i = 0; i < current_batch_size; ++i) {
                const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                Key key = queries[batch_start + i];
                
                int slot = linear_lower_bound(leaf, key); 
                
                if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                    if (!BTree::traits::selfverify) 
                        results[batch_start + i] = leaf->slotkey[slot];
                    found[batch_start + i] = true;
                } else {
                    found[batch_start + i] = false;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "Vectorized (Linear)";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures