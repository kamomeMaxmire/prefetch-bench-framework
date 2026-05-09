// RadixSpline index for learned index framework
// Compatible with the PGM-index NoPrefetch interface
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rs {

/**
 * A struct that stores the result of a query to a RadixSpline index,
 * that is, a range [lo, hi) where the key can be found.
 */
struct SearchBound {
    size_t lo; ///< The lower bound of the range (inclusive).
    size_t hi; ///< The upper bound of the range (exclusive).
};

/**
 * A static learned index based on the RadixSpline algorithm.
 *
 * Given a sorted array of keys, it builds a two‑level structure:
 *   1. A set of spline points (linear interpolation segments).
 *   2. A radix table that maps the high bits of a key directly to a spline segment.
 *
 * A query first uses the radix table to locate the correct spline segment, then
 * performs linear interpolation to predict the key's position, and returns a
 * search range [lo, hi) guaranteed to contain the key (if it exists).
 *
 * @tparam K       The type of the indexed keys (must be unsigned integer).
 * @tparam Error   The maximum allowed prediction error (default 64).
 * @tparam RadixBits Number of high bits used for the radix table (default 16).
 */
template<typename K, size_t Error = 64, size_t RadixBits = 16>
class RadixSplineIndex {
public:
    using key_type = K;
    using data_type = size_t;   // values are the original positions (0‑based)
    static constexpr size_t epsilon_value = Error;
    static_assert(std::is_unsigned<K>::value, "RadixSpline requires unsigned key type");
    static_assert(Error > 0, "Error must be positive");
    static_assert(RadixBits > 0 && RadixBits <= 64, "RadixBits must be in (0,64]");

    // -----------------------------------------------------------------------
    // Public members (same naming as PGMIndex for compatibility)
    // -----------------------------------------------------------------------
    std::vector<K> keys;        ///< The sorted keys (from the training data).
    std::vector<size_t> values; ///< The values associated with the keys.

    /**
     * Proxy object that enables the syntax `index.search(key)`.
     */
    struct IndexProxy {
        const RadixSplineIndex *rs;
        SearchBound search(const K &key) const { return rs->search(key); }
    };

    IndexProxy index{this};     ///< To mimic PGMIndex's nested index object.

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------
    RadixSplineIndex() = default;

    /**
     * Builds the index from a sorted vector of keys.
     * Optionally accepts a vector of values; if omitted, values are 0,1,...,n-1.
     */
    explicit RadixSplineIndex(const std::vector<K> &keys_vec,
                              const std::vector<size_t> &vals = {})
        : keys(keys_vec) {
        if (vals.empty()) {
            values.resize(keys.size());
            for (size_t i = 0; i < keys.size(); ++i)
                values[i] = i;
        } else {
            if (vals.size() != keys.size())
                throw std::invalid_argument("keys and values must have the same size");
            values = vals;
        }
        if (!keys.empty())
            build();
    }

    /**
     * Returns the approximate position and search bounds for a given key.
     */
    SearchBound search(const K &key) const {
        if (keys.empty())
            return {0, 0};

        // 1. Radix lookup
        size_t bucket = static_cast<size_t>(key >> (64 - RadixBits));
        size_t spline_start = radix_table_[bucket];
        size_t spline_end   = radix_table_[bucket + 1];

        // 2. Linear interpolation
        auto [x1, y1] = spline_points_[spline_start];
        auto [x2, y2] = spline_points_[spline_end];

        if (key >= x2) {
            // Key beyond last spline point -> predict last position + error
            size_t pred = static_cast<size_t>(y2);
            size_t lo = (pred > Error) ? pred - Error : 0;
            size_t hi = std::min(pred + Error + 1, keys.size());
            return {lo, hi};
        }

        // slope = (y2 - y1) / (x2 - x1)
        double slope = static_cast<double>(y2 - y1) / static_cast<double>(x2 - x1);
        size_t pred = static_cast<size_t>(y1 + slope * (key - x1));

        size_t lo = (pred > Error) ? pred - Error : 0;
        size_t hi = std::min(pred + Error + 1, keys.size());
        return {lo, hi};
    }

    /**
     * Returns the size of the index in bytes (for benchmarking).
     */
    size_t size_in_bytes() const {
        return spline_points_.size() * sizeof(std::pair<K, size_t>)
               + radix_table_.size() * sizeof(size_t)
               + keys.size() * sizeof(K)
               + values.size() * sizeof(size_t);
    }

private:
    std::vector<std::pair<K, size_t>> spline_points_;
    std::vector<size_t> radix_table_;

    // -----------------------------------------------------------------------
    // Build logic (implements the RadixSpline construction algorithm)
    // -----------------------------------------------------------------------
    void build() {
        size_t n = keys.size();
        K min_key = keys.front();
        K max_key = keys.back();

        // ---- Step 1: Greedy spline point selection ----
        spline_points_.clear();
        spline_points_.emplace_back(min_key, 0);

        // Parameters for the greedy algorithm
        size_t last_idx  = 0;
        K      last_key  = min_key;
        size_t last_pos  = 0;

        for (size_t i = 1; i < n; ++i) {
            K curr_key = keys[i];
            // Check if we can extend the current spline segment
            // while keeping the interpolation error <= Error for all points since last_idx.
            bool can_extend = true;
            for (size_t j = last_idx + 1; j <= i; ++j) {
                double predicted = last_pos + static_cast<double>(keys[j] - last_key)
                                            / static_cast<double>(curr_key - last_key)
                                            * (i - last_pos);
                if (std::fabs(predicted - j) > Error) {
                    can_extend = false;
                    break;
                }
            }
            if (!can_extend) {
                // Start a new segment using the previous point
                spline_points_.emplace_back(keys[i - 1], i - 1);
                last_idx  = i - 1;
                last_key  = keys[i - 1];
                last_pos  = i - 1;
                // Re‑evaluate current i in the next iteration (don't skip it)
                --i;
            }
        }
        // Add the last point
        if (spline_points_.back().first != max_key || spline_points_.back().second != n - 1)
            spline_points_.emplace_back(max_key, n - 1);

        // ---- Step 2: Build radix table ----
        size_t table_size = (1ULL << RadixBits) + 1;
        radix_table_.resize(table_size);

        size_t spline_idx = 0;
        for (size_t i = 0; i < table_size; ++i) {
            K bucket_prefix = static_cast<K>(i) << (64 - RadixBits);
            while (spline_idx + 1 < spline_points_.size()
                   && spline_points_[spline_idx + 1].first <= bucket_prefix) {
                ++spline_idx;
            }
            radix_table_[i] = spline_idx;
        }
        // Ensure the last entry points to the last spline point
        radix_table_[table_size - 1] = spline_points_.size() - 1;
    }
};

} // namespace rs