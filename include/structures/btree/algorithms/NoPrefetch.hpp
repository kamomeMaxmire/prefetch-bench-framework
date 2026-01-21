#pragma once
#include <vector>
#include <algorithm>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class NoPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Iterator = typename BTree::iterator;
    
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        results.resize(queries.size());
        std::vector<bool> found(queries.size());
        
        for (size_t i = 0; i < queries.size(); ++i) {
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
    
    static std::string name() {
        return "No Prefetch (Baseline)";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures
