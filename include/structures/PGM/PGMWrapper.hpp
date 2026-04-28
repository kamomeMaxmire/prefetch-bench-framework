#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

// 引入 PGM 核心头文件
#include "PGM_index/pgm_index.hpp"

// 引入所有外部实现的算法策略
#include "algorithms/NoPrefetch.hpp"
#include "algorithms/GroupPrefetch.hpp"
#include "algorithms/SPP.hpp"
#include "algorithms/Vectorized.hpp"
#include "algorithms/AMAC.hpp"

namespace structures {
namespace pgm {

template<typename K = uint64_t, typename V = uint64_t, size_t Epsilon = 64>
class PGMWrapper {
public:
    using key_type = K;
    using data_type = V;
    
    std::vector<K> keys;
    std::vector<V> values;
    
    ::pgm::PGMIndex<K, Epsilon> index;

    void bulk_load(const std::vector<std::pair<K, V>>& input_data) {
        keys.reserve(input_data.size());
        values.reserve(input_data.size());
        for (const auto& kv : input_data) {
            keys.push_back(kv.first);
            values.push_back(kv.second);
        }
        index = ::pgm::PGMIndex<K, Epsilon>(keys.begin(), keys.end());
    }
    // =============================================================
    // 策略 1: No Prefetch (Baseline)
    // =============================================================
    std::vector<bool> batch_lookup_no_prefetch(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::NoPrefetch<PGMWrapper>::batch_lookup(*this, queries, results);
    }

    // =============================================================
    // 策略 2: Group Prefetch 
    // =============================================================
    template<size_t GROUP_SIZE = 32>
    std::vector<bool> batch_lookup_group_prefetch(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::GroupPrefetch<PGMWrapper>::template batch_lookup<GROUP_SIZE>(*this, queries, results);
    }

    // =============================================================
    // 策略 3: Static SPP 
    // =============================================================
    template<size_t PIPELINE_DEPTH = 4>
    std::vector<bool> batch_lookup_static_spp(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::StaticSPP<PGMWrapper>::template batch_lookup<PIPELINE_DEPTH>(*this, queries, results);
    }

    // =============================================================
    // 策略 4: Vectorized 
    // =============================================================
    template<size_t VECTOR_SIZE = 64>
    std::vector<bool> batch_lookup_vectorized(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::VectorizedSearch<PGMWrapper>::template batch_lookup<VECTOR_SIZE>(*this, queries, results);
    }

    // =============================================================
    // 策略 5: FSM AMAC 
    // =============================================================
    template<size_t POOL_SIZE = 64>
    std::vector<bool> batch_lookup_fsm_amac(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        // 对齐 B+ 树的参数规范，传入第二个占位符 0
        return algorithms::AMACSearch<PGMWrapper>::template batch_lookup<POOL_SIZE, 0>(*this, queries, results);
    }
};

} // namespace pgms
} // namespace structures