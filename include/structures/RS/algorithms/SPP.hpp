#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "../RS_index/RSWrapper.hpp"

namespace structures {
namespace rs {
namespace algorithms {

// RS 版 Static Pipelined Batching (H*D) with Zero-copy Pointer Rotation
template<typename RSType>
class StaticSPP {
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

    template<size_t GROUP_SIZE = 64>
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
         * PGM 版用：
         *   using PGMIndexType = ...
         *   static constexpr size_t EPS = PGMIndexType::epsilon_value;
         *
         * 我们前面写的 RS index 里是运行时接口：
         *   rs.index.GetMaxError()
         *
         * 所以这里用动态 H，更稳，不要求 RSType 暴露 epsilon_value。
         */
        const size_t error = rs.index.GetMaxError();
        const int H = calc_steps(2 * error + 2);

        if (H <= 0) {
            return found;
        }

        const int NUM_STAGES = H + 1;

        struct Task {
            size_t query_idx;
            size_t lo;
            size_t hi;
        };

        // lo > hi means inactive.
        std::vector<Task> memory(NUM_STAGES * GROUP_SIZE, {0, 1, 0});

        std::vector<Task*> stages(NUM_STAGES);
        for (int i = 0; i < NUM_STAGES; ++i) {
            stages[i] = &memory[i * GROUP_SIZE];
        }

        size_t next_query_idx = 0;
        size_t finished_count = 0;
        const size_t total_queries = queries.size();

        while (finished_count < total_queries) {
            // --- Stage H: verification ---
            Task* final_stage = stages[H];

            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (final_stage[i].lo <= final_stage[i].hi) {
                    const size_t slot = final_stage[i].lo;
                    const size_t q_idx = final_stage[i].query_idx;
                    const Key& key = queries[q_idx];

                    if (slot < data_size && key == rs.keys[slot]) {
                        results[q_idx] = rs.values[slot];
                        found[q_idx] = true;
                    } else {
                        found[q_idx] = false;
                    }

                    finished_count++;

                    // Mark inactive.
                    final_stage[i].lo = 1;
                    final_stage[i].hi = 0;
                }
            }

            // --- Stage H-1 down to 0: one binary-search step per stage ---
            for (int s = H - 1; s >= 0; --s) {
                Task* current_stage = stages[s];

                for (size_t i = 0; i < GROUP_SIZE; ++i) {
                    if (current_stage[i].lo <= current_stage[i].hi) {
                        size_t lo = current_stage[i].lo;
                        size_t hi = current_stage[i].hi;

                        const size_t q_idx = current_stage[i].query_idx;
                        const Key& key = queries[q_idx];

                        if (lo < hi) {
                            const size_t mid = (lo + hi) >> 1;

                            if (key <= rs.keys[mid]) {
                                hi = mid;
                            } else {
                                lo = mid + 1;
                            }
                        }

                        current_stage[i].lo = lo;
                        current_stage[i].hi = hi;

                        if (lo < hi) {
                            const size_t next_mid = (lo + hi) >> 1;
                            __builtin_prefetch(&rs.keys[next_mid], 0, 1);
                        }
                    }
                }
            }

            // --- Rotate pipeline stages ---
            Task* empty_stage = stages[H];

            for (int s = H; s > 0; --s) {
                stages[s] = stages[s - 1];
            }

            stages[0] = empty_stage;

            // --- Stage 0 refill ---
            Task* refill_stage = stages[0];

            for (size_t i = 0; i < GROUP_SIZE; ++i) {
                if (next_query_idx < total_queries) {
                    const size_t q_idx = next_query_idx++;
                    const Key& key = queries[q_idx];

                    auto bound = rs.index.GetSearchBound(key);

                    size_t lo = std::min(bound.begin, data_size);
                    size_t hi = std::min(bound.end, data_size);

                    if (lo > hi) {
                        lo = hi;
                    }

                    refill_stage[i] = {q_idx, lo, hi};

                    if (lo < hi) {
                        const size_t mid = (lo + hi) >> 1;
                        __builtin_prefetch(&rs.keys[mid], 0, 1);
                    }
                } else {
                    // Mark inactive.
                    refill_stage[i].lo = 1;
                    refill_stage[i].hi = 0;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "RS SPP (H*D)";
    }
};

} // namespace algorithms
} // namespace rs
} // namespace structures