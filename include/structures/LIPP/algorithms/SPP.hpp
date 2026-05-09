#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace structures {
namespace lipp {
namespace algorithms {

template<typename LIPPMap>
class SoftwarePipelinedPrefetch {
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

    template<size_t PIPELINE_DEPTH = 64>
    static std::vector<bool> batch_lookup(
        LIPPMap& lipp,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size(), false);
        results.resize(queries.size());

        const Index& index = lipp.get_index();
        if (!index.root || queries.empty()) return found;

        struct Task {
            size_t query_idx;
            const Node* node;
            bool active;
        };

        Task tasks[PIPELINE_DEPTH];
        size_t next_query_idx = 0;
        size_t finished = 0;
        const size_t total = queries.size();

        for (size_t i = 0; i < PIPELINE_DEPTH; ++i) {
            if (next_query_idx < total) {
                tasks[i] = {next_query_idx++, index.root, true};
                __builtin_prefetch(tasks[i].node, 0, 1);
            } else {
                tasks[i] = {0, nullptr, false};
            }
        }

        while (finished < total) {
            for (size_t i = 0; i < PIPELINE_DEPTH; ++i) {
                if (!tasks[i].active) {
                    if (next_query_idx < total) {
                        tasks[i] = {next_query_idx++, index.root, true};
                        __builtin_prefetch(tasks[i].node, 0, 1);
                    } else {
                        continue;
                    }
                }

                const size_t q_idx = tasks[i].query_idx;
                const Key& key = queries[q_idx];
                const Node* node = tasks[i].node;
                int pos = predict_pos(node, key);

                if (BITMAP_GET(node->none_bitmap, pos) == 1) {
                    tasks[i].active = false;
                    ++finished;
                } else if (BITMAP_GET(node->child_bitmap, pos) == 1) {
                    tasks[i].node = node->items[pos].comp.child;
                    __builtin_prefetch(tasks[i].node, 0, 1);
                } else {
                    if (node->items[pos].comp.data.key == key) {
                        results[q_idx] = node->items[pos].comp.data.value;
                        found[q_idx] = true;
                    }
                    tasks[i].active = false;
                    ++finished;
                }
            }
        }
        return found;
    }

    static std::string name() { return "LIPP SPP"; }
};

} // namespace algorithms
} // namespace lipp
} // namespace structures
