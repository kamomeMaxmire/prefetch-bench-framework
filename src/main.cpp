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
#include "../include/structures/btree/profiling/LayerProfiler.hpp"

#include "../include/structures/PGM/PGMWrapper.hpp" 
#include "../include/structures/LIPP/LIPPWrapper.hpp"
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

// 全局配置：每种策略跑几次。15 次配合 trimmed mean，曲线通常比 top-k 更稳。
const int NUM_RUNS = 15; 

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
    std::sort(times.begin(), times.end());
    double min_t = times.front();
    double max_t = times.back();
    
    // 采用 trimmed mean：丢弃最快/最慢的少数运行，再平均中间样本。
    // 这比 top-k 更适合画图和写报告，因为它既能抑制系统噪声，也不偏向“挑最快结果”。
    double avg_time = 0.0;
    int trim_each_side = NUM_RUNS >= 10 ? 2 : 1;
    int first = trim_each_side;
    int last = NUM_RUNS - trim_each_side;
    if (first >= last) {
        first = 0;
        last = NUM_RUNS;
    }
    for (int i = first; i < last; ++i) {
        avg_time += times[i];
    }
    avg_time /= (last - first);
    
    // 4. 计算吞吐量和延迟
    double throughput = (query_count / avg_time) * 1000.0 / 1e6; // M ops/s
    double latency = (avg_time * 1e6) / query_count; // ns
    
    std::cout << "Done (Avg: " << std::fixed << std::setprecision(2) << avg_time << " ms)" << std::endl;
    
    return { name, avg_time, min_t, max_t, throughput, latency };
}

// =====================================================================
// 利用 C++17 折叠表达式，自动展开所有并发度 (1, 2, 4... 8192) 的测试代码
// =====================================================================
template <typename BTreeType, size_t... Ns>
struct BTreeBenchRunner {
    static void run_all(BTreeWithPrefetch<BTreeType>& btree, const std::vector<uint64_t>& queries, std::vector<TestResult>& results) {
        // --- 2. Group Prefetch ---
        (results.push_back(run_benchmark(
            "Group Prefetch (G=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return btree.template batch_lookup_group_prefetch<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 3. SPP ---
        (results.push_back(run_benchmark(
            "SPP (D=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return btree.template batch_lookup_spp<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 4. Vectorized ---
        (results.push_back(run_benchmark(
            "Vectorized (V=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return btree.template batch_lookup_vectorized<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 5. FSM AMAC ---
        (results.push_back(run_benchmark(
            "FSM AMAC (P=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return btree.template batch_lookup_fsm_amac<Ns>(queries, v); },
            queries.size()
        )), ...);
    }
};

// =====================================================================
// 利用 C++17 折叠表达式，自动展开所有并发度的 PGM 测试代码
// =====================================================================
template <typename PGMType, size_t... Ns>
struct PGMBenchRunner {
    static void run_all(PGMType& pgm_tree, const std::vector<uint64_t>& queries, std::vector<TestResult>& results) {
        // --- 2. Group Prefetch ---
        (results.push_back(run_benchmark(
            "PGM (GrpPref, G=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return pgm_tree.template batch_lookup_group_prefetch<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 3. SPP ---
        (results.push_back(run_benchmark(
            "PGM (StaticSPP, D=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return pgm_tree.template batch_lookup_static_spp<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 4. Vectorized ---
        (results.push_back(run_benchmark(
            "PGM (Vectorized, V=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return pgm_tree.template batch_lookup_vectorized<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 5. FSM AMAC ---
        (results.push_back(run_benchmark(
            "PGM (FSM AMAC, P=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return pgm_tree.template batch_lookup_fsm_amac<Ns>(queries, v); },
            queries.size()
        )), ...);
    }
};

// =====================================================================
// 利用 C++17 折叠表达式，自动展开所有并发度的 LIPP 测试代码
// =====================================================================
template <typename LIPPType, size_t... Ns>
struct LIPPBenchRunner {
    static void run_all(LIPPType& lipp, const std::vector<uint64_t>& queries, std::vector<TestResult>& results) {
        // --- 2. Group Prefetch ---
        (results.push_back(run_benchmark(
            "LIPP (GrpPref, G=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return lipp.template batch_lookup_group_prefetch<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 3. SPP ---
        (results.push_back(run_benchmark(
            "LIPP (SPP, D=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return lipp.template batch_lookup_spp<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 4. Vectorized ---
        (results.push_back(run_benchmark(
            "LIPP (Vectorized, V=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return lipp.template batch_lookup_vectorized<Ns>(queries, v); },
            queries.size()
        )), ...);

        // --- 5. FSM AMAC ---
        (results.push_back(run_benchmark(
            "LIPP (FSM AMAC, P=" + std::to_string(Ns) + ")",
            [&](std::vector<uint64_t>& v){ return lipp.template batch_lookup_fsm_amac<Ns>(queries, v); },
            queries.size()
        )), ...);
    }
};

// =====================================================================
// 利用 C++17 折叠表达式，自动展开不同 Epsilon 的 PGM 构建与基准测试
// =====================================================================
template <size_t... Eps>
struct PGMEpsilonRunner {
    static void run_all(
        const std::vector<std::pair<uint64_t, uint64_t>>& data,
        const std::vector<uint64_t>& queries, 
        std::vector<TestResult>& results
    ) {
        ([&]() {
            std::cout << "\n  ┌─ Building PGM-Index (Epsilon = " << std::left << std::setw(3) << Eps << ") ────────────────────" << std::endl;
            using PGMType = structures::pgm::PGMWrapper<uint64_t, uint64_t, Eps>;
            PGMType pgm_tree;
            
            Timer t;
            pgm_tree.bulk_load(data);
            
            std::cout << "  │  Build Time:    " << std::fixed << std::setprecision(2) << t.elapsed_ms() << " ms" << std::endl;
            std::cout << "  │  Index Size:    " << pgm_tree.index.size_in_bytes() << " Bytes (" 
                      << (pgm_tree.index.size_in_bytes() / 1024.0 / 1024.0) << " MB)" << std::endl;
            std::cout << "  └───────────────────────────────────────────────────────────" << std::endl;

            results.push_back(run_benchmark(
                "PGM (Eps=" + std::to_string(Eps) + ", NoPref)", 
                [&](std::vector<uint64_t>& v){ return pgm_tree.batch_lookup_no_prefetch(queries, v); }, 
                queries.size()
            ));
        }(), ...);
    }
};

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

    size_t leaf_size = stats.leaves * sizeof(typename BTreeType::leaf_node);
    size_t inner_size = stats.innernodes * sizeof(typename BTreeType::inner_node);
    size_t total_btree_size = leaf_size + inner_size;
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
    std::cout << "  │  Index Size:    " << total_btree_size << " Bytes (" 
              << (total_btree_size / 1024.0 / 1024.0) << " MB)" << std::endl;
    std::cout << "  └───────────────────────────────────────────────────────────" << std::endl;

    std::cout << "\n  Running Benchmark (" << NUM_RUNS << " runs per strategy)..." << std::endl;
    std::cout << "  " << std::string(60, '-') << std::endl;

    // --- 1. Baseline ---
    results.push_back(run_benchmark("No Prefetch", [&](std::vector<uint64_t>& v){ return btree.batch_lookup_no_prefetch(queries, v); }, queries.size()));

    // --- 使用模板展开一键运行所有的配置参数 ---
    // 参数列表：1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
    using BTreeRunner = BTreeBenchRunner<BTreeType, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192>;
    BTreeRunner::run_all(btree, queries, results);

    // =============================================================
    // Part 1: PGM-Index Epsilon Scaling Test (Baseline Only)
    // =============================================================
    // std::cout << "\n  " << std::string(60, '=') << std::endl;
    // std::cout << "  Part 1: PGM-Index Epsilon Scaling Test" << std::endl;
    // std::cout << "  " << std::string(60, '=') << std::endl;
    
    // using PGMEpsRunner = PGMEpsilonRunner<4,8,16, 32, 64, 128, 256>;
    // PGMEpsRunner::run_all(data, queries, results);

    // // =============================================================
    // // Part 2: PGM-Index (Epsilon = 64) Prefetch Strategies
    // // =============================================================
    // std::cout << "\n  " << std::string(60, '=') << std::endl;
    // std::cout << "  Part 2: PGM-Index (Eps=64) Prefetch Strategies" << std::endl;
    // std::cout << "  " << std::string(60, '=') << std::endl;

    // std::cout << "\n  ┌─ Building PGM-Index (Epsilon = 64) for Strategies ────────" << std::endl;
    // using MyPGM = structures::pgm::PGMWrapper<uint64_t, uint64_t, 64>;
    // MyPGM pgm_tree;
    // Timer pgm_timer;
    // pgm_tree.bulk_load(data);
    // std::cout << "  │  Build Time:    " << std::fixed << std::setprecision(2) << pgm_timer.elapsed_ms() << " ms" << std::endl;
    // std::cout << "  └───────────────────────────────────────────────────────────" << std::endl;

    // using PGMRunner = PGMBenchRunner<MyPGM, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192>;
    // PGMRunner::run_all(pgm_tree, queries, results);

    // 1. 定义每一列的宽度 (这样修改起来方便，保证上下对齐)
    const int W_NAME = 26;  // 策略名称列宽
    const int W_TIME = 14;  // 时间列宽
    const int W_LAT  = 14;  // 延迟列宽
    const int W_TP   = 18;  // 吞吐量列宽
    const int W_SPD  = 10;  // 加速比列宽
    
    // 计算总宽度 (用于画分割线)
    // 2 是因为左右各有一个 "║ " 和 "║" 的边框修正
    const int TOTAL_WIDTH = W_NAME + W_TIME + W_LAT + W_TP + W_SPD + 2; 

    // 辅助 lambda：画横线
    auto print_line = [&](const char* left, const char* mid, const char* right) {
        std::cout << left;
        for(int i=0; i<TOTAL_WIDTH; ++i) std::cout << mid;
        std::cout << right << std::endl;
    };

    std::cout << "\n";
    // 顶部分割线
    print_line("╔", "═", "╗");

    // 2. 打印表头 (Header)
    std::cout << "║ " 
              << std::left  << std::setw(W_NAME) << "Strategy"
              << std::right << std::setw(W_TIME) << "Avg Time(ms)"
              << std::right << std::setw(W_LAT)  << "Latency(ns)"
              << std::right << std::setw(W_TP)   << "Throughput(M/Ops)"
              << std::right << std::setw(W_SPD)  << "Speedup"
              << " ║" << std::endl;

    // 中间分割线
    print_line("╠", "═", "╣");

    // 3. 打印数据行 (Data Rows)
    double baseline_time = results[0].avg_time_ms;

    for (const auto& r : results) {
        double speedup_factor = baseline_time / r.avg_time_ms;
        
        std::cout << "║ " 
                  // Col 1: Name (左对齐)
                  << std::left << std::setw(W_NAME) << r.strategy_name 
                  // Col 2: Time (右对齐)
                  << std::right << std::setw(W_TIME) << std::fixed << std::setprecision(1) << r.avg_time_ms
                  // Col 3: Latency (右对齐)
                  << std::setw(W_LAT) << std::fixed << std::setprecision(1) << r.avg_latency_ns
                  // Col 4: Throughput (右对齐)
                  << std::setw(W_TP) << std::fixed << std::setprecision(2) << r.query_throughput;
                  
        // Col 5: Speedup (右对齐)
        if (r.strategy_name == "No Prefetch") {
             std::cout << std::setw(W_SPD) << "1.00x";
        } else {
             std::cout << std::setw(W_SPD-1) << std::fixed << std::setprecision(2) << speedup_factor << "x";
        }
        
        std::cout << " ║" << std::endl;
    }

    // // 底部分割线
    // print_line("╚", "═", "╝");
    // // 在结果表格打印后加
    // std::cout << "\n\n=== Per-Layer Cycle Profile ===\n";
}

void print_results_table(const std::vector<TestResult>& results) {
    const int W_NAME = 26;
    const int W_TIME = 14;
    const int W_LAT  = 14;
    const int W_TP   = 18;
    const int W_SPD  = 10;
    const int TOTAL_WIDTH = W_NAME + W_TIME + W_LAT + W_TP + W_SPD + 2;

    auto print_line = [&](const char* left, const char* mid, const char* right) {
        std::cout << left;
        for (int i = 0; i < TOTAL_WIDTH; ++i) std::cout << mid;
        std::cout << right << std::endl;
    };

    std::cout << "\n";
    print_line("╔", "═", "╗");
    std::cout << "║ "
              << std::left  << std::setw(W_NAME) << "Strategy"
              << std::right << std::setw(W_TIME) << "Avg Time(ms)"
              << std::right << std::setw(W_LAT)  << "Latency(ns)"
              << std::right << std::setw(W_TP)   << "Throughput(M/Ops)"
              << std::right << std::setw(W_SPD)  << "Speedup"
              << " ║" << std::endl;
    print_line("╠", "═", "╣");

    double baseline_time = results.empty() ? 1.0 : results[0].avg_time_ms;
    for (const auto& r : results) {
        double speedup_factor = baseline_time / r.avg_time_ms;

        std::cout << "║ "
                  << std::left << std::setw(W_NAME) << r.strategy_name
                  << std::right << std::setw(W_TIME) << std::fixed << std::setprecision(1) << r.avg_time_ms
                  << std::setw(W_LAT) << std::fixed << std::setprecision(1) << r.avg_latency_ns
                  << std::setw(W_TP) << std::fixed << std::setprecision(2) << r.query_throughput;

        if (&r == &results.front()) {
            std::cout << std::setw(W_SPD) << "1.00x";
        } else {
            std::cout << std::setw(W_SPD - 1) << std::fixed << std::setprecision(2) << speedup_factor << "x";
        }
        std::cout << " ║" << std::endl;
    }
}

void test_lipp_strategies(const std::vector<uint64_t>& keys, const std::vector<uint64_t>& queries) {
    using LIPPType = structures::lipp::LIPPWrapper<uint64_t, uint64_t, true>;
    std::vector<TestResult> results;

    std::cout << "\n  ┌─ Building LIPP ───────────────────────────────────────────" << std::endl;
    std::cout << "  │  Total Keys:    " << keys.size() << std::endl;

    std::vector<std::pair<uint64_t, uint64_t>> data;
    data.reserve(keys.size());
    for (const auto& key : keys) {
        data.emplace_back(key, key);
    }

    Timer build_timer;
    LIPPType lipp;
    lipp.bulk_load(data);
    double build_time = build_timer.elapsed_ms();

    std::cout << "  │  Build Time:    " << std::fixed << std::setprecision(2) << build_time << " ms" << std::endl;
    std::cout << "  │  Index Size:    " << lipp.index_size() << " Bytes ("
              << (lipp.index_size() / 1024.0 / 1024.0) << " MB)" << std::endl;
    std::cout << "  │  Verifying:     " << std::flush;
    lipp.verify();
    std::cout << "OK" << std::endl;
    std::cout << "  └───────────────────────────────────────────────────────────" << std::endl;

    std::cout << "\n  Running LIPP Benchmark (" << NUM_RUNS << " runs per strategy)..." << std::endl;
    std::cout << "  " << std::string(60, '-') << std::endl;

    results.push_back(run_benchmark(
        "LIPP (No Prefetch)",
        [&](std::vector<uint64_t>& v){ return lipp.batch_lookup_no_prefetch(queries, v); },
        queries.size()
    ));

    using Runner = LIPPBenchRunner<LIPPType, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192>;
    Runner::run_all(lipp, queries, results);

    print_results_table(results);
}

int main() {
    std::cout << "\n=== LIPP Prefetch Strategy Benchmark ===" << std::endl;
    
    // 1. 加载数据集
    const std::string dataset_name = "osm_cellids_200M"; 
    std::string data_file = "data/" + dataset_name + "_uint64";
    
    std::cout << "[Loading] Reading " << data_file << "..." << std::endl;
    // 使用 SOSDDataLoader 加载二进制文件
    auto keys = SOSDDataLoader::load_binary_file(data_file);
    
    if (keys.empty()) {
        std::cerr << "Error: Could not load keys. Please check path: " << data_file << std::endl;
        return 1;
    }
    std::cout << "    Loaded " << keys.size() << " keys." << std::endl;

    // 确保 key 有序。LIPP bulk_load 要求 key 严格递增。
    if (!std::is_sorted(keys.begin(), keys.end())) {
        std::cout << "    Sorting keys..." << std::endl;
        std::sort(keys.begin(), keys.end());
    }
    
    // 2. 生成查询
    const size_t query_count = 1000000;
    std::cout << "[Queries] Generating " << query_count << " queries from dataset..." << std::endl;
    auto queries = SOSDDataLoader::generate_queries(keys, query_count);

    std::cout << "    Shuffling queries to maximize L3 Cache Misses..." << std::endl;
    std::mt19937 g(42);
    std::shuffle(queries.begin(), queries.end(), g);

    // 3. 运行 LIPP 测试。BTree/PGM benchmark 先不运行。
    test_lipp_strategies(keys, queries);
    return 0;
}
