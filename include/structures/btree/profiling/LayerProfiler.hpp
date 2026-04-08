#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>

namespace structures {
namespace btree {
namespace profiling {

// =======================================================================
// RDTSC：直接读 CPU 时钟计数器，精度 = 单个时钟周期
// =======================================================================
static inline uint64_t rdtsc_begin() {
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ volatile("mfence\n\t lfence\n\t rdtsc"
                     : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

static inline uint64_t rdtsc_end() {
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ volatile("rdtscp\n\t lfence"
                     : "=a"(lo), "=d"(hi) :: "rcx", "memory");
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

// =======================================================================
// 1. 扩充版状态统计结构体
// =======================================================================
struct LayerStat {
    uint64_t cycles = 0;       // 总消耗时钟周期
    uint64_t count = 0;        // 经过该层的查询总数
    
    // 【新增的纯软件统计指标】
    uint64_t node_access = 0;  // 该层节点被访问的总次数
    uint64_t key_compare = 0;  // 该层发生的 key 比较总次数

    void record(uint64_t c) {
        cycles += c;
        count++;
    }

    double avg_cycles() const { return count ? (double)cycles / count : 0.0; }
    double avg_cmp() const    { return node_access ? (double)key_compare / node_access : 0.0; }
};

constexpr int MAX_LEVELS = 16;

// =======================================================================
// 2. 打印功能升级版
// =======================================================================
static void print_stats(const std::string& name, LayerStat stats[], int max_level, double cpu_ghz) {
    std::cout << "\n=== Per-Layer Profile (" << name << ") ===\n";
    std::cout << std::left 
              << std::setw(8)  << "Level" 
              << std::setw(12) << "Avg(cycles)" 
              << std::setw(12) << "Avg(ns)" 
              << std::setw(12) << "Accesses" 
              << std::setw(14) << "AvgCmp/Node" 
              << "Samples\n";
    std::cout << std::string(72, '-') << "\n";

    uint64_t total_cycles = 0;
    for (int i = 0; i <= max_level; ++i) {
        const auto& s = stats[i];
        if (s.count == 0) continue;

        std::string lvl_name = (i == max_level) ? "L" + std::to_string(i) + "(leaf)" : "L" + std::to_string(i);
        
        std::cout << std::left 
                  << std::setw(8)  << lvl_name
                  << std::setw(12) << std::fixed << std::setprecision(1) << s.avg_cycles()
                  << std::setw(12) << std::fixed << std::setprecision(1) << s.avg_cycles() / cpu_ghz
                  << std::setw(12) << s.node_access
                  << std::setw(14) << std::fixed << std::setprecision(1) << s.avg_cmp()
                  << s.count << "\n";
                  
        total_cycles += (uint64_t)s.avg_cycles();
    }
    std::cout << std::string(72, '-') << "\n";
    std::cout << "Total Avg Cycles/Query: " << total_cycles << " (~" 
              << std::fixed << std::setprecision(1) << total_cycles / cpu_ghz << " ns)\n\n";
}

// =======================================================================
// 3. Profiler 主类
// =======================================================================
template<typename BTree>
class LayerProfiler {
public:
    using Key = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Node = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode = typename BTree::leaf_node;

    enum Mode {
        MODE_SERIAL = 0 // 目前先只保留 No Prefetch 的剖析
    };

private:
    // 【新增】：带有比较次数统计的二分查找
    template <typename NodeType>
    static inline int find_lower_count(const NodeType* n, const Key& key, uint64_t& cmp_cnt) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            cmp_cnt++;   // 统计比较次数
            if (key <= n->slotkey[mid]) hi = mid;
            else lo = mid + 1;
        }
        return lo;
    }

    // ==================================================================
    // 完美捕获跨层 Cache Miss 并在内部记录统计信息的 run_serial
    // ==================================================================
    static void run_serial(BTree& btree, const std::vector<Key>& queries,
                           size_t n, const std::string& label, double ghz) {
        auto root = btree.get_root();
        LayerStat stats[MAX_LEVELS];
        int max_level = 0;

        for (size_t q = 0; q < n; ++q) {
            const Key& key = queries[q];
            const Node* node = root;
            int lv = 0;

            uint64_t t0 = rdtsc_begin();

            while (!node->isleafnode()) {
                const InnerNode* in = static_cast<const InnerNode*>(node);
                
                // 记录：进入该层节点
                stats[lv].node_access++;

                uint64_t cmp = 0;
                int slot = find_lower_count(in, key, cmp);
                
                // 记录：本层的比较次数
                stats[lv].key_compare += cmp;

                // 结算本层时间 (包含跨层指针解引用的 Miss + 二分查找)
                uint64_t t1 = rdtsc_end();
                stats[lv].record(t1 - t0);
                if (lv > max_level) max_level = lv;
                node = in->childid[slot];
                 t0 = rdtsc_begin();
                ++lv;
            }

            // --- 叶子节点层 ---
            const LeafNode* lf = static_cast<const LeafNode*>(node);
            stats[lv].node_access++;

            uint64_t cmp = 0;
            volatile int s = find_lower_count(lf, key, cmp); (void)s;
            stats[lv].key_compare += cmp;

            uint64_t t1 = rdtsc_end();
            stats[lv].record(t1 - t0);
            if (lv > max_level) max_level = lv;
        }

        print_stats(label, stats, max_level, ghz);
    }

public:
    static void run(BTree& btree, 
                    const std::vector<Key>& queries,
                    const std::string& label,
                    Mode mode,
                    size_t param,
                    double ghz) {
        if (mode == MODE_SERIAL) {
            run_serial(btree, queries, queries.size(), label, ghz);
        }
    }
};

} // namespace profiling
} // namespace btree
} // namespace structures