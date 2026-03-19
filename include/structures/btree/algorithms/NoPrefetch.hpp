#pragma once
#include <vector>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class NoPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;

    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());
        
        // 1. 获取根节点 (之前已公开)
        auto root = btree.get_root();
        if (!root) return found;

        // 2. 串行查找 - 使用官方原版逻辑
        for (size_t i = 0; i < queries.size(); ++i) {
            const Key& key = queries[i];
            const typename BTree::node* curr = root;

            // 逐层下潜
            while (!curr->isleafnode()) {
                const typename BTree::inner_node* inner = static_cast<const typename BTree::inner_node*>(curr);
                
                // 【关键修改】直接调用 btree 实例的 find_lower
                // 这会自动根据 SlotSize 选择最优的查找算法 (顺序 vs 二分)
                int slot = btree.find_lower(inner, key);
                
                curr = inner->childid[slot];
            }

            // 叶子节点
            const typename BTree::leaf_node* leaf = static_cast<const typename BTree::leaf_node*>(curr);
            
            // 【关键修改】同样调用官方 find_lower
            int slot = btree.find_lower(leaf, key);

            if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                 if (!BTree::traits::selfverify) results[i] = leaf->slotdata[slot];
                 found[i] = true;
            } else {
                 found[i] = false;
            }
        }
        return found;
    }

    static std::string name() { return "No Prefetch"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures