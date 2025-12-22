#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <random>

namespace data {

/**
 * SOSD 数据集加载器
 * 支持加载 SOSD 项目的二进制数据文件
 */
class SOSDDataLoader {
public:
    /**
     * 从二进制文件加载 uint64 数据
     * @param filename 文件名（不含路径）
     * @param max_size 最大加载数量，0 表示加载全部
     * @return 数据向量
     */
    static std::vector<uint64_t> load_binary_file(
        const std::string& filename, 
        size_t max_size = 0
    ) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return {};
        }
        
        // 获取文件大小
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        size_t num_elements = file_size / sizeof(uint64_t);
        if (max_size > 0 && max_size < num_elements) {
            num_elements = max_size;
        }
        
        std::vector<uint64_t> data(num_elements);
        file.read(reinterpret_cast<char*>(data.data()), num_elements * sizeof(uint64_t));
        file.close();
        
        std::cout << "    Loaded " << data.size() << " keys from " << filename << std::endl;
        return data;
    }
    
    /**
     * 从已加载的数据中生成查询负载
     * @param data 源数据
     * @param num_queries 查询数量
     * @param seed 随机种子
     * @return 查询向量
     */
    static std::vector<uint64_t> generate_queries(
        const std::vector<uint64_t>& data,
        size_t num_queries,
        uint64_t seed = 42
    ) {
        std::vector<uint64_t> queries(num_queries);
        std::mt19937_64 rng(seed);
        
        for (size_t i = 0; i < num_queries; ++i) {
            size_t idx = rng() % data.size();
            queries[i] = data[idx];
        }
        
        return queries;
    }
    
    /**
     * 生成范围查询负载（预留）
     */
    static std::vector<std::pair<uint64_t, uint64_t>> generate_range_queries(
        const std::vector<uint64_t>& data,
        size_t num_queries,
        size_t range_size,
        uint64_t seed = 42
    ) {
        std::vector<std::pair<uint64_t, uint64_t>> queries(num_queries);
        std::mt19937_64 rng(seed);
        
        for (size_t i = 0; i < num_queries; ++i) {
            size_t start_idx = rng() % data.size();
            size_t end_idx = std::min(start_idx + range_size, data.size() - 1);
            queries[i] = {data[start_idx], data[end_idx]};
        }
        
        return queries;
    }
};

} // namespace data