#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace pgm {
namespace algorithms {

template<typename PGMMapType>
class GroupPrefetch {
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

    template<size_t GROUP_SIZE = 32>
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

        size_t approx_pos[GROUP_SIZE];
        size_t lo[GROUP_SIZE];
        size_t hi[GROUP_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += GROUP_SIZE) {
            size_t this_batch_size = std::min(GROUP_SIZE, queries.size() - batch_start);

            for (size_t i = 0; i < this_batch_size; ++i) {
                approx_pos[i] = 0;
            }

            // --- Phase 1: 内部导航层的交错预取 ---
            for (int l = num_levels - 1; l >= 1; --l) {
                size_t level_start = pgm_map.index.levels_offsets[l];
                size_t level_end = pgm_map.index.levels_offsets[l + 1];

                for (size_t i = 0; i < this_batch_size; ++i) {
                    const Key& key = queries[batch_start + i];
                    auto it = std::upper_bound(pgm_map.index.segments.begin() + level_start, 
                                               pgm_map.index.segments.begin() + level_end, key);
                    auto segment = (it == pgm_map.index.segments.begin() + level_start) ? it : it - 1;
                    
                    approx_pos[i] = segment->operator()(key);

                    // 向下一层发出预取
                    size_t next_level_start = pgm_map.index.levels_offsets[l - 1];
                    __builtin_prefetch(&pgm_map.index.segments[next_level_start + approx_pos[i]], 0, 1);
                }
            }

            // --- Phase 1.5: 算出底层物理区间的边界 ---
            for (size_t i = 0; i < this_batch_size; ++i) {
                const Key& key = queries[batch_start + i];
                size_t bottom_start = pgm_map.index.levels_offsets[0];
                size_t bottom_end = pgm_map.index.levels_offsets[1];
                auto it = std::upper_bound(pgm_map.index.segments.begin() + bottom_start, 
                                           pgm_map.index.segments.begin() + bottom_end, key);
                auto segment = (it == pgm_map.index.segments.begin() + bottom_start) ? it : it - 1;
                
                size_t pos = segment->operator()(key);
                lo[i] = (pos < EPS) ? 0 : pos - EPS;
                hi[i] = std::min(pos + EPS + 2, data_size);
                
                // 对物理数组发出第一枪预取
                if (lo[i] < hi[i]) __builtin_prefetch(&pgm_map.keys[(lo[i] + hi[i]) >> 1], 0, 1);
            }

            // --- Phase 2: 物理数据层交错二分查找 ---
            for (int step = 0; step < BSEARCH_STEPS; ++step) {
                for (size_t i = 0; i < this_batch_size; ++i) {
                    if (lo[i] < hi[i]) {
                        size_t mid = (lo[i] + hi[i]) >> 1;
                        const Key& key = queries[batch_start + i];
                        
                        if (key <= pgm_map.keys[mid]) hi[i] = mid; 
                        else lo[i] = mid + 1;

                        if (lo[i] < hi[i]) {
                            size_t next_mid = (lo[i] + hi[i]) >> 1;
                            __builtin_prefetch(&pgm_map.keys[next_mid], 0, 1); 
                        }
                    }
                }
            }

            // --- Phase 3: 收集结果 ---
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

    static std::string name() { return "Group Prefetch (Full-Path)"; }
};

} // namespace algorithms
} // namespace pgm
} // namespace structures