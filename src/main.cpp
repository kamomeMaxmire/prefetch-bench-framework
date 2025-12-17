/**
 * B+ Tree Performance Benchmark
 * 测试 SOSD 真实数据集
 */

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>

// 项目工具类
#include "../include/utils/Timer.hpp"
#include "../include/utils/Statistics.hpp"
#include "../include/data/DataGenerator.hpp"
#include "../include/data/SOSDDataLoader.hpp"

// B+ 树相关
#include "../include/structures/btree/BTree.hpp"
#include "../include/structures/btree/BTreeStatistics.hpp"

using namespace utils;
using namespace data;
using namespace structures:: btree;

// ==================== 测试函数 ====================

/**
 * 测试 SOSD 数据集
 */
BenchmarkResult test_sosd_dataset(const std::string& dataset_name, 
                                  size_t query_count = 10'000'000) {
    BenchmarkResult result;
    result.test_name = dataset_name;
    
    OutputFormatter::print_header("Test: SOSD Dataset - " + dataset_name);
    
    // 1. 加载数据（固定从 data/ 目录）
    OutputFormatter::print_subheader("[1] Loading Data");
    
    std::string data_file = "data/" + dataset_name;
    if (data_file.find("_uint64") == std::string::npos) {
        data_file += "_uint64";
    }
    
    auto keys = SOSDDataLoader::load_binary_file(data_file);
    
    if (keys.empty()) {
        OutputFormatter::print_error("Failed to load:  " + data_file);
        std::cerr << "\n  Please ensure file exists in data/ directory" << std::endl;
        result.data_size = 0;
        return result;
    }
    
    std::cout << "    Loaded " << keys.size() << " keys" << std::endl;
    result.data_size = keys.size();
    result.query_count = std::min(query_count, keys.size());
    
    // 2. 生成查询
    OutputFormatter::print_subheader("[2] Generating Queries");
    auto queries = SOSDDataLoader::generate_queries(keys, result.query_count);
    std::cout << "    Generated " << queries.size() << " queries" << std::endl;
    
    // 3. 构建 B+ 树
    OutputFormatter::print_subheader("[3] Building B+ Tree");
    BTree64 btree;
    
    Timer build_timer;
    for (const auto& key : keys) {
        btree.insert(std::make_pair(key, key));
    }
    double build_time_ms = build_timer.elapsed_ms();
    
    result.build_time_ms = build_time_ms;
    result.insert_throughput = (keys.size() / build_time_ms) * 1000.0 / 1e6;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "    Build Time: " << build_time_ms << " ms" << std::endl;
    std::cout << "    Insert Throughput: " << result.insert_throughput << " M ops/s" << std::endl;
    
    // 4. 树统计
    OutputFormatter::print_subheader("[4] Tree Structure");
    auto tree_stats = TreeStatistics::collect(btree);
    tree_stats.print();
    
    // 5. 执行查询
    OutputFormatter::print_subheader("[5] Running Queries");
    
    Timer query_timer;
    size_t found = 0;
    
    for (const auto& q : queries) {
        auto it = btree.find(q);
        if (it != btree.end()) {
            found++;
        }
    }
    
    double query_time_ms = query_timer.elapsed_ms();
    
    // 直接显示原始测量值
    result.query_time_ms = query_time_ms;
    result.search_throughput = (queries.size() / query_time_ms) * 1000.0 / 1e6;
    result.avg_latency_ns = (query_time_ms * 1e6) / queries.size();
    result.hits = found;
    result.hit_rate = (found * 100.0) / queries.size();
    
    std::cout << "\n  ┌─ Query Performance ─────────────────────" << std::endl;
    std::cout << "  │ Total Time:       " << query_time_ms << " ms" << std::endl;
    std::cout << "  │ Query Count:     " << queries.size() << std::endl;
    std::cout << "  │ Throughput:      " << result.search_throughput << " M ops/s" << std::endl;
    std::cout << "  │ Avg Latency:     " << result.avg_latency_ns << " ns/query" << std::endl;
    std::cout << "  │ Hit Rate:        " << result.hit_rate << " %" << std::endl;
    std::cout << "  │ Hits:            " << found << " / " << queries.size() << std::endl;
    std::cout << "  └─────────────────────────────────────────" << std::endl;
    
    std::cout << "\n  Verification:  " << (found > 0 ? "✓ PASSED" : "✗ FAILED") << std::endl;
    
    return result;
}

// ==================== 主程序 ====================

int main(int argc, char* argv[]) {
    OutputFormatter::print_header("B+ Tree Performance Benchmark Framework");
    
    std::cout << "\n  SOSD B+ Tree (STX) Performance Testing" << std::endl;
    std::cout << "  ────────────────────────────────────────" << std::endl;
    
    // 默认数据集列表
    std::vector<std::string> datasets = {
        "books_200M",
        "osm_cellids_200M",
        "wiki_ts_200M",
        "fb_200M"
    };
    
    // 解析命令行参数
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "all") {
            // 测试所有数据集
            std::cout << "\n  Running all datasets.. .\n" << std::endl;
            
            std::vector<BenchmarkResult> results;
            for (const auto& dataset : datasets) {
                auto result = test_sosd_dataset(dataset);
                if (result.data_size > 0) {
                    results.push_back(result);
                }
                std::cout << "\n" << std::endl;
            }
            
            // 打印汇总
            if (!results.empty()) {
                OutputFormatter::print_header("Summary of All Tests");
                std::cout << std::left << std::setw(20) << "Dataset"
                          << std::right << std::setw(15) << "Build (ms)"
                          << std:: setw(15) << "Query (ms)"
                          << std::setw(15) << "Throughput"
                          << std::setw(12) << "Hit Rate" << std::endl;
                std::cout << std::string(77, '-') << std::endl;
                
                for (const auto& r : results) {
                    std::cout << std::fixed << std::setprecision(2);
                    std::cout << std::left << std::setw(20) << r.test_name
                              << std::right << std::setw(15) << r.build_time_ms
                              << std::setw(15) << r.query_time_ms
                              << std::setw(13) << r.search_throughput << " M"
                              << std::setw(11) << r.hit_rate << " %" << std::endl;
                }
            }
        } else {
            // 测试单个数据集
            auto result = test_sosd_dataset(arg);
            if (result.data_size > 0) {
                result.print();
            }
        }
    } else {
        // 默认测试 books_200M
        std::cout << "\n  No dataset specified, using default: books_200M" << std::endl;
        std::cout << "  (Use './prefetch_bench all' to test all datasets)\n" << std::endl;
        
        auto result = test_sosd_dataset("books_200M");
        if (result.data_size > 0) {
            result.print();
        }
    }
    
    // 性能分析提示
    OutputFormatter::print_header("Performance Analysis Tools");
    std::cout << "\n  Usage:" << std::endl;
    std::cout << "    ./prefetch_bench [dataset]    # Test specific dataset" << std::endl;
    std::cout << "    ./prefetch_bench all          # Test all datasets" << std::endl;
    std::cout << "\n  Available datasets:" << std:: endl;
    std::cout << "    books_200M, osm_cellids_200M, wiki_ts_200M, fb_200M" << std::endl;
    std::cout << "\n  For detailed profiling:" << std::endl;
    std::cout << "    ./run_perf.sh [dataset]" << std::endl;
    
    OutputFormatter::print_header("Benchmark Completed");
    
    return 0;
}