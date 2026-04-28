#pragma once
#include <vector>
#include <string>

#include "../PGM_index/pgm_index.hpp"

namespace structures {
namespace pgm {
namespace algorithms {

template<typename PGMType>
class NoPrefetch {
public:
    using Key = typename PGMType::key_type;
    using Value = typename PGMType::data_type;

    // find_lower for PGM
    static inline size_t find_lower(const PGMType& pgm, const Key& key, size_t lo, size_t hi) {
        while (lo < hi) {
            size_t mid = (lo + hi) >> 1;
            if (key <= pgm.keys[mid]) hi = mid;
            else lo = mid + 1;
        }
        return lo;
    }

    static std::vector<bool> batch_lookup(
        PGMType& pgm,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());
        

        for (size_t i = 0; i < queries.size(); ++i) {
            const Key& key = queries[i];

            auto range = pgm.index.search(key);

            size_t hi = range.hi > pgm.keys.size() ? pgm.keys.size() : range.hi;
            size_t slot = find_lower(pgm, key, range.lo, hi);

            if (slot < pgm.keys.size() && key == pgm.keys[slot]) {
                results[i] = pgm.values[slot];
                found[i] = true;
            } else {
                found[i] = false;
            }
        }
        return found;
    }

    static std::string name() { return "No Prefetch"; }
};

} // namespace algorithms
} // namespace pgm
} // namespace structures