#pragma once
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>
#include <type_traits>

#if defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif

namespace structures {
namespace btree {
namespace algorithms {

// Vectorized Search 
template<typename BTree>
class VectorizedSearch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    static constexpr int calc_steps(int max_slots) {
        int steps = 0, capacity = 1;
        while (capacity <= max_slots) {
            capacity *= 2;
            steps++;
        }
        return steps;
    }
    
    static constexpr int INNER_STEPS = calc_steps(BTree::innerslotmax);
    static constexpr int LEAF_STEPS  = calc_steps(BTree::leafslotmax);

    template <typename NodeType>
    static inline int find_lower(const NodeType* n, const Key& key) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            bool cmp = key > n->slotkey[mid];
            lo = cmp ? mid + 1 : lo;
            hi = cmp ? hi : mid;
        }
        return lo;
    }

    template <typename NodeType>
    static inline int simd_lower_bound_u64(const NodeType* n, const Key& key) {
        const int slotuse = n->slotuse;
        if (slotuse == 0) return 0;

        if constexpr (!std::is_same<Key, uint64_t>::value) {
            return find_lower(n, key);
        } else {
            const uint64_t* keys = n->slotkey;
            int pos = 0;

#if defined(__AVX512F__)
            const __m512i sign = _mm512_set1_epi64(static_cast<long long>(0x8000000000000000ULL));
            const __m512i query = _mm512_xor_si512(
                _mm512_set1_epi64(static_cast<long long>(key)),
                sign
            );

            for (; pos + 8 <= slotuse; pos += 8) {
                __m512i values = _mm512_loadu_si512(reinterpret_cast<const void*>(keys + pos));
                values = _mm512_xor_si512(values, sign);

                // first slot where key <= slotkey[pos], implemented as !(key > slotkey[pos])
                const __mmask8 greater = _mm512_cmpgt_epi64_mask(query, values);
                const uint32_t candidate_mask = static_cast<uint32_t>((~greater) & 0xFF);
                if (candidate_mask) {
                    return pos + __builtin_ctz(candidate_mask);
                }
            }
#elif defined(__AVX2__)
            const __m256i sign = _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ULL));
            const __m256i query = _mm256_xor_si256(
                _mm256_set1_epi64x(static_cast<long long>(key)),
                sign
            );

            for (; pos + 4 <= slotuse; pos += 4) {
                __m256i values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys + pos));
                values = _mm256_xor_si256(values, sign);

                // first slot where key <= slotkey[pos], implemented as !(key > slotkey[pos])
                const __m256i greater = _mm256_cmpgt_epi64(query, values);
                const uint32_t greater_mask = static_cast<uint32_t>(_mm256_movemask_pd(_mm256_castsi256_pd(greater)));
                const uint32_t candidate_mask = (~greater_mask) & 0xF;
                if (candidate_mask) {
                    return pos + __builtin_ctz(candidate_mask);
                }
            }
#endif

            for (; pos < slotuse; ++pos) {
                if (key <= keys[pos]) return pos;
            }
            return slotuse;
        }
    }

    template<size_t VECTOR_SIZE = 64>
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());

        auto root = btree.get_root();
        if (!root || queries.empty()) return found;

        const size_t total_queries = queries.size();
        
        const Node* curr_nodes[VECTOR_SIZE];
        int slots[VECTOR_SIZE];

        for (size_t batch_start = 0; batch_start < total_queries; batch_start += VECTOR_SIZE) {
            
            size_t current_batch_size = std::min(VECTOR_SIZE, total_queries - batch_start);
            
            // 提取裸指针，保证内存访问绝对连续，帮助编译器生成最紧凑的加载指令
            const Key* batch_queries = queries.data() + batch_start;

            // 1. init vector with root pointers
            for (size_t i = 0; i < current_batch_size; ++i) {
                curr_nodes[i] = root;
            }

            // 2. traverse inner nodes
            while (!curr_nodes[0]->isleafnode()) {
                for (size_t i = 0; i < current_batch_size; ++i) {
                    const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]);
                    slots[i] = simd_lower_bound_u64(inner, batch_queries[i]);
                }
                for (size_t i = 0; i < current_batch_size; ++i) {
                    const InnerNode* inner = static_cast<const InnerNode*>(curr_nodes[i]);
                    curr_nodes[i] = inner->childid[slots[i]]; 
                }
            }

            // 3. handle leaf nodes and gather results
            for (size_t i = 0; i < current_batch_size; ++i) {
                const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                slots[i] = simd_lower_bound_u64(leaf, batch_queries[i]);
            }
            for (size_t i = 0; i < current_batch_size; ++i) {
                const LeafNode* leaf = static_cast<const LeafNode*>(curr_nodes[i]);
                int slot = slots[i];
                Key key = batch_queries[i];
                
                if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                    if (!BTree::traits::selfverify) 
                        results[batch_start + i] = leaf->slotdata[slot];
                    found[batch_start + i] = true;
                } else {
                    found[batch_start + i] = false;
                }
            }
        }

        return found;
    }

    static std::string name() {
        return "Vectorized";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures
