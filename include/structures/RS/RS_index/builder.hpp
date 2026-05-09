#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "common.hpp"
#include "radix_spline.hpp"

namespace rs {

template <typename KeyType>
class Builder {
public:
    static_assert(std::is_integral<KeyType>::value,
                  "RadixSpline Builder only supports integral key types.");
    static_assert(std::is_unsigned<KeyType>::value,
                  "This RadixSpline Builder expects unsigned key types, e.g. uint32_t or uint64_t.");

    Builder(
        KeyType min_key,
        KeyType max_key,
        size_t num_radix_bits = 18,
        size_t max_error = 32
    )
        : min_key_(min_key),
          max_key_(max_key),
          num_radix_bits_(num_radix_bits),
          max_error_(max_error) {
        if (min_key_ > max_key_) {
            throw std::invalid_argument("min_key cannot be greater than max_key.");
        }

        if (num_radix_bits_ >= sizeof(size_t) * 8) {
            throw std::invalid_argument("num_radix_bits is too large.");
        }
    }

    void AddKey(const KeyType& key) {
        if (!keys_.empty() && key < keys_.back()) {
            throw std::invalid_argument("RadixSpline keys must be added in nondecreasing order.");
        }

        if (key < min_key_ || key > max_key_) {
            throw std::invalid_argument("key is out of [min_key, max_key] range.");
        }

        keys_.push_back(key);
    }

    RadixSpline<KeyType> Finalize() {
        std::vector<Coord<KeyType>> spline_points = BuildSplinePoints();
        std::vector<size_t> radix_table = BuildRadixTable(spline_points);

        return RadixSpline<KeyType>(
            min_key_,
            max_key_,
            keys_.size(),
            num_radix_bits_,
            max_error_,
            std::move(spline_points),
            std::move(radix_table)
        );
    }

private:
    KeyType min_key_;
    KeyType max_key_;

    size_t num_radix_bits_;
    size_t max_error_;

    std::vector<KeyType> keys_;

private:
    std::vector<Coord<KeyType>> BuildSplinePoints() const {
        std::vector<Coord<KeyType>> spline_points;

        const size_t n = keys_.size();
        if (n == 0) {
            return spline_points;
        }

        /*
         * 工程版 RadixSpline 构建：
         *
         * 这里没有直接照搬论文里的完整 spline corridor，而是用更稳的方式：
         * 每隔 max_error 个位置采一个 spline point。
         *
         * 好处：
         * 1. 代码短，容易接进你的框架。
         * 2. 查询时用 ±max_error 的窗口，基本能覆盖对应位置。
         * 3. 后面的 NoPrefetch / AMAC / SPP / Vectorized 都可以直接共用。
         *
         * 缺点：
         * 它不是最省空间的官方 RS 构建方式，但先跑通实验框架很合适。
         */
        const size_t stride = std::max<size_t>(1, max_error_);

        spline_points.push_back({keys_[0], 0.0});
        size_t last_added = 0;

        for (size_t i = stride; i < n; i += stride) {
            spline_points.push_back({keys_[i], static_cast<double>(i)});
            last_added = i;
        }

        if (last_added != n - 1) {
            spline_points.push_back({keys_[n - 1], static_cast<double>(n - 1)});
        }

        return spline_points;
    }

    std::vector<size_t> BuildRadixTable(
        const std::vector<Coord<KeyType>>& spline_points
    ) const {
        const size_t radix_table_size = (size_t(1) << num_radix_bits_) + 1;

        std::vector<size_t> radix_table(
            radix_table_size,
            spline_points.size()
        );

        if (spline_points.empty()) {
            return radix_table;
        }

        const size_t radix_limit = size_t(1) << num_radix_bits_;

        size_t table_pos = 0;

        for (size_t i = 0; i < spline_points.size(); ++i) {
            const size_t prefix = GetPrefix(spline_points[i].x);

            while (table_pos <= prefix && table_pos < radix_limit) {
                radix_table[table_pos] = i;
                ++table_pos;
            }
        }

        while (table_pos <= radix_limit) {
            radix_table[table_pos] = spline_points.size();
            ++table_pos;
        }

        return radix_table;
    }

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
};

} // namespace rs