#pragma once
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>

namespace data {

class DataGenerator {
public:
    // 生成有序的主键数据 (模拟数据库主键 0, 2, 4...)
    static std::vector<uint64_t> generate_keys(size_t size) {
        std::vector<uint64_t> keys(size);
        for (size_t i = 0; i < size; ++i) {
            keys[i] = i * 2;
        }
        return keys;
    }

    // 生成随机查询请求 (均匀分布)
    static std::vector<uint64_t> generate_random_queries(size_t size, size_t key_range) {
        std::vector<uint64_t> queries(size);
        std::mt19937_64 rng(42); // 固定种子，保证每次测试结果可复现
        for (size_t i = 0; i < size; ++i) {
            queries[i] = (rng() % key_range) * 2; 
        }
        return queries;
    }

    // (可选) 以后可以在这里添加 Zipf (长尾分布) 生成器
};

} // namespace data