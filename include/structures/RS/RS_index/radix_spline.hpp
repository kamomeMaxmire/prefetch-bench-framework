#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "common.hpp"

namespace rs {

template <typename KeyType>
class RadixSpline {
public:
    static_assert(std::is_integral<KeyType>::value,
                  "RadixSpline only supports integral key types.");
    static_assert(std::is_unsigned<KeyType>::value,
                  "This RadixSpline implementation expects unsigned key types, e.g. uint32_t or uint64_t.");

    RadixSpline()
        : min_key_(0),
          max_key_(0),
          num_keys_(0),
          num_radix_bits_(0),
          max_error_(0) {}

    RadixSpline(
        KeyType min_key,
        KeyType max_key,
        size_t num_keys,
        size_t num_radix_bits,
        size_t max_error,
        std::vector<Coord<KeyType>> spline_points,
        std::vector<size_t> radix_table
    )
        : min_key_(min_key),
          max_key_(max_key),
          num_keys_(num_keys),
          num_radix_bits_(num_radix_bits),
          max_error_(max_error),
          spline_points_(std::move(spline_points)),
          radix_table_(std::move(radix_table)) {
        if (num_radix_bits_ >= sizeof(size_t) * 8) {
            throw std::invalid_argument("num_radix_bits is too large.");
        }

        const size_t expected_radix_size = (size_t(1) << num_radix_bits_) + 1;
        if (!radix_table_.empty() && radix_table_.size() != expected_radix_size) {
            throw std::invalid_argument("radix_table size does not match num_radix_bits.");
        }
    }

    SearchBound GetSearchBound(const KeyType& key) const {
        if (num_keys_ == 0 || spline_points_.empty()) {
            return {0, 0};
        }

        size_t pos = GetEstimatedPosition(key);

        size_t begin = 0;
        if (pos > max_error_) {
            begin = pos - max_error_;
        }

        size_t end = pos + max_error_ + 2;
        if (end > num_keys_) {
            end = num_keys_;
        }

        if (begin > end) {
            begin = end;
        }

        return {begin, end};
    }

    size_t GetSize() const {
        return sizeof(*this)
             + spline_points_.size() * sizeof(Coord<KeyType>)
             + radix_table_.size() * sizeof(size_t);
    }

    size_t size_in_bytes() const {
        return GetSize();
    }

    size_t GetNumKeys() const {
        return num_keys_;
    }

    size_t GetMaxError() const {
        return max_error_;
    }

    size_t GetNumRadixBits() const {
        return num_radix_bits_;
    }

    const std::vector<Coord<KeyType>>& GetSplinePoints() const {
        return spline_points_;
    }

    const std::vector<size_t>& GetRadixTable() const {
        return radix_table_;
    }

private:
    KeyType min_key_;
    KeyType max_key_;

    size_t num_keys_;
    size_t num_radix_bits_;
    size_t max_error_;

    std::vector<Coord<KeyType>> spline_points_;
    std::vector<size_t> radix_table_;

private:
    static size_t BitWidth(KeyType x) {
        if (x == 0) {
            return 1;
        }

        size_t bits = 0;
        while (x > 0) {
            ++bits;
            x >>= 1;
        }
        return bits;
    }

    size_t GetPrefix(const KeyType& key) const {
        if (num_radix_bits_ == 0) {
            return 0;
        }

        const size_t max_prefix = (size_t(1) << num_radix_bits_) - 1;

        KeyType diff = 0;
        if (key <= min_key_) {
            diff = 0;
        } else if (key >= max_key_) {
            diff = max_key_ - min_key_;
        } else {
            diff = key - min_key_;
        }

        const KeyType key_range = max_key_ - min_key_;
        const size_t key_bits = BitWidth(key_range);

        size_t prefix = 0;

        if (key_bits > num_radix_bits_) {
            const size_t shift = key_bits - num_radix_bits_;
            prefix = static_cast<size_t>(diff >> shift);
        } else {
            const size_t shift = num_radix_bits_ - key_bits;
            prefix = static_cast<size_t>(diff << shift);
        }

        if (prefix > max_prefix) {
            prefix = max_prefix;
        }

        return prefix;
    }

    size_t GetSplineSegment(const KeyType& key) const {
        const size_t n = spline_points_.size();

        if (n <= 1) {
            return 0;
        }

        size_t search_begin = 0;
        size_t search_end = n;

        if (!radix_table_.empty()) {
            const size_t prefix = GetPrefix(key);

            search_begin = radix_table_[prefix];
            search_end = radix_table_[prefix + 1];

            // The segment responsible for this key may start just before
            // the first spline point with the same radix prefix.
            if (search_begin > 0) {
                --search_begin;
            }

            // Keep one extra point on the right so interpolation has a next point.
            if (search_end < n) {
                ++search_end;
            }

            if (search_begin >= n) {
                search_begin = n - 1;
            }

            if (search_end > n) {
                search_end = n;
            }

            if (search_begin >= search_end) {
                search_end = search_begin + 1;
                if (search_end > n) {
                    search_end = n;
                }
            }
        }

        auto begin_it = spline_points_.begin() + search_begin;
        auto end_it = spline_points_.begin() + search_end;

        auto it = std::upper_bound(
            begin_it,
            end_it,
            key,
            [](const KeyType& k, const Coord<KeyType>& point) {
                return k < point.x;
            }
        );

        size_t idx = 0;

        if (it == spline_points_.begin()) {
            idx = 0;
        } else {
            idx = static_cast<size_t>((it - spline_points_.begin()) - 1);
        }

        if (idx >= n - 1) {
            idx = n - 2;
        }

        return idx;
    }

    size_t GetEstimatedPosition(const KeyType& key) const {
        const size_t n = spline_points_.size();

        if (n == 0) {
            return 0;
        }

        if (n == 1) {
            return ClampPosition(spline_points_[0].y);
        }

        if (key <= spline_points_.front().x) {
            return ClampPosition(spline_points_.front().y);
        }

        if (key >= spline_points_.back().x) {
            return ClampPosition(spline_points_.back().y);
        }

        const size_t segment_idx = GetSplineSegment(key);

        const Coord<KeyType>& left = spline_points_[segment_idx];
        const Coord<KeyType>& right = spline_points_[segment_idx + 1];

        if (right.x == left.x) {
            return ClampPosition(left.y);
        }

        const double x_diff = static_cast<double>(right.x - left.x);
        const double y_diff = right.y - left.y;
        const double slope = y_diff / x_diff;

        const double estimated =
            left.y + slope * static_cast<double>(key - left.x);

        return ClampPosition(estimated);
    }

    size_t ClampPosition(double pos) const {
        if (pos <= 0.0) {
            return 0;
        }

        if (pos >= static_cast<double>(num_keys_)) {
            return num_keys_;
        }

        return static_cast<size_t>(pos);
    }
};

} // namespace rs