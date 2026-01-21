#!/bin/bash

# 项目结构诊断脚本

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║         B+ Tree Prefetch Benchmark - Setup Diagnostic            ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

SUCCESS=0
FAILED=0

check_file() {
    if [ -f "$1" ]; then
        echo "  ✓ $1"
        SUCCESS=$((SUCCESS + 1))
    else
        echo "  ✗ $1 [MISSING]"
        FAILED=$((FAILED + 1))
    fi
}

check_dir() {
    if [ -d "$1" ]; then
        echo "  ✓ $1/"
        SUCCESS=$((SUCCESS + 1))
    else
        echo "  ✗ $1/ [MISSING]"
        FAILED=$((FAILED + 1))
    fi
}

# ========== 检查核心文件 ==========
echo "[1] Checking Core Files..."
check_file "CMakeLists.txt"
check_file "src/main.cpp"
check_file "src/main_prefetch.cpp"
echo ""

# ========== 检查头文件目录 ==========
echo "[2] Checking Include Structure..."
check_dir "include"
check_dir "include/data"
check_dir "include/utils"
check_dir "include/structures"
check_dir "include/structures/btree"
check_dir "include/structures/btree/algorithms"
echo ""

# ========== 检查必需的头文件 ==========
echo "[3] Checking Required Headers..."
check_file "include/data/DataGenerator.hpp"
check_file "include/data/SOSDDataLoader.hpp"
check_file "include/utils/Timer.hpp"
check_file "include/utils/PrefetchUtils.hpp"
check_file "include/utils/Statistics.hpp"
check_file "include/structures/btree/BTree.hpp"
check_file "include/structures/btree/BTreeStatistics.hpp"
echo ""

# ========== 检查预取算法文件 ==========
echo "[4] Checking Prefetch Algorithm Files..."
check_file "include/structures/btree/algorithms/NoPrefetch.hpp"
check_file "include/structures/btree/algorithms/GroupPrefetch.hpp"

# 检查 SPP 文件（可能是 SPP.hpp 或 SoftwarePipelinedPrefetch.hpp）
if [ -f "include/structures/btree/algorithms/SPP.hpp" ]; then
    echo "  ✓ include/structures/btree/algorithms/SPP.hpp"
    SUCCESS=$((SUCCESS + 1))
elif [ -f "include/structures/btree/algorithms/SoftwarePipelinedPrefetch.hpp" ]; then
    echo "  ✓ include/structures/btree/algorithms/SoftwarePipelinedPrefetch.hpp"
    SUCCESS=$((SUCCESS + 1))
else
    echo "  ✗ include/structures/btree/algorithms/SPP.hpp or SoftwarePipelinedPrefetch.hpp [MISSING]"
    FAILED=$((FAILED + 1))
fi
echo ""

# ========== 检查 STX B+Tree ==========
echo "[5] Checking STX B+Tree Library..."
check_dir "include/structures/btree/stx-btree-0.9"
check_dir "include/structures/btree/stx-btree-0.9/include"
check_dir "include/structures/btree/stx-btree-0.9/include/stx"
check_file "include/structures/btree/stx-btree-0.9/include/stx/btree_map.h"
check_file "include/structures/btree/stx-btree-0.9/include/stx/btree.h"
echo ""

# ========== 检查数据目录 ==========
echo "[6] Checking Data Directory..."
check_dir "data"

if [ -d "data" ]; then
    # 检查所有可能的数据文件格式
    DATA_COUNT=$(ls -1 data/*_uint64 data/*.uint64 data/*.zst 2>/dev/null | wc -l)
    if [ $DATA_COUNT -gt 0 ]; then
        echo "  ✓ Found $DATA_COUNT data file(s)"
        echo ""
        echo "  Available data files:"
        ls -lh data/*_uint64 data/*.uint64 data/*.zst 2>/dev/null | awk '{print "    - " $9 " (" $5 ")"}'
    else
        echo "  ⚠ No data files found (this is OK, you can generate them)"
    fi
fi
echo ""

# ========== 检查构建目录 ==========
echo "[7] Checking Build Directory..."
if [ -d "build" ]; then
    check_dir "build"
    if [ -f "build/prefetch_compare" ]; then
        echo "  ✓ build/prefetch_compare [compiled]"
    else
        echo "  ⚠ build/prefetch_compare [not compiled yet]"
    fi
    if [ -f "build/prefetch_bench" ]; then
        echo "  ✓ build/prefetch_bench [compiled]"
    else
        echo "  ⚠ build/prefetch_bench [not compiled yet]"
    fi
else
    echo "  ⚠ build/ directory not found (will be created on first build)"
fi
echo ""

# ========== 汇总 ==========
echo "══════════════════════════════════════════════════════════════════"
echo "Diagnostic Summary"
echo "══════════════════════════════════════════════════════════════════"
echo "  ✓ Success: $SUCCESS"
echo "  ✗ Failed:  $FAILED"
echo ""

if [ $FAILED -gt 0 ]; then
    echo "❌ Setup has issues! Please fix the missing files."
    echo ""
    echo "Common fixes:"
    echo ""
    
    # 检查 STX B+Tree
    if [ ! -d "include/structures/btree/stx-btree-0.9" ]; then
        echo "1. STX B+Tree Missing"
        echo "   If you have SOSD project:"
        echo "     ln -s /path/to/SOSD/competitors/stx-btree-0.9 include/structures/btree/"
        echo ""
        echo "   Or download it:"
        echo "     cd include/structures/btree/"
        echo "     wget https://idlebox.net/files/stx-btree/stx-btree-0.9.tar.bz2"
        echo "     tar xjf stx-btree-0.9.tar.bz2"
        echo ""
    fi
    
    # 检查算法文件
    if [ ! -f "include/structures/btree/algorithms/NoPrefetch.hpp" ]; then
        echo "2. Algorithm Files Missing"
        echo "   Please create the algorithm files in:"
        echo "     include/structures/btree/algorithms/"
        echo ""
    fi
    
    exit 1
else
    echo "✅ All required files are present!"
    echo ""
    echo "Next steps:"
    echo "  1. Build the project:"
    echo "     ./run_prefetch_benchmark.sh quick"
    echo ""
    echo "  2. Or manually:"
    echo "     mkdir -p build && cd build"
    echo "     cmake .. && make -j$(nproc)"
    echo "     cd .."
    echo ""
    exit 0
fi