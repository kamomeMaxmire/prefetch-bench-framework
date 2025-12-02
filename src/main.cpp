#include <iostream>
#include <vector>
#include "../include/data/DataGenerator.hpp"
#include "../include/utils/Timer.hpp"
#include "../include/structures/SimpleBTree.hpp"

// 引入优化算法的头文件
// #include "../include/algorithms/GroupPrefetchBTree.hpp" 

int main() {
    // 1. 配置参数
    const size_t DATA_SIZE = 1000000;  // 100万数据
    const size_t QUERY_SIZE = 500000;  // 50万查询
    
    std::cout << ">>> Benchmark Started <<<" << std::endl;

    // 2. 生成数据 
    auto keys = data::DataGenerator::generate_keys(DATA_SIZE);
    auto queries = data::DataGenerator::generate_random_queries(QUERY_SIZE, DATA_SIZE);
    std::cout << "[Data] Generated " << keys.size() << " keys, " << queries.size() << " queries." << std::endl;

    // 3. 准备计时器和结果容器
    utils::Timer timer;
    uint64_t val;
    
    // --- 测试 1: 基础 B+ 树 (Baseline) ---
    {
        std::cout << "\n[Test: Baseline B+ Tree]" << std::endl;
        structures::SimpleBTree<uint64_t, uint64_t, 32> btree;

        // Build
        timer.reset();
        for (auto k : keys) btree.insert(k, k);
        std::cout << "  -> Build Time: " << timer.elapsed_ms() << " ms" << std::endl;

        // Search
        timer.reset();
        size_t found = 0;
        for (auto q : queries) {
            if (btree.search(q, val)) found++;
        }
        double time = timer.elapsed_ms();
        std::cout << "  -> Search Time: " << time << " ms" << std::endl;
        std::cout << "  -> Throughput:  " << (QUERY_SIZE / time) * 1000.0 / 1e6 << " M ops/sec" << std::endl;
    }

    // --- 测试 2: (预留位置) Group Prefetch ---
    /*
    {
        std::cout << "\n[Test: Group Prefetch]" << std::endl;
        // 调用其他算法函数
    }
    */

    return 0;
}