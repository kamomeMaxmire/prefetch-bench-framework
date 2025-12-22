/**
 * B+ Tree Performance Benchmark
 * 测试 SOSD 真实数据集
 */

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#include "../include/utils/Timer.hpp"
#include "../include/utils/Statistics.hpp"
#include "../include/data/DataGenerator.hpp"
#include "../include/data/SOSDDataLoader.hpp"
#include "../include/structures/btree/BTree.hpp"
#include "../include/structures/btree/BTreeStatistics.hpp"

using namespace utils;
using namespace data;
using namespace structures:: btree;

// ==================== 辅助函数：测试特定 slot size ====================

template<int SlotSize>
BenchmarkResult test_with_slot_size(const std:: string& dataset_name,
                                    const std::vector<uint64_t>& keys,
                                    const std::vector<uint64_t>& queries) {
    using BTree = typename CustomBTree<uint64_t, uint64_t, SlotSize, SlotSize>::type;
    
    BenchmarkResult result;
    result.test_name = dataset_name + " [Slot=" + std::to_string(SlotSize) + "]";
    result.data_size = keys.size();
    result.query_count = queries.size();
    
    std::cout << "\n  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    std::cout << "  Testing Slot Size: " << SlotSize << std::endl;
    std::cout << "  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    // 构建 B+ 树
    BTree btree;
    Timer build_timer;
    
    size_t count = 0;
    for (const auto& key : keys) {
        btree.insert(std::make_pair(key, key));
        count++;
        if (count % 20'000'000 == 0) {
            std::cout << "    Inserted " << count / 1'000'000 << "M keys..." << std::endl;
        }
    }
    
    result.build_time_ms = build_timer.elapsed_ms();
    result.insert_throughput = (keys.size() / result.build_time_ms) * 1000.0 / 1e6;
    
    // 执行查询
    Timer query_timer;
    size_t found = 0;
    for (const auto& q : queries) {
        auto it = btree.find(q);
        if (it != btree.end()) {
            found++;
        }
    }
    result.query_time_ms = query_timer.elapsed_ms();
    result.search_throughput = (queries.size() / result.query_time_ms) * 1000.0 / 1e6;
    result.avg_latency_ns = (result.query_time_ms * 1e6) / queries.size();
    result.hits = found;
    result.hit_rate = (found * 100.0) / queries.size();
    
    // 打印结果
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "    Build Time:         " << result.build_time_ms << " ms" << std::endl;
    std::cout << "    Query Time:         " << result.query_time_ms << " ms" << std:: endl;
    std::cout << "    Avg Latency:         " << result.avg_latency_ns << " ns/query" << std::endl;
    std::cout << "    Query Throughput:   " << result.search_throughput << " M ops/s" << std::endl;
    
    return result;
}

// ==================== Slot Size 对比测试 ====================

void compare_slot_sizes(const std::string& dataset_name, size_t query_count = 10'000'000) {
    OutputFormatter::print_header("B+ Tree Slot Size Comparison - " + dataset_name);
    
    // 1. 加载数据
    std::cout << "\n[1] Loading Data..." << std::endl;
    std::string data_file = "data/" + dataset_name;
    if (data_file.find("_uint64") == std::string::npos) {
        data_file += "_uint64";
    }
    
    auto keys = SOSDDataLoader::load_binary_file(data_file);
    if (keys.empty()) {
        std::cerr << "  ✗ Error: Failed to load " << data_file << std::endl;
        return;
    }
    std::cout << "    ✓ Loaded " << keys.size() << " keys" << std::endl;
    
    // 2. 生成查询
    std::cout << "\n[2] Generating Queries..." << std::endl;
    query_count = std::min(query_count, keys.size());
    auto queries = SOSDDataLoader::generate_queries(keys, query_count);
    std::cout << "    ✓ Generated " << queries.size() << " queries" << std::endl;
    
    // 3. 测试多种 slot 配置
    std::cout << "\n[3] Testing Different Slot Sizes..." << std::endl;
    
    std::vector<BenchmarkResult> results;
    std::vector<int> slot_sizes;
    
    // 测试各种 slot size（通过模板展开）
    results.push_back(test_with_slot_size<16>(dataset_name, keys, queries));
    slot_sizes.push_back(16);
    
    results.push_back(test_with_slot_size<24>(dataset_name, keys, queries));
    slot_sizes.push_back(24);
    
    results.push_back(test_with_slot_size<32>(dataset_name, keys, queries));
    slot_sizes.push_back(32);
    
    results.push_back(test_with_slot_size<40>(dataset_name, keys, queries));
    slot_sizes.push_back(40);
    
    results.push_back(test_with_slot_size<48>(dataset_name, keys, queries));
    slot_sizes.push_back(48);
    
    results.push_back(test_with_slot_size<56>(dataset_name, keys, queries));
    slot_sizes.push_back(56);
    
    results.push_back(test_with_slot_size<64>(dataset_name, keys, queries));
    slot_sizes.push_back(64);
    
    results.push_back(test_with_slot_size<80>(dataset_name, keys, queries));
    slot_sizes.push_back(80);
    
    // 4. 汇总对比
    std::cout << "\n" << std::endl;
    OutputFormatter::print_header("Performance Summary");
    
    std::cout << "\n  Dataset: " << dataset_name << std::endl;
    std::cout << "  Total Keys: " << keys.size() << " | Query Count: " << queries.size() << std::endl;
    
    std::cout << "\n  ╔═══════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "  ║  Slot    Build(ms)   Query(ms)   Latency(ns)   Throughput(Mops)  ║" << std::endl;
    std:: cout << "  ╠═══════════════════════════════════════════════════════════════════╣" << std::endl;
    
    // 找到最优配置
    double best_latency = 1e9;
    int best_slot = 0;
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        
        if (r.avg_latency_ns < best_latency) {
            best_latency = r.avg_latency_ns;
            best_slot = slot_sizes[i];
        }
    }
    
    // 输出每行结果
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        bool is_best = (slot_sizes[i] == best_slot);
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  ║  " << std::setw(4) << slot_sizes[i]
                  << std::setw(14) << r.build_time_ms
                  << std::setw(13) << r.query_time_ms
                  << std::setw(15) << r.avg_latency_ns
                  << std::setw(17) << r.search_throughput;
        
        if (is_best) {
            std::cout << "  ⭐ ║" << std::endl;
        } else {
            std::cout << "     ║" << std::endl;
        }
    }
    
    std::cout << "  ╚═══════════════════════════════════════════════════════════════════╝" << std::endl;
    
    // 标记最优配置
    std::cout << "\n  ┌─ Optimal Configuration ────────────────────────────────" << std::endl;
    std::cout << "  │" << std::endl;
    std:: cout << "  │  ⭐ Best Slot Size:    " << best_slot << std::endl;
    std::cout << "  │  ⭐ Best Latency:    " << std::fixed << std::setprecision(2) 
              << best_latency << " ns/query" << std:: endl;
    
    // 对比首尾性能
    double best_improvement = ((results[0].avg_latency_ns - best_latency) 
                              / results[0].avg_latency_ns) * 100.0;
    
    std:: cout << "  │" << std::endl;
    std::cout << "  │  Improvement (Slot 16 → " << best_slot << "):" << std::endl;
    std::cout << "  │    Latency:  " << std::fixed << std::setprecision(1) 
              << best_improvement << "% reduction ✓" << std::endl;
    
    if (slot_sizes.back() != best_slot) {
        std::cout << "  │" << std::endl;
        std::cout << "  │  ⚠ Note: Performance degrades after slot size " << best_slot << std::endl;
    }
    
    std::cout << "  │" << std::endl;
    std::cout << "  └────────────────────────────────────────────────────────" << std::endl;
    
    std::cout << std::endl;
}

// ==================== 单个数据集测试 ====================

BenchmarkResult test_sosd_dataset(const std::string& dataset_name, size_t query_count = 10'000'000) {
    BenchmarkResult result;
    result.test_name = dataset_name;
    
    OutputFormatter::print_header("Test:  SOSD Dataset - " + dataset_name);
    
    // 1. 加载数据
    OutputFormatter::print_subheader("[1] Loading Data");
    
    std::string data_file = "data/" + dataset_name;
    if (data_file. find("_uint64") == std::string::npos) {
        data_file += "_uint64";
    }
    
    auto keys = SOSDDataLoader:: load_binary_file(data_file);
    
    if (keys.empty()) {
        OutputFormatter::print_error("Failed to load:   " + data_file);
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
    
    // 3. 构建 B+ 树（使用默认配置）
    OutputFormatter::print_subheader("[3] Building B+ Tree (Default:  16 slots)");
    BTree64 btree;
    
    Timer build_timer;
    size_t count = 0;
    for (const auto& key :  keys) {
        btree.insert(std::make_pair(key, key));
        
        count++;
        if (count % 20'000'000 == 0) {
            std::cout << "    Inserted " << count / 1'000'000 << "M keys..." << std::endl;
        }
    }
    double build_time_ms = build_timer.elapsed_ms();
    
    result.build_time_ms = build_time_ms;
    result.insert_throughput = (keys.size() / build_time_ms) * 1000.0 / 1e6;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "    Build Time:  " << build_time_ms << " ms" << std::endl;
    std::cout << "    Insert Throughput: " << result.insert_throughput << " M ops/s" << std::endl;
    
    // 4. 树统计
    OutputFormatter::print_subheader("[4] Tree Structure");
    auto tree_stats = TreeStatistics:: collect(btree);
    tree_stats.print();
    
    // 5. 执行查询
    OutputFormatter::print_subheader("[5] Running Queries");
    
    Timer query_timer;
    size_t found = 0;
    
    for (const auto& q :  queries) {
        auto it = btree.find(q);
        if (it != btree. end()) {
            found++;
        }
    }
    
    double query_time_ms = query_timer.elapsed_ms();
    
    result.query_time_ms = query_time_ms;
    result.search_throughput = (queries.size() / query_time_ms) * 1000.0 / 1e6;
    result.avg_latency_ns = (query_time_ms * 1e6) / queries.size();
    result.hits = found;
    result.hit_rate = (found * 100.0) / queries.size();
    
    std::cout << "\n  ┌─ Query Performance ─────────────────────" << std::endl;
    std::cout << "  │ Total Time:       " << query_time_ms << " ms" << std::endl;
    std::cout << "  │ Query Count:      " << queries.size() << std::endl;
    std::cout << "  │ Throughput:       " << result.search_throughput << " M ops/s" << std::endl;
    std::cout << "  │ Avg Latency:      " << result.avg_latency_ns << " ns/query" << std::endl;
    std::cout << "  │ Hit Rate:         " << result.hit_rate << " %" << std::endl;
    std::cout << "  │ Hits:             " << found << " / " << queries.size() << std::endl;
    std::cout << "  └─────────────────────────────────────────" << std::endl;
    
    std::cout << "\n  Verification:   " << (found > 0 ? "✓ PASSED" : "✗ FAILED") << std::endl;
    
    return result;
}

// ==================== 主程序 ====================

int main(int argc, char* argv[]) {
    OutputFormatter::print_header("B+ Tree Performance Benchmark Framework");
    
    std::cout << "\n  SOSD B+ Tree (STX) Performance Testing" << std::endl;
    std::cout << "  Default Slot Size: 16" << std::endl;
    std::cout << "  ────────────────────────────────────────" << std::endl;
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        // 生成数据
        if (arg == "generate") {
            if (argc < 4) {
                std::cout << "\n  Usage: ./prefetch_bench generate <size> <type>" << std::endl;
                std::cout << "  Types: random, sorted, partial" << std::endl;
                std::cout << "  Example: ./prefetch_bench generate 200000000 random" << std::endl;
                return 1;
            }
            
            size_t size = std::stoull(argv[2]);
            std::string type = argv[3];
            std::string filename = "data/custom_" + type + "_" + std::to_string(size) + "_uint64";
            
            std:: cout << "\n  Generating custom dataset..." << std::endl;
            
            if (type == "random") {
                DataGenerator::generate_and_save_random(size, filename);
            } else if (type == "sorted") {
                DataGenerator::generate_and_save_sorted(size, filename);
            } else if (type == "partial") {
                DataGenerator::generate_and_save_partial(size, filename);
            } else {
                std::cerr << "  Unknown type: " << type << std:: endl;
                return 1;
            }
            
            std::cout << "  ✓ Dataset generated successfully!" << std::endl;
            std::cout << "\n  To test it:" << std::endl;
            std::cout << "    ./prefetch_bench custom_" << type << "_" << size << std::endl;
            std::cout << "  To compare slot sizes:" << std::endl;
            std::cout << "    ./prefetch_bench compare custom_" << type << "_" << size << std::endl;
            
            return 0;
        }
        
        // Slot size 对比测试
        if (arg == "compare") {
            if (argc < 3) {
                std::cout << "\n  Usage: ./prefetch_bench compare <dataset>" << std::endl;
                std::cout << "  Example: ./prefetch_bench compare books_200M" << std::endl;
                std::cout << "           ./prefetch_bench compare custom_random_200000000" << std::endl;
                return 1;
            }
            
            std::string dataset = argv[2];
            compare_slot_sizes(dataset);
            return 0;
        }
        
        // 测试单个数据集
        auto result = test_sosd_dataset(arg);
        if (result.data_size > 0) {
            result.print();
        }
    } else {
        // 默认帮助信息
        std::cout << "\n  Usage:" << std::endl;
        std::cout << "    ./prefetch_bench generate <size> <type>   # Generate dataset" << std::endl;
        std::cout << "    ./prefetch_bench <dataset>                # Test with default (16 slots)" << std::endl;
        std::cout << "    ./prefetch_bench compare <dataset>        # Compare slot sizes (16-128)" << std::endl;
        std::cout << "\n  Examples:" << std::endl;
        std::cout << "    ./prefetch_bench generate 200000000 random" << std::endl;
        std::cout << "    ./prefetch_bench books_200M" << std::endl;
        std::cout << "    ./prefetch_bench compare books_200M" << std::endl;
        std::cout << "    ./prefetch_bench compare custom_random_200000000" << std::endl;
    }
    
    return 0;
}