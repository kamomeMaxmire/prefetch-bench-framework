#pragma once
#include <vector>
#include <algorithm>
#include <string>
#include "../../../utils/PrefetchUtils.hpp"

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class SoftwarePipelinedPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Iterator = typename BTree::iterator;
    
    template<size_t DISTANCE = 4>
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        results.resize(queries.size());
        std::vector<bool> found(queries.size());
        
        if (queries.empty()) return found;
        
        const size_t N = queries.size();
        
        // Prologue: Prefetch first DISTANCE queries
        for (size_t i = 0; i < std::min(DISTANCE, N); ++i) {
            do_prefetch(&queries[i]);
        }
        
        // Main loop
        for (size_t i = 0; i < N; ++i) {
            // Prefetch future query
            if (i + DISTANCE < N) {
                do_prefetch(&queries[i + DISTANCE]);
            }
            
            // Execute current query
            auto it = btree.find(queries[i]);
            
            if (it != btree.end()) {
                results[i] = it->second;
                found[i] = true;
            } else {
                found[i] = false;
            }
        }
        
        return found;
    }
    
    static std::vector<bool> batch_lookup_d1(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        return batch_lookup<1>(btree, queries, results);
    }
    
    static std::vector<bool> batch_lookup_d2(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        return batch_lookup<2>(btree, queries, results);
    }
    
    template<size_t DISTANCE = 4>
    static std::string name() {
        return "SPP (D=" + std::to_string(DISTANCE) + ")";
    }
    
    static std::string name_d1() {
        return "SPP D=1";
    }
    
    static std::string name_d2() {
        return "SPP D=2";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures
