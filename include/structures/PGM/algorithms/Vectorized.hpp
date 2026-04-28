#pragma once
#include <vector>
#include <string>

// 引入 PGM 核心头文件
#include "../PGM_index/pgm_index.hpp"

namespace structures {
namespace pgm {
namespace algorithms {

template<typename PGMMapType>
class VectorizedSearch {
public:
    using Key = typename PGMMapType::key_type;
    using Value = typename PGMMapType::data_type;

    static constexpr int calc_steps(size_t max_range) {
        int steps = 0;
        size_t capacity = 1;
        while (capacity <= max_range) {
            capacity *= 2;
            steps++;
        }
        return steps;
    }

    template<size_t VECTOR_SIZE = 64>
    static std::vector<bool> batch_lookup(
        PGMMapType& pgm_map,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        using PGMIndexType = typename std::remove_reference<decltype(pgm_map.index)>::type;
        static constexpr size_t EPS = PGMIndexType::epsilon_value;
        static constexpr int BSEARCH_STEPS = calc_steps(2 * EPS + 2);

        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        const size_t data_size = pgm_map.keys.size();
        if (data_size == 0 || queries.empty()) return found;

        size_t num_levels = pgm_map.index.levels_offsets.size() - 1;
        
        // 向量化寄存器数组 (要求内存对齐以利于 AVX 加载)
        size_t lo[VECTOR_SIZE];
        size_t hi[VECTOR_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += VECTOR_SIZE) {
            size_t this_batch_size = std::min(VECTOR_SIZE, queries.size() - batch_start);

            // =======================================================
            // Phase 1: Vectorized Range Search
            // This loop relies on hardware out-of-order execution to hide latency
            // by processing multiple independent queries concurrently.
            // No software prefetching is used.
            // =======================================================
            for (size_t i = 0; i < this_batch_size; ++i) {
                const Key& key = queries[batch_start + i];
                auto range = pgm_map.index.search(key);
                size_t pos = range.pos;
                lo[i] = (pos < EPS) ? 0 : pos - EPS;
                hi[i] = std::min(pos + EPS + 2, data_size);
            }

            // =======================================================
            // Phase 2: 物理数组的向量化二分查找 (Vectorized Binary Search)
            // =======================================================
            // The BSEARCH_STEPS loop is unrolled here. The outer loop over the batch
            // allows the CPU to hide the latency of each binary search step.
            for (int step = 0; step < BSEARCH_STEPS; ++step) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        size_t mid = (lo[i] + hi[i]) >> 1;
                        const Key& key = queries[batch_start + i];
                        
                        // 真正的无分支计算 (Branchless Binary Search)，强制编译器生成 CMOV 指令
                        bool cmp = key > pgm_map.keys[mid];
                        lo[i] = cmp ? mid + 1 : lo[i];
                        hi[i] = cmp ? hi[i] : mid;
                    }
                }
            }

            // =======================================================
            // Phase 3: 结果收集
            // =======================================================
            for (size_t i = 0; i < this_batch_size; ++i) {
                size_t slot = lo[i];
                size_t idx = batch_start + i;
                const Key& key = queries[idx];
                
                if (slot < data_size && key == pgm_map.keys[slot]) { 
                    results[idx] = pgm_map.values[slot]; 
                    found[idx] = true;
                } else {
                    found[idx] = false;
                }
            }
        }
        return found;
    }

    static std::string name() { return "Vectorized"; }
};

} // namespace algorithms
} // namespace pgm
} // namespace structures