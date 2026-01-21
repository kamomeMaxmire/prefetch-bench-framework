/**
 * B+ Tree Prefetch Strategy Benchmark
 * Data Source: SOSD books_200M (File)
 * Logic: Multi-run Average for stability
 */

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <random>
#include <functional> // for std::function

#include "../include/utils/Timer.hpp"
#include "../include/data/SOSDDataLoader.hpp"
#include "../include/structures/btree/BTree.hpp"

using namespace utils;
using namespace data;
using namespace structures::btree;

// ================== 链接修复 ==================
// 在这里真正“定义”变量，防止链接错误
namespace stx {
    size_t g_tree_height = 0; 
}
// =============================================

struct TestResult {
    std::string strategy_name;
    double avg_time_ms;     // 平均耗时
    double min_time_ms;     // 最小耗时
    double max_time_ms;     // 最大耗时
    double query_throughput; // 基于平均耗时
    double avg_latency_ns;   // 基于平均耗时
};

// 全局配置：每种策略跑几次
const int NUM_RUNS = 5; 

// 辅助函数：运行单个策略多次并取平均值
template<typename Func>
TestResult run_benchmark(
    const std::string& name, 
    Func strategy_func, 
    size_t query_count
) {
    std::cout << "  Testing " << std::left << std::setw(25) << name << "... " << std::flush;
    
    std::vector<double> times;
    times.reserve(NUM_RUNS);
    
    // 1. Warmup (热身跑，不计入成绩)
    // 预热 CPU 指令缓存 (I-Cache) 和 分支预测器
    {
        std::vector<uint64_t> dummy_values;
        strategy_func(dummy_values);
    }
    
    // 2. 正式运行 NUM_RUNS 次
    for (int i = 0; i < NUM_RUNS; ++i) {
        std::vector<uint64_t> values;
        
        Timer t;
        auto found = strategy_func(values); // 执行查询
        double elapsed = t.elapsed_ms();
        
        times.push_back(elapsed);
    }
    
    // 3. 计算统计数据
    double sum = 0;
    double min_t = times[0];
    double max_t = times[0];
    
    for (double t : times) {
        sum += t;
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
    }
    double avg_time = sum / NUM_RUNS;
    
    // 4. 计算吞吐量和延迟
    double throughput = (query_count / avg_time) * 1000.0 / 1e6; // M ops/s
    double latency = (avg_time * 1e6) / query_count; // ns
    
    std::cout << "Done (Avg: " << std::fixed << std::setprecision(2) << avg_time << " ms)" << std::endl;
    
    return { name, avg_time, min_t, max_t, throughput, latency };
}

void test_strategies(
    const std::vector<uint64_t>& keys, 
    const std::vector<uint64_t>& queries
) {
    std::vector<TestResult> results;
    
    // 配置树参数 (SlotSize 32 是比较通用的优化值)
    constexpr int SlotSize = 32; 
    using BTreeType = typename CustomBTree<uint64_t, uint64_t, SlotSize, SlotSize>::type;
    BTreeWithPrefetch<BTreeType> btree;
    
    std::cout << "\n  ┌─ Building B+ Tree ────────────────────────────────────────" << std::endl;
    
    std::vector<std::pair<uint64_t, uint64_t>> data;
    data.reserve(keys.size());
    for (const auto& key : keys) {
        data.emplace_back(key, key); 
    }
    
    Timer build_timer;
    btree.bulk_load(data.begin(), data.end());
    double build_time = build_timer.elapsed_ms();
    
    auto stats = btree.get_stats();

    // 测量真实树高
    size_t tree_height = 0;
    stx::g_tree_height = 0; 
    if (!queries.empty()) {
        btree.get_tree().find(queries[0]); 
        tree_height = stx::g_tree_height;
    }

    std::cout << "  │  Build Time:    " << std::fixed << std::setprecision(2) << build_time << " ms" << std::endl;
    std::cout << "  │  Tree Height:   " << tree_height << " levels" << std::endl;
    std::cout << "  │  Total Keys:    " << stats.itemcount << std::endl;
    std::cout << "  └───────────────────────────────────────────────────────────" << std::endl;
    
    std::cout << "\n  Running Benchmark (" << NUM_RUNS << " runs per strategy)..." << std::endl;
    // 使用 '-' 而不是特殊字符，避免编译警告
    std::cout << "  " << std::string(60, '-') << std::endl;
    
    // --- 1. Baseline: No Prefetch ---
    results.push_back(run_benchmark(
        "No Prefetch", 
        [&](std::vector<uint64_t>& v){ return btree.batch_lookup_no_prefetch(queries, v); }, 
        queries.size()
    ));
    
    // --- 2. Group Prefetch (G=16) ---
    results.push_back(run_benchmark(
        "Group Prefetch (G=16)", 
        [&](std::vector<uint64_t>& v){ return btree.batch_lookup_group_prefetch<16>(queries, v); }, 
        queries.size()
    ));
    
    // --- 3. Group Prefetch (G=32) ---
    results.push_back(run_benchmark(
        "Group Prefetch (G=32)", 
        [&](std::vector<uint64_t>& v){ return btree.batch_lookup_group_prefetch<32>(queries, v); }, 
        queries.size()
    ));
    
    // --- 4. Group Prefetch (G=64) ---
    results.push_back(run_benchmark(
        "Group Prefetch (G=64)", 
        [&](std::vector<uint64_t>& v){ return btree.batch_lookup_group_prefetch<64>(queries, v); }, 
        queries.size()
    ));

    // --- Output Table ---
    std::cout << "\n╔══════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                     Benchmark Results (Avg of " << NUM_RUNS << " runs)                    ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Strategy                 Avg Time(ms)   Latency(ns)   Throughput(M)   Speedup║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    double baseline_latency = results[0].avg_latency_ns;
    
    for (const auto& r : results) {
        double speedup = (baseline_latency - r.avg_latency_ns) / baseline_latency * 100.0;
        
        std::cout << "║ " << std::left << std::setw(23) << r.strategy_name 
                  << std::right 
                  << std::setw(11) << std::fixed << std::setprecision(1) << r.avg_time_ms
                  << std::setw(14) << std::setprecision(1) << r.avg_latency_ns
                  << std::setw(15) << std::setprecision(2) << r.query_throughput;
                  
        if (r.strategy_name == "No Prefetch") {
             std::cout << "        -    ║" << std::endl;
        } else {
             std::cout << std::setw(9) << std::setprecision(1) << speedup << "%  ║" << std::endl;
        }
    }
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝" << std::endl;
}

int main() {
    std::cout << "\n=== B+ Tree Group Prefetch Benchmark ===" << std::endl;
    
    // 1. 加载数据集
    const std::string dataset_name = "books_200M"; 
    std::string data_file = "data/" + dataset_name + "_uint64";
    
    std::cout << "[Loading] Reading " << data_file << "..." << std::endl;
    // 使用 SOSDDataLoader 加载二进制文件
    auto keys = SOSDDataLoader::load_binary_file(data_file);
    
    if (keys.empty()) {
        std::cerr << "Error: Could not load keys. Please check path: " << data_file << std::endl;
        return 1;
    }
    std::cout << "    Loaded " << keys.size() << " keys." << std::endl;

    // 确保 key 有序 (STX B+ Tree Bulk Load 的要求)
    if (!std::is_sorted(keys.begin(), keys.end())) {
        std::cout << "    Sorting keys..." << std::endl;
        std::sort(keys.begin(), keys.end());
    }
    
    // 2. 生成查询
    // 这里的 query_count 可以调整，100万比较快，1000万更稳定
    const size_t query_count = 1000000; 
    std::cout << "[Queries] Generating " << query_count << " queries from dataset..." << std::endl;
    auto queries = SOSDDataLoader::generate_queries(keys, query_count);

    // 3. 运行测试
    test_strategies(keys, queries);
    
    return 0;
}