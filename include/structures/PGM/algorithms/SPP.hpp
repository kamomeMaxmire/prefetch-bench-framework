#pragma once
#include <vector>
#include <deque>
#include <string>
#include <algorithm>

// 引入 PGM 核心头文件
#include "../PGM_index/pgm_index.hpp"

namespace structures {
namespace pgm {
namespace algorithms {

// PGM 版 Static Pipelined Batching (H*D) with Zero-copy Pointer Rotation
template<typename PGMMapType>
class StaticSPP {
public:
    using Key = typename PGMMapType::key_type;
    using Value = typename PGMMapType::data_type;

    // 编译期算步数
    static constexpr int calc_steps(size_t max_range) {
        int steps = 0;
        size_t capacity = 1;
        while (capacity <= max_range) {
            capacity *= 2;
            steps++;
        }
        return steps;
    }

    template<size_t GROUP_SIZE = 64> // Default prefetch distance D
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

        const int H = BSEARCH_STEPS;
        if (H <= 0) return found;

        const int NUM_STAGES = H + 1; // 修复差一错误，总共需要 H+1 个阶段

        struct Task {
            size_t query_idx;
            size_t lo;
            size_t hi;
        };

        // Allocate memory for all stages
        std::vector<Task> memory(NUM_STAGES * GROUP_SIZE, {0, 1, 0});
        // Array of pointers to each stage's start
        std::vector<Task*> stages(NUM_STAGES);
        for (int i = 0; i < NUM_STAGES; ++i) {
            stages[i] = &memory[i * GROUP_SIZE];
        }

        size_t next_query_idx = 0;
        size_t finished_count = 0;
        const size_t total_queries = queries.size();

       while (finished_count < total_queries) {

            // --- Stage H: Final step of binary search & verification ---
            Task* final_stage = stages[H]; // 从 H-1 改为 H
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (final_stage[i].lo <= final_stage[i].hi) { // is active
                    size_t slot = final_stage[i].lo;
                    size_t q_idx = final_stage[i].query_idx;
                    const Key& key = queries[q_idx];

                    if (slot < data_size && key == pgm_map.keys[slot]) {
                        results[q_idx] = pgm_map.values[slot];
                        found[q_idx] = true;
                    } else {
                        found[q_idx] = false;
                    }
                    
                    finished_count++;
                    final_stage[i].lo = 1; 
                    final_stage[i].hi = 0;
                }
            }

            // --- Stage H-1 down to 0: Process binary search steps ---
            // 完美执行 H 次二分查找，确保 lo == hi
            for (int s = H - 1; s >= 0; --s) { // 从 H-2 改为 H-1
                Task* current_stage = stages[s];
                for (size_t i = 0; i < GROUP_SIZE; ++i) {
                    if (current_stage[i].lo <= current_stage[i].hi) { // is active
                        size_t lo = current_stage[i].lo;
                        size_t hi = current_stage[i].hi;
                        const Key& key = queries[current_stage[i].query_idx];

                        if (lo < hi) {
                            size_t mid = (lo + hi) >> 1;
                            if (key <= pgm_map.keys[mid]) hi = mid;
                            else lo = mid + 1;
                        }

                        current_stage[i].lo = lo;
                        current_stage[i].hi = hi;

                        if (lo < hi) {
                            size_t next_mid = (lo + hi) >> 1;
                            __builtin_prefetch(&pgm_map.keys[next_mid], 0, 1);
                        }
                    }
                }
            }

            // --- Rotate Pipeline Stages ---
            Task* empty_stage = stages[H]; // 从 H-1 改为 H
            for (int s = H; s > 0; --s) {  // 从 H-1 改为 H
                stages[s] = stages[s - 1];
            }
            stages[0] = empty_stage;
            // --- Stage 0 Refill: Feed new queries into the pipeline ---
            Task* refill_stage = stages[0];
            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (next_query_idx < total_queries) {
                    size_t q_idx = next_query_idx++;
                    auto range = pgm_map.index.search(queries[q_idx]);
                    refill_stage[i] = {q_idx, range.lo, std::min(range.hi, data_size)};
                    
                    if (refill_stage[i].lo < refill_stage[i].hi) {
                        size_t mid = (refill_stage[i].lo + refill_stage[i].hi) >> 1;
                        __builtin_prefetch(&pgm_map.keys[mid], 0, 1);
                    }
                } else {
                    refill_stage[i].lo = 1; // Mark as inactive
                    refill_stage[i].hi = 0;
                }
            }
        }
        return found;
    }

    static std::string name() { return "SPP (H*D)"; }
};

} // namespace algorithms
} // namespace pgm
} // namespace structures