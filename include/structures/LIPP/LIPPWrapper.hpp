#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "LIPP_index/lipp.h"
#include "algorithms/NoPrefetch.hpp"
#include "algorithms/GroupPrefetch.hpp"
#include "algorithms/SPP.hpp"
#include "algorithms/Vectorized.hpp"
#include "algorithms/AMAC.hpp"

namespace structures {
namespace lipp {

template<typename K = uint64_t, typename V = uint64_t, bool UseFMCD = true>
class LIPPWrapper {
public:
    using key_type = K;
    using data_type = V;
    using value_type = std::pair<K, V>;
    using index_type = ::LIPP<K, V, UseFMCD>;

    LIPPWrapper(double build_lr_remain = 0, bool quiet = true)
        : index_(build_lr_remain, quiet) {}

    void build_model(const std::vector<value_type>& data) {
        bulk_load(data);
    }

    void bulk_load(const std::vector<value_type>& data) {
        if (data.empty()) {
            index_.bulk_load(nullptr, 0);
            size_ = 0;
            return;
        }

        RT_ASSERT(std::is_sorted(
            data.begin(), data.end(),
            [](const value_type& lhs, const value_type& rhs) {
                return lhs.first < rhs.first;
            }
        ));

        index_.bulk_load(data.data(), static_cast<int>(data.size()));
        size_ = data.size();
    }

    template<typename Iterator>
    void bulk_load(Iterator begin, Iterator end) {
        std::vector<value_type> data(begin, end);
        bulk_load(data);
    }

    bool exists(const key_type& key) const {
        return index_.exists(key);
    }

    data_type at(const key_type& key, bool skip_existence_check = true) const {
        return index_.at(key, skip_existence_check);
    }

    size_t size() const {
        return size_;
    }

    size_t index_size(bool total = false, bool ignore_child = true) const {
        return index_.index_size(total, ignore_child);
    }

    void verify() const {
        index_.verify();
    }

    std::vector<bool> batch_lookup_no_prefetch(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::NoPrefetch<LIPPWrapper>::batch_lookup(*this, queries, results);
    }

    template<size_t GROUP_SIZE = 64>
    std::vector<bool> batch_lookup_group_prefetch(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::GroupPrefetch<LIPPWrapper>::template batch_lookup<GROUP_SIZE>(*this, queries, results);
    }

    template<size_t PIPELINE_DEPTH = 64>
    std::vector<bool> batch_lookup_spp(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::SoftwarePipelinedPrefetch<LIPPWrapper>::template batch_lookup<PIPELINE_DEPTH>(*this, queries, results);
    }

    template<size_t VECTOR_SIZE = 64>
    std::vector<bool> batch_lookup_vectorized(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::VectorizedSearch<LIPPWrapper>::template batch_lookup<VECTOR_SIZE>(*this, queries, results);
    }

    template<size_t POOL_SIZE = 64>
    std::vector<bool> batch_lookup_fsm_amac(
        const std::vector<key_type>& queries,
        std::vector<data_type>& results
    ) {
        return algorithms::AMACSearch<LIPPWrapper>::template batch_lookup<POOL_SIZE, 0>(*this, queries, results);
    }

    index_type& get_index() {
        return index_;
    }

    const index_type& get_index() const {
        return index_;
    }

private:
    index_type index_;
    size_t size_ = 0;
};

} // namespace lipp
} // namespace structures
