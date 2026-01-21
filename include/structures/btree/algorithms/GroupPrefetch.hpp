#pragma once
#include <vector>
#include <algorithm>
#include <string>

namespace structures {
namespace btree {
namespace algorithms {

template<typename BTree>
class GroupPrefetch {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    
    // 修改后的 batch_lookup 直接调用 BTree 内部实现的 find_group
    template<size_t GROUP_SIZE = 32>
    static std::vector<bool> batch_lookup(
        BTree& btree,
        const std::vector<Key>& queries,
        std::vector<Value>& results
    ) {
        std::vector<bool> found(queries.size());
        results.resize(queries.size());
        
        // 调用我们刚刚在 BTree 类里添加的函数
        // 注意：你需要确保 BTree 类也就是 stx::btree_map 暴露了这个接口
        // 如果你用的是 BTreeWithPrefetch 包装器，请确保那里也转发了这个调用
        btree.template find_group<GROUP_SIZE>(queries, results, found);
        
        return found;
    }
    
    template<size_t GROUP_SIZE = 32>
    static std::string name() {
        return "Group Prefetch (G=" + std::to_string(GROUP_SIZE) + ")";
    }
};

} // namespace algorithms
} // namespace btree
} // namespace structures