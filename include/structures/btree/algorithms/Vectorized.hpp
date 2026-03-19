#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

// =============================================================
// Vectorized Search (向量化查找)
// 核心：利用紧凑循环处理批量数据，依赖 CPU 的 OoO (乱序执行) 隐式隐藏延迟
// =============================================================
template<typename BTree>
class VectorizedSearch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    // 复用之前的模板二分查找（保证计算逻辑与其他方法一致）
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

    // ---------------------------------------------------------
    // 核心实现
    // ---------------------------------------------------------
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
        
        // 维护一个向量，保存当前批次每个查询到达的节点指针
        const Node* curr_nodes[VECTOR_SIZE];

        // 以外层 Batch 为单位推进
        for (size_t batch_start = 0; batch_start < total_queries; batch_start += VECTOR_SIZE) {
            
            // 处理尾部可能不满一个 VECTOR_SIZE 的情况
            size_t current_batch_size = std::min(VECTOR_SIZE, total_queries - batch_start);

            // 1. 初始化向量：所有任务都从 Root 开始
            for (size_t i = 0; i < current_batch_size; ++i) {
                curr_nodes[i] = root;
            }

            // 2. 向量化横向遍历树的中间层
            // B+ 树是平衡的，所有查询会同时到达叶子节点
            while (!curr_nodes[0]->isleafnode()) {
                // 纯粹通过紧凑的循环，让 CPU 乱序执行去隐式并行读取内存
                for (size_t i = 0; i < current_batch_size; ++i) {
                    const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]); 
                    Key key = queries[batch_start + i]; 
                    
                    int slot = find_lower(inner, key);
                    curr_nodes[i] = inner->childid[slot]; // 更新向量中的指针
                }
            }

            // 3. 向量化处理叶子层，收割结果
            for (size_t i = 0; i < current_batch_size; ++i) {
                const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                Key key = queries[batch_start + i];
                
                int slot = find_lower(leaf, key);
                
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