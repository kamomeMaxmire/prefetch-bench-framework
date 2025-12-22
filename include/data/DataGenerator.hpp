#pragma once
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>

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

    // ============ 新增：生成并保存数据到文件 ============
    
    /**
     * 生成随机数据并保存到文件
     */
    // 生成唯一的随机数据并保存到文件
    static void generate_and_save_random(size_t size, const std:: string& filename) {
        std::vector<uint64_t> data(size);
    
    // 先生成有序的唯一值
        for (size_t i = 0; i < size; ++i) {
            data[i] = i + 1;
         }
    
    // 然后打乱顺序
        std::random_device rd;
        std::mt19937_64 rng(42);  // 使用固定种子保证可重复
        std::shuffle(data. begin(), data.end(), rng);
            
        save_to_file(data, filename);
        std::cout << "    Generated random dataset (" << size << " keys) -> " << filename << std:: endl;
    }
    
    /**
     * 生成有序数据并保存到文件
     */
    static void generate_and_save_sorted(size_t size, const std::string& filename) {
        std::vector<uint64_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i + 1;
        }
        
        save_to_file(data, filename);
        std::cout << "    Generated sorted dataset (" << size << " keys) -> " << filename << std::endl;
    }
    
    /**
     * 生成部分有序数据并保存到文件
     */
    static void generate_and_save_partial(size_t size, const std:: string& filename) {
        std::vector<uint64_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i + 1;
        }
        
        // 打乱前一半数据
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(data.begin(), data.begin() + size / 2, rng);
        
        save_to_file(data, filename);
        std::cout << "    Generated partially sorted dataset (" << size << " keys) -> " << filename << std::endl;
    }

private:
    /**
     * 保存数据到二进制文件
     */
    static void save_to_file(const std::vector<uint64_t>& data, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Error: Cannot create file " << filename << std::endl;
            return;
        }
        file.write(reinterpret_cast<const char*>(data. data()), data.size() * sizeof(uint64_t));
        file.close();
    }
};

} // namespace data