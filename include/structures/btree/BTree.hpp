#pragma once

#include <stx/btree_map.h>
/**
 * B+ 树封装接口
 * 
 * 这个文件封装了 STX B+ Tree 的使用，
 * 并提供统一的接口供测试框架调用
 */

namespace structures {
namespace btree {

/**
 * 默认的 B+ 树类型
 * 使用 STX B+ Tree 的默认配置
 */
template<typename Key = uint64_t, typename Value = uint64_t>
using BTreeDefault = stx::btree_map<Key, Value>;

/**
 * 小节点配置的 B+ 树
 */
template<typename Key = uint64_t, typename Value = uint64_t>
struct SmallNodeBTree {
    struct traits {
        static const bool selfverify = false;
        static const bool debug = false;
        static const int leafslots = 32;
        static const int innerslots = 32;
        static const size_t binsearch_threshold = 256;
    };
    
    using type = stx::btree_map<Key, Value, std::less<Key>, traits>;
};

/**
 * 大节点配置的 B+ 树
 */
template<typename Key = uint64_t, typename Value = uint64_t>
struct LargeNodeBTree {
    struct traits {
        static const bool selfverify = false;
        static const bool debug = false;
        static const int leafslots = 128;
        static const int innerslots = 128;
        static const size_t binsearch_threshold = 256;
    };
    
    using type = stx::btree_map<Key, Value, std::less<Key>, traits>;
};

/**
 * 自定义配置的 B+ 树
 */
template<typename Key = uint64_t, typename Value = uint64_t, 
         int LeafSlots = 64, int InnerSlots = 64>
struct CustomBTree {
    struct traits {
        static const bool selfverify = false;
        static const bool debug = false;
        static const int leafslots = LeafSlots;
        static const int innerslots = InnerSlots;
        static const size_t binsearch_threshold = 256;
    };
    
    using type = stx::btree_map<Key, Value, std::less<Key>, traits>;
};

/**
 * B+ 树类型别名，方便使用
 */
using BTree64 = BTreeDefault<uint64_t, uint64_t>;
using BTree32 = BTreeDefault<uint32_t, uint32_t>;

using BTreeSmall = typename SmallNodeBTree<uint64_t, uint64_t>::type;
using BTreeLarge = typename LargeNodeBTree<uint64_t, uint64_t>::type;

} // namespace btree
} // namespace structures

/**
 * 使用示例：
 * 
 * // 1. 使用默认配置
 * structures::btree::BTree64 btree;
 * 
 * // 2. 使用小节点配置
 * structures::btree::BTreeSmall btree;
 * 
 * // 3. 使用自定义配置
 * using MyBTree = structures::btree::CustomBTree<uint64_t, uint64_t, 256, 256>::type;
 * MyBTree btree;
 * 
 * // 4. 插入和查询
 * btree.insert(std::make_pair(key, value));
 * auto it = btree.find(key);
 * if (it != btree.end()) {
 *     // 找到了
 * }
 */