#pragma once

#include <vector>
#include <string>
#include <cstddef>
#include <algorithm>

#include "../RS_index/RSWrapper.hpp"

namespace structures {
namespace rs {
namespace algorithms {

template<typename RSType>
class NoPrefetch {
public:
    using Key = typename RSType::key_type;
    using Value = typename RSType::data_type;

    static inline size_t find_lower(
        const RSType& rs,
        const Key& key,
        size_t lo,
        size_t hi
    ) {
        while (lo < hi) {
            size_t mid = (lo + hi) >> 1;
            if (key <= rs.keys[mid]) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }
        return lo;
    }

    static std::vector<bool> batch_lookup(
        RSType& rs,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        const size_t n = rs.keys.size();

        for (size_t i = 0; i < queries.size(); ++i) {
            const Key& key = queries[i];

            auto bound = rs.index.GetSearchBound(key);

            size_t lo = std::min(bound.begin, n);
            size_t hi = std::min(bound.end, n);

            size_t slot = find_lower(rs, key, lo, hi);

            if (slot < n && key == rs.keys[slot]) {
                results[i] = rs.values[slot];
                found[i] = true;
            } else {
                found[i] = false;
            }
        }

        return found;
    }

    static std::string name() {
        return "RS No Prefetch";
    }
};

} // namespace algorithms
} // namespace rs
} // namespace structures