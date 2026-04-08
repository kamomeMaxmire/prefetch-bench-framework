#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>

namespace utils {

class PerfMonitor {
private:
    struct Event {
        int fd;
        std::string name;
        uint64_t value;
    };
    std::vector<Event> events;

    // 封装 Linux 底层的 perf_event_open 系统调用
    static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                                int cpu, int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    }

    void add_event(uint32_t type, uint64_t config, const std::string& name) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = type;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1; // 剔除操作系统的后台干扰，只测我们的代码
        pe.exclude_hv = 1;

        int fd = perf_event_open(&pe, 0, -1, -1, 0);
        if (fd == -1) {
            std::cerr << " [警告] 无法打开硬件计数器: " << name 
                      << " (可能需要 sudo 权限运行)\n";
        }
        events.push_back({fd, name, 0});
    }

public:
    PerfMonitor() {
        // 1. L1 数据缓存 Miss (L1-D Miss) - 代表跨层指针跳转的总次数
        add_event(PERF_TYPE_HW_CACHE, 
                  (PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), 
                  "L1-Miss");
                  
        // 2. LLC (L3) 末级缓存 Miss - 代表真正去慢速 DRAM 内存拿数据的次数！
        add_event(PERF_TYPE_HW_CACHE, 
                  (PERF_COUNT_HW_CACHE_LL) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), 
                  "LLC-Miss");
    }

    ~PerfMonitor() {
        for (auto& ev : events) if (ev.fd != -1) close(ev.fd);
    }

    void start() {
        for (auto& ev : events) {
            if (ev.fd != -1) {
                ioctl(ev.fd, PERF_EVENT_IOC_RESET, 0);
                ioctl(ev.fd, PERF_EVENT_IOC_ENABLE, 0);
            }
        }
    }

    void stop() {
        for (auto& ev : events) {
            if (ev.fd != -1) {
                ioctl(ev.fd, PERF_EVENT_IOC_DISABLE, 0);
                read(ev.fd, &ev.value, sizeof(uint64_t));
            }
        }
    }

    void print_stats(size_t query_count) {
        std::cout << "  [Hardware Counters per Query]" << std::endl;
        for (const auto& ev : events) {
            if (ev.fd != -1) {
                // 计算平均每一次查询引发了多少次物理动作
                double per_query = static_cast<double>(ev.value) / query_count;
                std::cout << "   - " << std::left << std::setw(15) << ev.name 
                          << ": " << std::fixed << std::setprecision(2) << per_query << " 次/查询\n";
            }
        }
    }
};

} // namespace utils