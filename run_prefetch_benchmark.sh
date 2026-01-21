#!/bin/bash

# B+ Tree Prefetch Benchmark - Run Script

set -e

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║         B+ Tree Prefetch Performance Benchmark                   ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# ========== 步骤 1: 编译 ==========
echo "[1] Building project..."

if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "  ✗ Build failed!"
    exit 1
fi

echo "  ✓ Build successful"
cd ..

# ========== 步骤 2: 检查数据文件 ==========
echo ""
echo "[2] Checking for data files..."

DATA_FILES=(
    "books_200M_uint64"
    "osm_cellids_200M_uint64"
    "wiki_ts_200M_uint64"
    "fb_200M_uint64"
)

FOUND_DATA=false
for file in "${DATA_FILES[@]}"; do
    if [ -f "data/$file" ]; then
        echo "  ✓ Found: $file"
        FOUND_DATA=true
    fi
done

# ========== 步骤 3: 运行 Benchmark ==========
echo ""
echo "[3] Running prefetch benchmark..."
echo "════════════════════════════════════════════════════════════════════"
echo ""

MODE=${1:-all}

case $MODE in
    "all")
        echo "🔥 Running ALL prefetch tests on all datasets..."
        echo ""
        
        for dataset in "${DATA_FILES[@]}"; do
            dataset_name="${dataset/_uint64/}"
            
            if [ -f "data/$dataset" ]; then
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                echo "Testing: $dataset_name"
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                ./build/prefetch_compare "$dataset_name"
                echo ""
                sleep 2
            fi
        done
        ;;
        
    "quick")
        echo "⚡ Quick test on books_200M..."
        echo ""
        
        if [ -f "data/books_200M_uint64" ]; then
            ./build/prefetch_compare books_200M
        elif [ -f "data/custom_random_200000000_uint64" ]; then
            ./build/prefetch_compare custom_random_200000000
        else
            echo "  ⚠ No data files found. Generating synthetic data..."
            ./build/prefetch_compare generate 10000000 random
            ./build/prefetch_compare custom_random_10000000
        fi
        ;;
        
    "generate")
        SIZE=${2:-10000000}
        TYPE=${3:-random}
        
        echo "📊 Generating synthetic dataset..."
        echo "  Size: $SIZE"
        echo "  Type: $TYPE"
        echo ""
        
        ./build/prefetch_compare generate $SIZE $TYPE
        
        DATASET_NAME="custom_${TYPE}_${SIZE}"
        
        echo ""
        echo "✓ Dataset generated!"
        echo ""
        echo "To test it, run:"
        echo "  ./run_prefetch_benchmark.sh $DATASET_NAME"
        exit 0
        ;;
        
    *)
        # 测试指定数据集
        DATASET=$MODE
        
        if [ -f "data/${DATASET}_uint64" ] || [ -f "data/$DATASET" ]; then
            echo "🎯 Testing dataset: $DATASET"
            echo ""
            ./build/prefetch_compare "$DATASET"
        else
            echo "  ✗ Dataset not found: $DATASET"
            echo ""
            echo "Available datasets:"
            for file in "${DATA_FILES[@]}"; do
                if [ -f "data/$file" ]; then
                    echo "  - ${file/_uint64/}"
                fi
            done
            exit 1
        fi
        ;;
esac

echo ""
echo "════════════════════════════════════════════════════════════════════"
echo "✓ Benchmark completed!"
echo ""
echo "Usage:"
echo "  ./run_prefetch_benchmark.sh [mode]"
echo ""
echo "Modes:"
echo "  all                          # Test all datasets"
echo "  quick                        # Quick test (single dataset)"
echo "  <dataset_name>               # Test specific dataset"
echo "  generate <size> <type>       # Generate synthetic data"
echo ""
echo "Examples:"
echo "  ./run_prefetch_benchmark.sh all"
echo "  ./run_prefetch_benchmark.sh books_200M"
echo "  ./run_prefetch_benchmark.sh generate 200000000 random"
echo ""
echo "For performance analysis:"
echo "  sudo perf stat -r 3 ./build/prefetch_compare books_200M"
echo ""