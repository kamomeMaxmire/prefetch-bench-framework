#pragma once
#include <vector>
#include <string>

// 引入 PGM 核心头文件
#include "../PGM_index/pgm_index.hpp"

namespace structures {
namespace pgm {
namespace algorithms {

template<typename PGMMapType>
class AMACSearch {
public:
    using Key   = typename PGMMapType::key_type;
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

private:
    enum class state_t { INIT, INTERNAL_SEARCH,BSEARCH, DONE };

    struct fsm_shared_t {  
        state_t state; 
        size_t  i;       // query 索引
        size_t  lo;      // 二分查找下界
        size_t  hi;      // 二分查找上界
        int     step;    // 当前处于二分查找的第几步
        int     current_level; //记录当前卡在 PGM 的哪一层
        size_t  approx_pos;
    };

public:
    template<size_t POOL_SIZE = 64, size_t IGNORED = 0>
    static std::vector<bool> batch_lookup(
        PGMMapType&             pgm_map,
        const std::vector<Key>& queries,
        std::vector<Value>&     results
    ) {
        size_t n = queries.size();
        std::vector<bool> found(n, false);
        results.resize(n); 

        const size_t data_size = pgm_map.keys.size();
        if (data_size == 0 || n == 0) return found;

        using PGMIndexType = typename std::remove_reference<decltype(pgm_map.index)>::type;
        static constexpr size_t EPS = PGMIndexType::epsilon_value;
        static constexpr int BSEARCH_STEPS = calc_steps(2 * EPS + 2);

        fsm_shared_t pool[POOL_SIZE];
        size_t next_query_idx = 0;

        // init pool
        for (size_t k = 0; k < POOL_SIZE; ++k) {
            if (next_query_idx < n) {
                pool[k].state = state_t::INIT;
                pool[k].i = next_query_idx++;
            } else {
                pool[k].state = state_t::DONE;
            }
        }

        size_t all_done = 0;

        // FSM 轮询主循环
        while (all_done < n) {
            // Use a standard for-loop to improve branch prediction and allow compiler optimizations.
            for (size_t k = 0; k < POOL_SIZE; ++k) {

                // fill next query if this slot is done
                if (pool[k].state == state_t::DONE && next_query_idx < n) {
                    pool[k].state = state_t::INIT;
                    pool[k].i = next_query_idx++;
                }

                switch (pool[k].state) {
                case state_t::INIT: {
                pool[k].current_level = pgm_map.index.levels_offsets.size() - 2; // 从最顶层开始
                pool[k].approx_pos = 0;
                pool[k].state = state_t::INTERNAL_SEARCH;
                break;
                }

            case state_t::INTERNAL_SEARCH: {
                int l = pool[k].current_level;
                const Key& key = queries[pool[k].i];
                
                // 如果还没到底层
                if (l >= 0) {
                    size_t level_start = pgm_map.index.levels_offsets[l];
                    size_t level_end = pgm_map.index.levels_offsets[l + 1];
                    
                    auto it = std::upper_bound(pgm_map.index.segments.begin() + level_start, 
                                            pgm_map.index.segments.begin() + level_end, key);
                    auto segment = (it == pgm_map.index.segments.begin() + level_start) ? it : it - 1;
                    
                    size_t pos = segment->operator()(key);
                    
                    if (l > 0) { // 还在内部，预取下一层模型
                        size_t next_level_start = pgm_map.index.levels_offsets[l - 1];
                        __builtin_prefetch(&pgm_map.index.segments[next_level_start + pos], 0, 1);
                        pool[k].approx_pos = pos;
                        pool[k].current_level--;
                    } else { // 到达底层物理边界
                        pool[k].lo = (pos < EPS) ? 0 : pos - EPS;
                        pool[k].hi = std::min(pos + EPS + 2, data_size);
                        pool[k].step = 0;
                        
                        if (pool[k].lo < pool[k].hi) {
                            __builtin_prefetch(&pgm_map.keys[(pool[k].lo + pool[k].hi) >> 1], 0, 1);
                        }
                        pool[k].state = state_t::BSEARCH; // 状态跃迁：进入物理二分
                    }
                }
                break;
                }

                case state_t::BSEARCH: {
                    if (pool[k].step < BSEARCH_STEPS) {
                        // 还在二分查找的步数范围内，继续推演
                        if (pool[k].lo < pool[k].hi) {
                            size_t mid = (pool[k].lo + pool[k].hi) >> 1;
                            const Key& key = queries[pool[k].i];

                            if (key <= pgm_map.keys[mid]) pool[k].hi = mid;
                            else pool[k].lo = mid + 1;

                            if (pool[k].lo < pool[k].hi) {
                                size_t next_mid = (pool[k].lo + pool[k].hi) >> 1;
                                __builtin_prefetch(&pgm_map.keys[next_mid], 0, 1);
                            }
                        }
                        pool[k].step++;
                    } else {
                        // 二分查找固定步数走完，验证最终结果
                        size_t slot = pool[k].lo;
                        const Key& key = queries[pool[k].i];

                        if (slot < data_size && key == pgm_map.keys[slot]) {
                            results[pool[k].i] = pgm_map.values[slot];
                            found[pool[k].i] = true;
                        } else {
                            found[pool[k].i] = false;
                        }

                        // 直接跃迁到 DONE 状态并记录完成
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

    static std::string name() { return "AMAC"; }
};

} // namespace algorithms
} // namespace pgm
} // namespace structures