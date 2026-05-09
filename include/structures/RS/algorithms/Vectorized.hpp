#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "../RS_index/RSWrapper.hpp"

namespace structures {
namespace rs {
namespace algorithms {

template<typename RSType>
class VectorizedSearch {
public:
    using Key = typename RSType::key_type;
    using Value = typename RSType::data_type;

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
        RSType& rs,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        const size_t data_size = rs.keys.size();

        if (data_size == 0 || queries.empty()) {
            return found;
        }

        /*
         * PGM 版是编译期 EPS：
         *   static constexpr size_t EPS = PGMIndexType::epsilon_value;
         *
         * RS 版这里用我们前面 RS index 里的运行时接口：
         *   rs.index.GetMaxError()
         */
        const size_t error = rs.index.GetMaxError();
        const int BSEARCH_STEPS = calc_steps(2 * error + 2);

        size_t lo[VECTOR_SIZE];
        size_t hi[VECTOR_SIZE];

        for (size_t batch_start = 0;
             batch_start < queries.size();
             batch_start += VECTOR_SIZE) {

            const size_t this_batch_size =
                std::min(VECTOR_SIZE, queries.size() - batch_start);

            // =======================================================
            // Phase 1: RS Range Search
            // 对每个 query 先通过 RadixSpline 得到物理数组搜索范围。
            // =======================================================
            for (size_t i = 0; i < this_batch_size; ++i) {
                const Key& key = queries[batch_start + i];

                auto bound = rs.index.GetSearchBound(key);

                lo[i] = std::min(bound.begin, data_size);
                hi[i] = std::min(bound.end, data_size);

                if (lo[i] > hi[i]) {
                    lo[i] = hi[i];
                }
            }

            // =======================================================
            // Phase 2: Branchless Binary Search
            // 这里和你 PGM 版一样，不显式 prefetch，
            // 主要靠 batch 内多个独立 query 暴露并行性。
            // =======================================================
            for (int step = 0; step < BSEARCH_STEPS; ++step) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        const size_t mid = (lo[i] + hi[i]) >> 1;
                        const Key& key = queries[batch_start + i];

                        const bool cmp = key > rs.keys[mid];

                        lo[i] = cmp ? mid + 1 : lo[i];
                        hi[i] = cmp ? hi[i] : mid;
                    }
                }
            }

            // =======================================================
            // Phase 3: Collect Results
            // =======================================================
            for (size_t i = 0; i < this_batch_size; ++i) {
                const size_t slot = lo[i];
                const size_t idx = batch_start + i;
                const Key& key = queries[idx];

                if (slot < data_size && key == rs.keys[slot]) {
                    results[idx] = rs.values[slot];
                    found[idx] = true;
                } else {
                    found[idx] = false;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "RS Vectorized";
    }
};

} // namespace algorithms
} // namespace rs
} // namespace structures