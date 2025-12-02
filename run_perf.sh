#!/bin/bash

# 确保在 build 目录
cd build || exit

echo "========================================"
echo "Running PERF analysis on B+ Tree Benchmark"
echo "========================================"

# 定义要监控的事件
EVENTS="cycles,instructions,L1-dcache-load-misses,LLC-load-misses"

# 运行 perf stat
# -r 5 表示运行 5 次取平均值，结果更准
sudo perf stat -r 5 -e $EVENTS ./prefetch_bench