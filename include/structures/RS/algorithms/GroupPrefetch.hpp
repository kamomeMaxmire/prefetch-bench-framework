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
class GroupPrefetch {
public:
    using Key = typename RSType::key_type;
    using Value = typename RSType::data_type;

    template<size_t GROUP_SIZE = 32>
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

        size_t lo[GROUP_SIZE];
        size_t hi[GROUP_SIZE];

        for (size_t batch_start = 0;
             batch_start < queries.size();
             batch_start += GROUP_SIZE) {

            const size_t this_batch_size =
                std::min(GROUP_SIZE, queries.size() - batch_start);

            // --- Phase 1: RS 预测每个 query 的物理搜索范围 ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                const Key& key = queries[batch_start + i];

                auto bound = rs.index.GetSearchBound(key);

                lo[i] = std::min(bound.begin, data_size);
                hi[i] = std::min(bound.end, data_size);

                if (lo[i] > hi[i]) {
                    lo[i] = hi[i];
                }

                // 对物理数组发出第一枪预取
                if (lo[i] < hi[i]) {
                    size_t mid = (lo[i] + hi[i]) >> 1;
                    __builtin_prefetch(&rs.keys[mid], 0, 1);
                }
            }

            // --- Phase 2: 对这一组 query 交错执行二分查找 ---
            bool active = true;

            while (active) {
                active = false;

                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        active = true;

                        const Key& key = queries[batch_start + i];

                        size_t mid = (lo[i] + hi[i]) >> 1;

                        if (key <= rs.keys[mid]) {
                            hi[i] = mid;
                        } else {
                            lo[i] = mid + 1;
                        }

                        // 预取下一轮可能访问的位置
                        if (lo[i] < hi[i]) {
                            size_t next_mid = (lo[i] + hi[i]) >> 1;
                            __builtin_prefetch(&rs.keys[next_mid], 0, 1);
                        }
                    }
                }
            }

            // --- Phase 3: 收集结果 ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                const size_t idx = batch_start + i;
                const size_t slot = lo[i];
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
        return "RS Group Prefetch";
    }
};

} // namespace algorithms
} // namespace rs
} // namespace structures