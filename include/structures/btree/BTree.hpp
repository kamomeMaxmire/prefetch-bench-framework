#pragma once

#include <stx/btree_map.h> // 引用底层的 stx::btree
#include <vector>
#include <algorithm>
#include <string>

// 引入所有外部实现的算法策略
#include "algorithms/NoPrefetch.hpp"
#include "algorithms/GroupPrefetch.hpp"
#include "algorithms/SPP.hpp" 
#include "algorithms/Vectorized.hpp"
#include "algorithms/AMAC3.hpp"

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
    BTreeType tree_; // 底层 stx::btree 实例
    
public:
    using key_type = typename BTreeType::key_type;
    using mapped_type = typename BTreeType::data_type;
    using value_type = typename BTreeType::value_type;
    
    // ===== 基础操作 (透传给底层树) =====
    
    void insert(const key_type& key, const mapped_type& value) {
        tree_.insert(key, value);
    }
    
    // [新代码] 支持 iterator 和 const_iterator
    template <typename Iterator>
    void bulk_load(Iterator begin, Iterator end) {
        tree_.bulk_load(begin, end);
    }
    
    typename BTreeType::tree_stats get_stats() const {
        return tree_.get_stats();
    }
    
    // 获取底层树引用（外部算法需要用它来 get_root）
    BTreeType& get_tree() { return tree_; }

    // =============================================================
    // 策略 1: No Prefetch (Baseline)
    // 逻辑实现：algorithms/NoPrefetch.hpp
    // =============================================================
    std::vector<bool> batch_lookup_no_prefetch(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        // 调用外部策略，传入 tree_ 实例
        return algorithms::NoPrefetch<BTreeType>::batch_lookup(tree_, queries, results);
    }
    
    // =============================================================
    // 策略 2: Group Prefetch
    // 逻辑实现：algorithms/GroupPrefetch.hpp
    // =============================================================
    template<size_t GROUP_SIZE = 32>
    std::vector<bool> batch_lookup_group_prefetch(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        // 调用外部策略
        return algorithms::GroupPrefetch<BTreeType>::template batch_lookup<GROUP_SIZE>(tree_, queries, results);
    }

    // =============================================================
    // 策略 3: SPP (Software Pipelined Prefetching)
    // 逻辑实现：algorithms/SPP.hpp
    // =============================================================
    template<size_t PIPELINE_DEPTH = 4>
    std::vector<bool> batch_lookup_spp(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        // 调用外部策略
        return algorithms::SoftwarePipelinedPrefetch<BTreeType>::template batch_lookup<PIPELINE_DEPTH>(tree_, queries, results);
    }
    // =============================================================
    // 策略 4: Vectorized Search (纯向量化计算，无显式预取)
    // =============================================================
    template<size_t VECTOR_SIZE = 64>
    std::vector<bool> batch_lookup_vectorized(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        return algorithms::VectorizedSearch<BTreeType>::template batch_lookup<VECTOR_SIZE>(tree_, queries, results);
    }
    // =============================================================
    // 策略 5: AMAC (Asynchronous Memory Access Chaining)
    // =============================================================
    template<size_t POOL_SIZE = 64>
    std::vector<bool> batch_lookup_amac(
        const std::vector<key_type>& queries,
        std::vector<mapped_type>& results
    ) {
        return algorithms::AMACSearch<BTreeType>::template batch_lookup<POOL_SIZE>(tree_, queries, results);
    }
};

} // namespace btree
} // namespace structures