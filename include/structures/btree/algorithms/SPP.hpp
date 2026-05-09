#pragma once
#include <vector>
#include <deque>

namespace structures {
namespace btree {
namespace algorithms {

// Static Pipelined Batching (H*D) with Zero-copy Pointer Rotation
template<typename BTree>
class SoftwarePipelinedPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    // 动态获取当前 B+ 树的高度，以精确定义流水线阶段数
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

    template<size_t GROUP_SIZE = 64> // Default prefetch distance D
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        auto root = btree.get_root();
        if (!root || queries.empty()) return found;

        int H = get_tree_height(btree);
        if (H <= 0) return found;

        struct Task {
            size_t query_idx;
            const Node* node;
        };
        int num_stages = H+1;

        // 【性能黑魔法】增加 Padding，打破完美 2 的幂次对齐导致的 L1 Cache 组冲突 (Cache Aliasing)
        constexpr size_t PAD = 2; // 故意错开 32 字节
        size_t stride = GROUP_SIZE + PAD;
        
        // 分配 num_stages * stride 大小的连续内存，用作流水线各 Stage 的缓冲
        std::vector<Task> memory(num_stages * stride, {0, nullptr});
        // 指针数组，stages[s] 指向当前处于第 s 阶段的 Task 批次
        std::vector<Task*> stages(num_stages);
        for (int i = 0; i < num_stages; ++i) {
            stages[i] = &memory[i * stride];
        }

        size_t next_query_idx = 0;
        size_t finished_count = 0;
        size_t total_queries = queries.size();

        while (finished_count < total_queries) {

            // --- Stage H: Process Leaf Nodes ---
            Task* leaf_stage = stages[H];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (leaf_stage[i].node) {
                    const LeafNode* leaf = static_cast<const LeafNode*>(leaf_stage[i].node);
                    Key key = queries[leaf_stage[i].query_idx];
                    
                    int slot = find_lower(leaf, key);
                    
                    if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                        if (!BTree::traits::selfverify) 
                            results[leaf_stage[i].query_idx] = leaf->slotdata[slot];
                        found[leaf_stage[i].query_idx] = true;
                    } else {
                        found[leaf_stage[i].query_idx] = false;
                    }
                    
                    finished_count++;
                    leaf_stage[i].node = nullptr; // Mark as finished
                }
            }

            // --- Stage H-1 down to 1: Process Inner Nodes ---
            for (int s = H - 1; s >= 1; --s) {
                Task* inner_stage = stages[s];
                for (size_t i = 0; i < GROUP_SIZE; ++i) {
                    if (inner_stage[i].node) {
                        const InnerNode* inner = static_cast<const InnerNode*>(inner_stage[i].node);
                        Key key = queries[inner_stage[i].query_idx];
                        
                        int slot = find_lower(inner, key);
                        const Node* child = inner->childid[slot];

                        inner_stage[i].node = child;
                        
                        __builtin_prefetch(child, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(child) + 64, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(child) + 128, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(child) + 192, 0, 1);
                    }
                }
            }

            // --- Rotate Pipeline Stages (Zero-copy Shift) ---
            // stages[0] 变成 stages[1], stages[H-2] 变成 stages[H-1]
            // 原先的 stages[H] (已处理完空闲) 变成新的 stages[0]
            Task* empty_stage = stages[H];
            for (int s = H ; s > 0; --s) {
                stages[s] = stages[s - 1];
            }
            stages[0] = empty_stage;

            // --- Stage 0 Refill: Feed new queries into the pipeline ---
            Task* refill_stage = stages[0];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (next_query_idx < total_queries) {
                    refill_stage[i].query_idx = next_query_idx++;
                    refill_stage[i].node = root;
                    
                    __builtin_prefetch(root, 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(root) + 64, 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(root) + 128, 0, 1);
                    __builtin_prefetch(reinterpret_cast<const char*>(root) + 192, 0, 1);
                } else {
                    refill_stage[i].node = nullptr;
                }
            }
        }
        return found;
    }

    static std::string name() {
        return "SPP (H*D)";
    }
    
    static std::vector<bool> batch_lookup_d4(BTree& t, const std::vector<Key>& q, std::vector<Value>& r) { return batch_lookup<4>(t, q, r); }
};

} // namespace algorithms
} // namespace btree
} // namespace structures
