#pragma once
#include <vector>
#include <deque>

namespace structures {
namespace btree {
namespace algorithms {

// Static Pipelined Batching (H*D)
template<typename BTree>
class SoftwarePipelinedPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    // tree height 
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
    static inline int find_lower(const NodeType* n, const Key& key) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (key <= n->slotkey[mid]) hi = mid;
            else lo = mid + 1;
        }
        return lo;
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

        // 1. determine tree height and set pipeline depth
        const int tree_height = get_tree_height(btree);
        const int pipeline_depth = tree_height; 
        
        // 2. define task structure for each pipeline stage
        struct Task {
            size_t query_idx;
            const Node* node;
            bool active;
        };

        // 3. construct pipeline batches
        std::vector<std::vector<Task>> batches(pipeline_depth);
        
        // init batches
        for(int i=0; i<pipeline_depth; ++i) {
            batches[i].resize(GROUP_SIZE, {0, nullptr, false});
        }

        size_t next_query_idx = 0;
        size_t finished_count = 0;
        size_t total_queries = queries.size();

        // 4. main loop
        while (finished_count < total_queries) {

            // Stage A: check if last stage has finished tasks and process them
            auto& last_batch = batches[pipeline_depth - 1];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (last_batch[i].active) {
                    const LeafNode* leaf = static_cast<const LeafNode*>(last_batch[i].node);
                    Key key = queries[last_batch[i].query_idx];
                    
                    int slot = find_lower(leaf, key);
                    
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

            // Stage B: Shift
            for (int stage = pipeline_depth - 2; stage >= 0; --stage) {
                auto& curr_batch = batches[stage];
                auto& next_batch = batches[stage + 1];

                for (size_t i = 0; i < GROUP_SIZE; ++i) {
                    if (curr_batch[i].active) {
                        const InnerNode* inner = static_cast<const InnerNode*>(curr_batch[i].node);
                        Key key = queries[curr_batch[i].query_idx];
                        
                        int slot = find_lower(inner, key);
                        const Node* child = inner->childid[slot];

                        next_batch[i].query_idx = curr_batch[i].query_idx;
                        next_batch[i].node = child;
                        next_batch[i].active = true;

                        // prefetch child node and its next cache line to reduce latency
                        __builtin_prefetch(child, 0, 3);
                        __builtin_prefetch((const char*)child + 64, 0, 3);
                        
                        curr_batch[i].active = false; 
                    } else {
                        next_batch[i].active = false;
                    }
                }
            }

            // Stage C: Refill
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
        return "Static SPP (H*D)";
    }
    
    static std::vector<bool> batch_lookup_d4(BTree& t, const std::vector<Key>& q, std::vector<Value>& r) { return batch_lookup<4>(t, q, r); }
};

} // namespace algorithms
} // namespace btree
} // namespace structures