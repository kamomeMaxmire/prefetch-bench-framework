#pragma once

#include <iostream>
#include <iomanip>
#include <cstddef>

namespace structures {
namespace btree {

/**
 * B+ 树统计信息
 */
struct TreeStatistics {
    size_t total_keys;
    size_t leaf_nodes;
    size_t inner_nodes; 
    size_t leafslot_max;
    size_t innerslot_max;
    size_t tree_height;  // 新增：树高
    
    /**
     * 从 B+ 树中收集统计信息
     */
    template<typename BTree>
    static TreeStatistics collect(const BTree& btree) {
        TreeStatistics stat;
        auto stx_stats = btree.get_stats();
        
        stat.total_keys = stx_stats.itemcount;
        stat.leaf_nodes = stx_stats.leaves;
        stat.inner_nodes = stx_stats.innernodes;
        stat.leafslot_max = BTree::leafslotmax;
        stat.innerslot_max = BTree::innerslotmax;
        
        // 计算树高：通过节点数量估算
        stat.tree_height = calculate_tree_height(stx_stats.innernodes, stx_stats.leaves);
        
        return stat;
    }
    
    /**
     * 计算树高
     * 树高 = 从根到叶子的层数
     */
    static size_t calculate_tree_height(size_t inner_nodes, size_t leaf_nodes) {
        if (leaf_nodes == 0) return 0;
        if (inner_nodes == 0) return 1;  // 只有根节点（是叶子）
        
        // 粗略估算：log_fanout(leaf_nodes)
        // 假设平均 fanout（分支因子）约为 slot_max / 2
        // 这里简化为：如果有内部节点，树高至少为2
        // 更精确的计算需要遍历树
        
        size_t height = 1;  // 根节点
        size_t current_level_nodes = 1;
        size_t remaining_leaves = leaf_nodes;
        
        // 假设平均 fanout = 16（保守估计）
        const size_t avg_fanout = 16;
        
        while (remaining_leaves > current_level_nodes) {
            height++;
            current_level_nodes *= avg_fanout;
        }
        
        return height;
    }
    
    /**
     * 打印统计信息
     */
    void print() const {
        std::cout << "\n  ════════════════════════════════════════════════════" << std::endl;
        std::cout << "  Tree Structure Statistics" << std::endl;
        std::cout << "  ════════════════════════════════════════════════════" << std::endl;
        
        auto print_stat = [](const std::string& name, auto value, const std::string& note = "") {
            std::cout << "    " << std::left << std::setw(30) << name 
                      << std::right << std::setw(15) << value;
            if (!note.empty()) {
                std::cout << " " << note;
            }
            std::cout << std::endl;
        };
        
        print_stat("Total Keys:", total_keys);
        print_stat("Leaf Nodes:", leaf_nodes);
        print_stat("Inner Nodes:", inner_nodes);   
        print_stat("Estimated Tree Height:", tree_height, "levels");
        std::cout << "    " << std::left << std::setw(30) << "Leaf Slot Max:" 
                  << std::right << std::setw(15) << leafslot_max << std::endl;
        std::cout << "    " << std::left << std::setw(30) << "Inner Slot Max:" 
                  << std::right << std::setw(15) << innerslot_max << std::endl;
        
        // 计算填充率
        double avg_fill = (double)total_keys / (leaf_nodes * leafslot_max);
        std::cout << "    " << std::left << std::setw(30) << "Avg Leaf Fill Rate:" 
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) 
                  << (avg_fill * 100.0) << " %" << std::endl;
        
        std::cout << "  ════════════════════════════════════════════════════" << std::endl;
    }
};

/**
 * 带深度追踪的查找包装器
 * 在查找过程中统计访问的节点层数
 */
template<typename BTree>
class DepthTrackingSearch {
public:
    using iterator = typename BTree::iterator;
    using key_type = typename BTree::key_type;
    
    /**
     * 执行查找并追踪深度
     * 注意：这需要修改 btree.h 中的 find() 实现
     */
    static iterator find_with_depth(BTree& tree, const key_type& key, size_t& depth) {
        depth = 0;
        // 这里需要访问 B+ 树的内部结构
        // 由于 STX B+ Tree 的限制，我们在外部统计
        auto result = tree.find(key);
        
        // 粗略估算：基于树的统计信息
        auto stats = tree.get_stats();
        if (stats.innernodes > 0) {
            depth = TreeStatistics::calculate_tree_height(stats.innernodes, stats.leaves);
        } else {
            depth = 1;
        }
        
        return result;
    }
};

} // namespace btree
} // namespace structures