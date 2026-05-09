#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class AMACSearch {
public:
    using Key       = typename BTree::key_type;
    using Value     = typename BTree::data_type;
    using Node      = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode  = typename BTree::leaf_node;

private:
    template<typename NodeType>
    static inline int find_lower(const NodeType* n, const Key& key) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (key <= n->slotkey[mid]) hi = mid;
            else lo = mid + 1;
        }
        return lo;
    }

    enum class state_t { INIT, SEARCH, DONE };

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

        // init pool
        for (size_t k = 0; k < POOL_SIZE; ++k) {
            if (next_query_idx < n) {
                pool[k].state = state_t::INIT;
                pool[k].i = next_query_idx++;
            } else {
                pool[k].state = state_t::DONE;
            }
        }

        size_t all_done = 0;

        while (all_done < n) {
            // 内部采用标准 for 循环，消灭取模和分支跳转开销，极大提升分支预测命中率
            for (size_t k = 0; k < POOL_SIZE; ++k) {
                // 即时 Refill：一旦 DONE 立刻装填新查询
                if (pool[k].state == state_t::DONE && next_query_idx < n) {
                    pool[k].state = state_t::INIT;
                    pool[k].i = next_query_idx++;
                }

                switch (pool[k].state) {
                    case state_t::INIT: {
                        pool[k].v = m_root;
                        __builtin_prefetch(m_root, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(m_root) + 64, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(m_root) + 128, 0, 1);
                        __builtin_prefetch(reinterpret_cast<const char*>(m_root) + 192, 0, 1);

                        pool[k].state = state_t::SEARCH;
                        break;
                    }

                    case state_t::SEARCH: {
                        const Node *current_node = pool[k].v;
                        if (!current_node->isleafnode()) { 
                            const InnerNode *inner = static_cast<const InnerNode *>(current_node);
                            int pos = find_lower(inner, queries[pool[k].i]);
                            pool[k].v = inner->childid[pos]; 
                            
                            // 发射内存预取并保留在 SEARCH 状态，等待下一次 FSM 轮询
                            __builtin_prefetch(pool[k].v, 0, 1);
                            __builtin_prefetch(reinterpret_cast<const char*>(pool[k].v) + 64, 0, 1);
                            __builtin_prefetch(reinterpret_cast<const char*>(pool[k].v) + 128, 0, 1);
                            __builtin_prefetch(reinterpret_cast<const char*>(pool[k].v) + 192, 0, 1);
                        } else {                            
                            // 已经是叶子节点，上一个 FSM 周期已完成了预取，直接收集结果并 DONE
                            const LeafNode *leaf = static_cast<const LeafNode *>(current_node);
                            Key key = queries[pool[k].i];
                            int pos = find_lower(leaf, key);

                            if (pos < leaf->slotuse && key == leaf->slotkey[pos]) {
                                if (!BTree::traits::selfverify) results[pool[k].i] = leaf->slotdata[pos];
                                found[pool[k].i] = true;
                            }

                            pool[k].state = state_t::DONE;
                            ++all_done;
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

    static std::string name() { return "FSM_AMAC"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures
