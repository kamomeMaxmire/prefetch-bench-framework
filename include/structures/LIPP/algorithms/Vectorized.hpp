#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace structures {
namespace lipp {
namespace algorithms {

template<typename LIPPMap>
class VectorizedSearch {
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

    template<size_t VECTOR_SIZE = 64>
    static std::vector<bool> batch_lookup(
        LIPPMap& lipp,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size(), false);
        results.resize(queries.size());

        const Index& index = lipp.get_index();
        if (!index.root || queries.empty()) return found;

        const Node* nodes[VECTOR_SIZE];
        int pos[VECTOR_SIZE];
        bool active[VECTOR_SIZE];

        for (size_t batch_start = 0; batch_start < queries.size(); batch_start += VECTOR_SIZE) {
            const size_t batch_size = std::min(VECTOR_SIZE, queries.size() - batch_start);
            const Key* batch_queries = queries.data() + batch_start;
            size_t remaining = batch_size;

            for (size_t i = 0; i < batch_size; ++i) {
                nodes[i] = index.root;
                active[i] = true;
            }

            while (remaining > 0) {
                for (size_t i = 0; i < batch_size; ++i) {
                    if (active[i]) pos[i] = predict_pos(nodes[i], batch_queries[i]);
                }

                for (size_t i = 0; i < batch_size; ++i) {
                    if (!active[i]) continue;

                    const Node* node = nodes[i];
                    const int p = pos[i];
                    const size_t q_idx = batch_start + i;

                    if (BITMAP_GET(node->none_bitmap, p) == 1) {
                        active[i] = false;
                        --remaining;
                    } else if (BITMAP_GET(node->child_bitmap, p) == 1) {
                        nodes[i] = node->items[p].comp.child;
                    } else {
                        if (node->items[p].comp.data.key == batch_queries[i]) {
                            results[q_idx] = node->items[p].comp.data.value;
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

    static std::string name() { return "LIPP Vectorized"; }
};

} // namespace algorithms
} // namespace lipp
} // namespace structures
