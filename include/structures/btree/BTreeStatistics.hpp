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
        
       
        return stat;
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
        std::cout << "    " << std::left << std::setw(30) << "Leaf Slot Max:" 
                  << std::right << std::setw(15) << leafslot_max << std::endl;
        std::cout << "    " << std::left << std::setw(30) << "Inner Slot Max:" 
                  << std::right << std::setw(15) << innerslot_max << std::endl;
        std::cout << "  ════════════════════════════════════════════════════" << std::endl;
    }
};

} // namespace btree
} // namespace structures