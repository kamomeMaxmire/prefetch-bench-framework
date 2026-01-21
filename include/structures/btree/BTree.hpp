#pragma once

#include <stx/btree_map.h>
#include <vector>
#include <algorithm>
#include <string>

// 保持项目结构引用，但逻辑主要在 BTreeWithPrefetch 中直接实现
#include "algorithms/NoPrefetch.hpp"
// #include "algorithms/GroupPrefetch.hpp" // 可以暂时注释掉，或者保留以防其他地方用到
// #include "algorithms/SPP.hpp" // 暂时注释掉 SPP

namespace structures {
namespace btree {

// 定义默认 B+ 树类型
template<typename Key = uint64_t, typename Value = uint64_t>
using BTreeDefault = stx::btree<Key, Value>;

/**
 * 自定义配置的 B+ 树
 */
template<typename Key = uint64_t, typename Value = uint64_t, 
         int LeafSlots = 64, int InnerSlots = 64>
struct CustomBTree {
    struct traits {
        static const bool selfverify = false;
        static const bool debug = false;
        static const int leafslots = LeafSlots;
        static const int innerslots = InnerSlots;
        static const size_t binsearch_threshold = 256;
    };
    
    using type = stx::btree<Key, Value, std::pair<Key, Value>, std::less<Key>, traits>;
};

/**
 * B+ 树包装器 - 适配 Benchmark
 */
template<typename BTreeType>
class BTreeWithPrefetch {
private:
    BTreeType tree_;
    
public:
    using key_type = typename BTreeType::key_type;
    using mapped_type = typename BTreeType::data_type;
    using value_type = typename BTreeType::value_type;
    
    // ===== 基础操作 =====
    
    void insert(const key_type& key, const mapped_type& value) {
        tree_.insert(key, value);
    }
    
    void bulk_load(typename std::vector<std::pair<key_type, mapped_type>>::iterator begin, 
                   typename std::vector<std::pair<key_type, mapped_type>>::iterator end) {
        tree_.bulk_load(begin, end);
    }
    
    typename BTreeType::tree_stats get_stats() const {
        return tree_.get_stats();
    }
    
    // 获取底层树引用（用于调试或高级操作）
    BTreeType& get_tree() { return tree_; }

    // ===== 策略 1: 无预取 (Baseline) =====
    
    std::vector<bool> batch_lookup_no_prefetch(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        // 直接使用 NoPrefetch 算法或简单的循环
        return algorithms::NoPrefetch<BTreeType>::batch_lookup(tree_, queries, results);
    }
    
    // ===== 策略 2: Group Prefetch (直接调用底层优化实现) =====
    
    template<size_t GROUP_SIZE = 32>
    std::vector<bool> batch_lookup_group_prefetch(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        std::vector<bool> found;
        // 【关键】直接调用 stx::btree 中新添加的 find_group 函数
        // 这利用了你刚刚添加的内部流水线逻辑
        tree_.template find_group<GROUP_SIZE>(queries, results, found);
        return found;
    }
};

} // namespace btree
} // namespace structures