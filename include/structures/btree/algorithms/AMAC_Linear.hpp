#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class AMACSearch_Linear {
public:
    using Key       = typename BTree::key_type;
    using Value     = typename BTree::data_type;
    using Node      = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode  = typename BTree::leaf_node;

private:
    template<typename NodeType>
    static inline int linear_lower_bound(const NodeType* n, const Key& key) {
        for (int s = 0; s < n->slotuse; ++s) {
            if (key <= n->slotkey[s]) return s;
        }
        return n->slotuse;
    }

    enum class state_t { INIT, SEARCH_INNER, SEARCH_LEAF, DONE };

    struct fsm_shared_t {  
        state_t state; 
        size_t  i;       
        const Node *v;   
    };

public:
    template<size_t POOL_SIZE = 64, size_t IGNORED = 0>
    static std::vector<bool> batch_lookup(
        BTree&                  btree,
        const std::vector<Key>& queries,
        std::vector<Value>&     results
    ) {
        size_t n = queries.size();
        std::vector<bool> found(n, false);
        results.resize(n); 

        auto m_root = btree.get_root();
        if (!m_root || n == 0) return found;

        fsm_shared_t pool[POOL_SIZE];
        size_t next_query_idx = 0;

        for (size_t k = 0; k < POOL_SIZE; ++k) {
            if (next_query_idx < n) {
                pool[k].state = state_t::INIT;
                pool[k].i = next_query_idx++;
            } else {
                pool[k].state = state_t::DONE;
            }
        }

        size_t all_done = 0;
        size_t k = 0;

        while (all_done < n) {
            k = (k == POOL_SIZE) ? 0 : k; 

            if (pool[k].state == state_t::DONE && next_query_idx < n) {
                pool[k].state = state_t::INIT;
                pool[k].i = next_query_idx++;
            }

            switch (pool[k].state) {
                case state_t::INIT: {
                    pool[k].v = m_root;
                    __builtin_prefetch(m_root, 0, 3);
                    pool[k].state = state_t::SEARCH_INNER;
                    break;
                }

                case state_t::SEARCH_INNER: {
                    const Node *current_node = pool[k].v;
                    if (!current_node->isleafnode()) { 
                        const InnerNode *inner = static_cast<const InnerNode *>(current_node);
                        int pos = linear_lower_bound(inner, queries[pool[k].i]); 
                        pool[k].v = inner->childid[pos]; 
                        
                        __builtin_prefetch(pool[k].v, 0, 3);
                        __builtin_prefetch(reinterpret_cast<const char*>(pool[k].v) + 64, 0, 3);
                    } else {                            
                        pool[k].state = state_t::SEARCH_LEAF;
                    }
                    break;
                }

                case state_t::SEARCH_LEAF: { 
                    const LeafNode *leaf = static_cast<const LeafNode *>(pool[k].v);
                    Key key = queries[pool[k].i];
                   
                    int pos = linear_lower_bound(leaf, key); 

                    if (pos < leaf->slotuse && key == leaf->slotkey[pos]) {
                        if (!BTree::traits::selfverify) {
                            results[pool[k].i] = leaf->slotdata[pos];
                        }
                        found[pool[k].i] = true;
                    }

                    pool[k].state = state_t::DONE;
                    ++all_done;
                    break;
                }

                case state_t::DONE:
                    break;
            }
            ++k; 
        }

        return found;
    }

    static std::string name() { return "FSM_AMAC (Linear)"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures