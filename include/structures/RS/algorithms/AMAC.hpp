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
class AMACSearch {
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

private:
    enum class state_t {
        INIT,
        RANGE_SEARCH,
        BSEARCH,
        DONE
    };

    struct fsm_shared_t {
        state_t state;
        size_t i;      // query index
        size_t lo;     // lower bound, inclusive
        size_t hi;     // upper bound, exclusive
        int step;      // current binary-search step
    };

public:
    template<size_t POOL_SIZE = 64, size_t IGNORED = 0>
    static std::vector<bool> batch_lookup(
        RSType& rs,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        static_assert(POOL_SIZE > 0, "POOL_SIZE must be positive.");

        const size_t n = queries.size();

        std::vector<bool> found(n, false);
        results.resize(n);

        const size_t data_size = rs.keys.size();
        if (data_size == 0 || n == 0) {
            return found;
        }

        const size_t error = rs.index.GetMaxError();
        const int BSEARCH_STEPS = calc_steps(2 * error + 2);

        fsm_shared_t pool[POOL_SIZE];

        size_t next_query_idx = 0;

        // Init pool.
        for (size_t k = 0; k < POOL_SIZE; ++k) {
            if (next_query_idx < n) {
                pool[k].state = state_t::INIT;
                pool[k].i = next_query_idx++;
                pool[k].lo = 1;
                pool[k].hi = 0;
                pool[k].step = 0;
            } else {
                pool[k].state = state_t::DONE;
                pool[k].i = 0;
                pool[k].lo = 1;
                pool[k].hi = 0;
                pool[k].step = 0;
            }
        }

        size_t all_done = 0;

        while (all_done < n) {
            for (size_t k = 0; k < POOL_SIZE; ++k) {

                // Fill next query if this FSM slot is free.
                if (pool[k].state == state_t::DONE && next_query_idx < n) {
                    pool[k].state = state_t::INIT;
                    pool[k].i = next_query_idx++;
                    pool[k].lo = 1;
                    pool[k].hi = 0;
                    pool[k].step = 0;
                }

                switch (pool[k].state) {
                case state_t::INIT: {
                    pool[k].state = state_t::RANGE_SEARCH;
                    break;
                }

                case state_t::RANGE_SEARCH: {
                    const Key& key = queries[pool[k].i];

                    auto bound = rs.index.GetSearchBound(key);

                    size_t lo = std::min(bound.begin, data_size);
                    size_t hi = std::min(bound.end, data_size);

                    if (lo > hi) {
                        lo = hi;
                    }

                    pool[k].lo = lo;
                    pool[k].hi = hi;
                    pool[k].step = 0;

                    // Prefetch first binary-search position.
                    if (pool[k].lo < pool[k].hi) {
                        size_t mid = (pool[k].lo + pool[k].hi) >> 1;
                        __builtin_prefetch(&rs.keys[mid], 0, 1);
                    }

                    pool[k].state = state_t::BSEARCH;
                    break;
                }

                case state_t::BSEARCH: {
                    if (pool[k].step < BSEARCH_STEPS) {
                        if (pool[k].lo < pool[k].hi) {
                            size_t mid = (pool[k].lo + pool[k].hi) >> 1;
                            const Key& key = queries[pool[k].i];

                            if (key <= rs.keys[mid]) {
                                pool[k].hi = mid;
                            } else {
                                pool[k].lo = mid + 1;
                            }

                            if (pool[k].lo < pool[k].hi) {
                                size_t next_mid = (pool[k].lo + pool[k].hi) >> 1;
                                __builtin_prefetch(&rs.keys[next_mid], 0, 1);
                            }
                        }

                        pool[k].step++;
                    } else {
                        const size_t slot = pool[k].lo;
                        const size_t q_idx = pool[k].i;
                        const Key& key = queries[q_idx];

                        if (slot < data_size && key == rs.keys[slot]) {
                            results[q_idx] = rs.values[slot];
                            found[q_idx] = true;
                        } else {
                            found[q_idx] = false;
                        }

                        pool[k].state = state_t::DONE;
                        ++all_done;
                    }

                    break;
                }

                case state_t::DONE:
                    break;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "RS AMAC";
    }
};

} // namespace algorithms
} // namespace rs
} // namespace structures