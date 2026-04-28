#pragma once
#include <vector>
#include <deque>

namespace structures {
namespace btree {
namespace algorithms {

// Static Pipelined Batching (H*D) - Linear Search Version
template<typename BTree>
class SoftwarePipelinedPrefetch_Linear {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    static int get_tree_height(BTree& btree) {
        auto root = btree.get_root();
        if (!root) return 0;
        int h = 1;
        const Node* curr = root;
        while (!curr->isleafnode()) {
            curr = static_cast<const InnerNode*>(curr)->childid[0];
            h++;
        }
        return h;
    }

    template <typename NodeType>
    static inline int linear_lower_bound(const NodeType* n, const Key& key) {
        for (int s = 0; s < n->slotuse; ++s) {
            if (key <= n->slotkey[s]) return s;
        }
        return n->slotuse;
    }

    template<size_t GROUP_SIZE = 4> // D = GROUP_SIZE
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        auto root = btree.get_root();
        if (!root || queries.empty()) return found;

        const int tree_height = get_tree_height(btree);
        const int pipeline_depth = tree_height; 
        
        struct Task {
            size_t query_idx;
            const Node* node;
            bool active;
        };

        std::vector<std::vector<Task>> batches(pipeline_depth);
        
        for(int i=0; i<pipeline_depth; ++i) {
            batches[i].resize(GROUP_SIZE, {0, nullptr, false});
        }

        size_t next_query_idx = 0;
        size_t finished_count = 0;
        size_t total_queries = queries.size();

        while (finished_count < total_queries) {

            // Stage A: 最后阶段处理（叶子节点）
            auto& last_batch = batches[pipeline_depth - 1];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (last_batch[i].active) {
                    const LeafNode* leaf = static_cast<const LeafNode*>(last_batch[i].node);
                    Key key = queries[last_batch[i].query_idx];
                    
                    //linear_lower_bound
                    int slot = linear_lower_bound(leaf, key);
                    
                    if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                        if (!BTree::traits::selfverify) 
                            results[last_batch[i].query_idx] = leaf->slotdata[slot];
                        found[last_batch[i].query_idx] = true;
                    } else {
                        found[last_batch[i].query_idx] = false;
                    }
                    
                    finished_count++;
                    last_batch[i].active = false; 
                }
            }

            // Stage B: Shift（流水线移位）
            for (int stage = pipeline_depth - 2; stage >= 0; --stage) {
                auto& curr_batch = batches[stage];
                auto& next_batch = batches[stage + 1];

                for (size_t i = 0; i < GROUP_SIZE; ++i) {
                    if (curr_batch[i].active) {
                        const InnerNode* inner = static_cast<const InnerNode*>(curr_batch[i].node);
                        Key key = queries[curr_batch[i].query_idx];
                        
                        int slot = linear_lower_bound(inner, key);
                        const Node* child = inner->childid[slot];

                        next_batch[i].query_idx = curr_batch[i].query_idx;
                        next_batch[i].node = child;
                        next_batch[i].active = true;

                        __builtin_prefetch(child, 0, 3);
                        __builtin_prefetch((const char*)child + 64, 0, 3);
                        
                        curr_batch[i].active = false; 
                    } else {
                        next_batch[i].active = false;
                    }
                }
            }

            // Stage C: Refill（填充新query）
            auto& first_batch = batches[0];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (next_query_idx < total_queries) {
                    first_batch[i].query_idx = next_query_idx++;
                    first_batch[i].node = root;
                    first_batch[i].active = true;
                    __builtin_prefetch(root, 0, 3);
                } else {
                    first_batch[i].active = false;
                }
            }
        }
        return found;
    }

    static std::string name() {
        return "Static SPP (Linear, H*D)";
    }
    
    static std::vector<bool> batch_lookup_d4(BTree& t, const std::vector<Key>& q, std::vector<Value>& r) { return batch_lookup<4>(t, q, r); }
};

} // namespace algorithms
} // namespace btree
} // namespace structures