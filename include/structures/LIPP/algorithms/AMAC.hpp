#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace structures {
namespace lipp {
namespace algorithms {

template<typename LIPPMap>
class AMACSearch {
public:
    using Key = typename LIPPMap::key_type;
    using Value = typename LIPPMap::data_type;
    using Index = typename LIPPMap::index_type;
    using Node = typename Index::Node;

private:
    enum class state_t { INIT, SEARCH, DONE };

    struct fsm_t {
        state_t state;
        size_t query_idx;
        const Node* node;
    };

    static inline int predict_pos(const Node* node, const Key& key) {
        double v = node->model.predict_double(key);
        if (v > std::numeric_limits<int>::max() / 2) return node->num_items - 1;
        if (v < 0) return 0;
        return std::min(node->num_items - 1, static_cast<int>(v));
    }

public:
    template<size_t POOL_SIZE = 64, size_t IGNORED = 0>
    static std::vector<bool> batch_lookup(
        LIPPMap& lipp,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size(), false);
        results.resize(queries.size());

        const Index& index = lipp.get_index();
        if (!index.root || queries.empty()) return found;

        fsm_t pool[POOL_SIZE];
        size_t next_query_idx = 0;
        const size_t total = queries.size();

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            if (next_query_idx < total) {
                pool[i].state = state_t::INIT;
                pool[i].query_idx = next_query_idx++;
                pool[i].node = nullptr;
            } else {
                pool[i].state = state_t::DONE;
                pool[i].query_idx = 0;
                pool[i].node = nullptr;
            }
        }

        size_t finished = 0;
        while (finished < total) {
            for (size_t i = 0; i < POOL_SIZE; ++i) {
                if (pool[i].state == state_t::DONE && next_query_idx < total) {
                    pool[i].state = state_t::INIT;
                    pool[i].query_idx = next_query_idx++;
                    pool[i].node = nullptr;
                }

                switch (pool[i].state) {
                    case state_t::INIT:
                        pool[i].node = index.root;
                        __builtin_prefetch(pool[i].node, 0, 1);
                        pool[i].state = state_t::SEARCH;
                        break;

                    case state_t::SEARCH: {
                        const size_t q_idx = pool[i].query_idx;
                        const Key& key = queries[q_idx];
                        const Node* node = pool[i].node;
                        int pos = predict_pos(node, key);

                        if (BITMAP_GET(node->none_bitmap, pos) == 1) {
                            pool[i].state = state_t::DONE;
                            ++finished;
                        } else if (BITMAP_GET(node->child_bitmap, pos) == 1) {
                            pool[i].node = node->items[pos].comp.child;
                            __builtin_prefetch(pool[i].node, 0, 1);
                        } else {
                            if (node->items[pos].comp.data.key == key) {
                                results[q_idx] = node->items[pos].comp.data.value;
                                found[q_idx] = true;
                            }
                            pool[i].state = state_t::DONE;
                            ++finished;
                        }
                        break;
                    }

                    case state_t::DONE:
                        break;
                }
            }
        }
        return found;
    }

    static std::string name() { return "LIPP AMAC"; }
};

} // namespace algorithms
} // namespace lipp
} // namespace structures
