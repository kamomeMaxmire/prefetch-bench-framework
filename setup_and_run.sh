#!/bin/bash

# B+ Tree Benchmark - Setup and Run Script

set -e

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║         B+ Tree Performance Benchmark - Setup & Run             ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# ========== 步骤 1: 检查 STX B+ Tree ==========
echo "[1] Checking for STX B+ Tree..."

STX_FOUND=0
STX_LOCATION=""

# 检查各种可能的位置
if [ -d "../SOSD/competitors/stx-btree-0.9/include/stx" ]; then
    echo "  ✓ Found in: ../SOSD/competitors/stx-btree-0.9/include"
    STX_FOUND=1
    STX_LOCATION="../SOSD"
fi

# ========== 步骤 2: 检查数据文件 ==========
echo ""
echo "[2] Checking for data files..."

# 检查 data 目录
if [ ! -d "data" ]; then
    mkdir -p data
    echo "  Created data/ directory"
fi

# 数据文件列表
DATA_FILES=(
    "books_200M_uint64"
    "osm_cellids_200M_uint64"
    "wiki_ts_200M_uint64"
    "fb_200M_uint64"
)

FOUND_COUNT=0

# 在多个位置查找
for file in "${DATA_FILES[@]}"; do
    if [ -f "$file" ] || [ -f "data/$file" ]; then
        echo "  ✓ Found: $file"
        FOUND_COUNT=$((FOUND_COUNT + 1))
   
    fi
done


# ========== 步骤 3: 编译 ==========
echo ""
echo "[3] Building project..."

mkdir -p build
cd build

echo "  Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "  Compiling..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "  ✗ Build failed!"
    exit 1
fi

echo "  ✓ Build successful"
cd ..

# ========== 步骤 4: 运行测试 ==========
echo ""
echo "[4] Running benchmark..."
echo "════════════════════════════════════════════════════════════════════"
echo ""

DATASET=${1:-books_200M}
MODE=${2:-sosd}

# 检查数据文件是否存在
if [ -f "${DATASET}_uint64" ] || [ -f "data/${DATASET}_uint64" ]; then
    ./build/prefetch_bench $DATASET
elif [ "$MODE" == "synthetic" ] || [ "$1" == "synthetic" ]; then
    ./build/prefetch_bench synthetic
elif [ "$1" == "compare" ]; then
    ./build/prefetch_bench compare
elif [ "$1" == "all" ]; then
    ./build/prefetch_bench all
else
    echo "  Data file not found, running with synthetic data..."
    ./build/prefetch_bench synthetic
fi

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "✓ Setup and benchmark completed!"
echo ""
echo "Usage:"
echo "  ./setup_and_run.sh [dataset]     # Test SOSD dataset"
echo "  ./setup_and_run.sh synthetic     # Test synthetic data"
echo "  ./setup_and_run.sh compare       # Compare configurations"
echo "  ./setup_and_run.sh all           # Run all tests"
echo ""
echo "For performance analysis:"
echo "  ./run_perf.sh [dataset]"
echo ""
echo "Available datasets:"
echo "  books_200M, osm_cellids_200M, wiki_ts_200M, fb_200M"