#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "builder.hpp"
#include "radix_spline.hpp"

namespace structures {
namespace rs {

template<
    typename KeyType = uint64_t,
    typename ValueType = uint64_t,
    size_t NumRadixBits = 18,
    size_t MaxError = 32
>
class RSWrapper {
public:
    static_assert(std::is_integral<KeyType>::value,
                  "RSWrapper only supports integral key types.");
    static_assert(std::is_unsigned<KeyType>::value,
                  "RSWrapper expects unsigned key types, e.g. uint32_t or uint64_t.");

    using key_type = KeyType;
    using data_type = ValueType;
    using index_type = ::rs::RadixSpline<KeyType>;

    std::vector<KeyType> keys;
    std::vector<ValueType> values;
    index_type index;

    RSWrapper() = default;

    RSWrapper(
        const std::vector<KeyType>& input_keys,
        const std::vector<ValueType>& input_values
    ) {
        build(input_keys, input_values);
    }

    void build(
        const std::vector<KeyType>& input_keys,
        const std::vector<ValueType>& input_values
    ) {
        if (input_keys.size() != input_values.size()) {
            throw std::invalid_argument("RSWrapper: keys and values size mismatch.");
        }

        keys = input_keys;
        values = input_values;

        if (keys.empty()) {
            index = index_type();
            return;
        }

        SortByKey();

        ::rs::Builder<KeyType> builder(
            keys.front(),
            keys.back(),
            NumRadixBits,
            MaxError
        );

        for (const auto& key : keys) {
            builder.AddKey(key);
        }

        index = builder.Finalize();
    }

    size_t size() const {
        return keys.size();
    }

    bool empty() const {
        return keys.empty();
    }

    size_t index_size_bytes() const {
        return index.GetSize();
    }

    size_t size_in_bytes() const {
        return index.GetSize()
             + keys.size() * sizeof(KeyType)
             + values.size() * sizeof(ValueType);
    }

private:
    void SortByKey() {
        if (std::is_sorted(keys.begin(), keys.end())) {
            return;
        }

        std::vector<size_t> order(keys.size());
        std::iota(order.begin(), order.end(), 0);

        std::stable_sort(order.begin(), order.end(),
            [&](size_t a, size_t b) {
                return keys[a] < keys[b];
            }
        );

        std::vector<KeyType> sorted_keys;
        std::vector<ValueType> sorted_values;

        sorted_keys.reserve(keys.size());
        sorted_values.reserve(values.size());

        for (size_t idx : order) {
            sorted_keys.push_back(keys[idx]);
            sorted_values.push_back(values[idx]);
        }

        keys.swap(sorted_keys);
        values.swap(sorted_values);
    }
};

} // namespace rs
} // namespace structures