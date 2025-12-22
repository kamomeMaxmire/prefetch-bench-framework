#pragma once

#include <iostream>
#include <iomanip>
#include <string>

namespace utils {

/**
 * 性能统计结果
 */
struct BenchmarkResult {
    std::string test_name;
    size_t data_size;
    size_t query_count;
    
    // 构建性能
    double build_time_ms;
    double insert_throughput;  // M ops/s
    
    // 查询性能
    double query_time_ms;
    double search_throughput;  // M ops/s
    double avg_latency_ns;
    
    // 正确性
    size_t hits;
    double hit_rate;
    
    void print() const {
        std::cout << "\n  ════════════════════════════════════════════════════" << std::endl;
        std::cout << "  Performance Results: " << test_name << std::endl;
        std::cout << "  ════════════════════════════════════════════════════" << std::endl;
        
        auto print_metric = [](const std::string& name, auto value, const std::string& unit = "") {
            std::cout << "    " << std::left << std::setw(28) << name 
                      << std::right << std::setw(12) << std::fixed << std::setprecision(2) 
                      << value;
            if (!unit.empty()) std::cout << " " << unit;
            std::cout << std::endl;
        };
        
        std::cout << "\n  Dataset Info:" << std::endl;
        print_metric("Data Size:", data_size, "keys");
        print_metric("Query Count:", query_count, "queries");
        
        std::cout << "\n  Build Performance:" << std::endl;
        print_metric("Build Time:", build_time_ms, "ms");
        print_metric("Insert Throughput:", insert_throughput, "M ops/s");
        
        std::cout << "\n  Query Performance:" << std::endl;
        print_metric("Query Time:", query_time_ms, "ms");
        print_metric("Search Throughput:", search_throughput, "M ops/s");
        print_metric("Avg Latency:", avg_latency_ns, "ns/query");
        print_metric("Hit Rate:", hit_rate, "%");
        print_metric("Hits:", static_cast<double>(hits), "");
    }
};

/**
 * 格式化输出工具
 */
class OutputFormatter {
public:
    static void print_header(const std::string& title) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "  " << title << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    static void print_subheader(const std::string& title) {
        std::cout << "\n  " << title << std::endl;
        std::cout << "  " << std::string(title.length(), '-') << std::endl;
    }
    
    static void print_metric(const std::string& name, double value, const std::string& unit = "") {
        std::cout << "  " << std::left << std::setw(30) << (name + ":")
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) 
                  << value;
        if (!unit.empty()) std::cout << " " << unit;
        std::cout << std::endl;
    }
    
    static void print_success(const std::string& message) {
        std::cout << "  ✓ " << message << std::endl;
    }
    
    static void print_error(const std::string& message) {
        std::cout << "  ✗ " << message << std::endl;
    }
};

} // namespace utils