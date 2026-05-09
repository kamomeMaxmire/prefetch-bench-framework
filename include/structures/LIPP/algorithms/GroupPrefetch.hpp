#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace structures {
namespace lipp {
namespace algorithms {

template<typename LIPPMap>
class GroupPrefetch {
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

    template<size_t GROUP_SIZE = 64>
    static std::vector<bool> batch_lookup(
        LIPPMap& lipp,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size(), false);
        results.resize(queries.size());

        const Index& index = lipp.get_index();
        if (!index.root || queries.empty()) return found;

        const Node* nodes[GROUP_SIZE];
        bool active[GROUP_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += GROUP_SIZE) {
            const size_t batch_size = std::min(GROUP_SIZE, queries.size() - batch_start);
            size_t remaining = batch_size;

            for (size_t i = 0; i < batch_size; ++i) {
                nodes[i] = index.root;
                active[i] = true;
                __builtin_prefetch(nodes[i], 0, 1);
            }

            while (remaining > 0) {
                for (size_t i = 0; i < batch_size; ++i) {
                    if (!active[i]) continue;

                    const size_t q_idx = batch_start + i;
                    const Key& key = queries[q_idx];
                    const Node* node = nodes[i];
                    int pos = predict_pos(node, key);

                    if (BITMAP_GET(node->none_bitmap, pos) == 1) {
                        active[i] = false;
                        --remaining;
                    } else if (BITMAP_GET(node->child_bitmap, pos) == 1) {
                        nodes[i] = node->items[pos].comp.child;
                        __builtin_prefetch(nodes[i], 0, 1);
                    } else {
                        if (node->items[pos].comp.data.key == key) {
                            results[q_idx] = node->items[pos].comp.data.value;
                            found[q_idx] = true;
                        }
                        active[i] = false;
                        --remaining;
                    }
                }
            }
        }
        return found;
    }

    static std::string name() { return "LIPP Group Prefetch"; }
};

} // namespace algorithms
} // namespace lipp
} // namespace structures
