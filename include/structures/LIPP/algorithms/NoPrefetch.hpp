#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace structures {
namespace lipp {
namespace algorithms {

template<typename LIPPMap>
class NoPrefetch {
public:
    using Key = typename LIPPMap::key_type;
    using Value = typename LIPPMap::data_type;
    using Index = typename LIPPMap::index_type;
    using Node = typename Index::Node;

    static inline int predict_pos(const Node* node, const Key& key) {
        double v = node->model.predict_double(key);
        if (v > std::numeric_limits<int>::max() / 2) return node->num_items - 1;
        if (v < 0) return 0;
        return std::min(node->num_items - 1, static_cast<int>(v));
    }

    static std::vector<bool> batch_lookup(
        LIPPMap& lipp,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size(), false);
        results.resize(queries.size());

        const Index& index = lipp.get_index();
        if (!index.root || queries.empty()) return found;

        for (size_t i = 0; i < queries.size(); ++i) {
            const Key& key = queries[i];
            const Node* node = index.root;

            while (true) {
                int pos = predict_pos(node, key);
                if (BITMAP_GET(node->none_bitmap, pos) == 1) {
                    break;
                }
                if (BITMAP_GET(node->child_bitmap, pos) == 1) {
                    node = node->items[pos].comp.child;
                    continue;
                }

                if (node->items[pos].comp.data.key == key) {
                    results[i] = node->items[pos].comp.data.value;
                    found[i] = true;
                }
                break;
            }
        }
        return found;
    }

    static std::string name() { return "LIPP No Prefetch"; }
};

} // namespace algorithms
} // namespace lipp
} // namespace structures
