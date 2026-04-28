import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import re
import os
import sys

# 获取项目根目录（脚本所在目录的上一级）
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def parse_benchmark_results(file_path):
    """
    解析基准测试的输出文件，并提取性能数据。
    """
    results = []
    # 匹配整行：║ 策略名（可能含参数） 数值1 数值2 数值3 加速比x ║
    pattern = re.compile(
        r"║\s*(?P<strategy>.+?)\s+(?P<avg_time>\d+\.?\d*)\s+(?P<latency>\d+\.?\d*)\s+(?P<throughput>\d+\.?\d*)\s+(?P<speedup>\d+\.?\d*)x\s*║"
    )

    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            m = pattern.search(line)
            if not m:
                continue
            data = m.groupdict()
            strategy = data['strategy'].strip()

            # 提取参数，例如 (G=64) -> param_letter='G', param_value=64
            param_match = re.search(r'\(([A-Z])=(\d+)\)', strategy)
            if param_match:
                param_letter = param_match.group(1)
                param_value = int(param_match.group(2))
            else:
                param_letter = None
                param_value = 0   # 用于 No Prefetch 等无参数策略

            # 策略组名（去掉括号内容）
            strategy_group = re.sub(r'\s*\([^)]*\)', '', strategy).strip()

            # ========== 在这里加入转换逻辑 ==========
            # 因为 SPP 的真实并发度是 D * H，这里的 H 在你的 C++ 中算出来是 7
            if strategy_group == 'SPP' and param_letter == 'D':
                param_value = param_value * 7
            # ========================================

            results.append({
                'strategy_group': strategy_group,
                'parameter': param_value,
                'throughput': float(data['throughput']),
                'latency': float(data['latency'])
            })

    return pd.DataFrame(results)

def analyze_and_visualize(df, output_dir='analysis/plots'):
    """
    根据基准测试数据生成并保存图表。
    """
    if df.empty:
        print("Data is empty, cannot generate plots.")
        return

    # 如果输出目录不存在，则创建它
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # 设置图表样式
    sns.set_theme(style="whitegrid", palette="viridis")
    plt.rcParams['figure.figsize'] = (12, 8)
    plt.rcParams['axes.titlesize'] = 18
    plt.rcParams['axes.labelsize'] = 14
    plt.rcParams['xtick.labelsize'] = 12
    plt.rcParams['ytick.labelsize'] = 12
    plt.rcParams['legend.fontsize'] = 13

    # --- 1. 吞吐量 vs. 并发度 ---
    plt.figure()
    data_to_plot = df[df['strategy_group'] != 'No Prefetch']
    
    ax = sns.lineplot(
        data=data_to_plot, 
        x='parameter', 
        y='throughput', 
        hue='strategy_group', 
        marker='o',
        linewidth=2.5,
        markersize=8
    )
    ax.set_xscale('log', base=2)
    
    # 动态生成纯净的 2的幂次方 刻度 (1, 2, 4, 8, 16 ... 直到最大值)
    max_x = int(data_to_plot['parameter'].max())
    clean_ticks = [2**i for i in range(30) if 2**i <= max_x * 2]
    
    ax.set_xticks(clean_ticks)
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    
    # 可选：如果后面的数字(比如 65536)太长，让它稍微倾斜一点点更美观
    plt.xticks(rotation=30)
    
    plt.title('Throughput vs. Concurrency Parameter (Log Scale X)', fontweight='bold')
    plt.xlabel('Total In-flight Queries (Concurrency Level)')
    plt.ylabel('Throughput (M Ops/sec)')
    plt.legend(title='Prefetch Strategy')
    plt.grid(True, which="both", ls="--")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'btree_throughput_vs_concurrency.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'btree_throughput_vs_concurrency.png')}")

    # --- 2. 延迟 vs. 并发度 ---
    plt.figure()
    ax = sns.lineplot(
        data=data_to_plot, 
        x='parameter', 
        y='latency', 
        hue='strategy_group', 
        marker='o',
        linewidth=2.5,
        markersize=8
    )
    ax.set_xscale('log', base=2)
    
    # 动态生成纯净的 2的幂次方 刻度 (1, 2, 4, 8, 16 ... 直到最大值)
    max_x = int(data_to_plot['parameter'].max())
    clean_ticks = [2**i for i in range(30) if 2**i <= max_x * 2]
    
    ax.set_xticks(clean_ticks)
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    
    # 可选：如果后面的数字(比如 65536)太长，让它稍微倾斜一点点更美观
    plt.xticks(rotation=30)

    plt.title('Latency vs. Concurrency Parameter (Log Scale X)', fontweight='bold')
    plt.xlabel('Concurrency Parameter (G, D, V, P)')
    plt.ylabel('Average Latency (ns/op)')
    plt.legend(title='Prefetch Strategy')
    plt.grid(True, which="both", ls="--")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'btree_latency_vs_concurrency.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'btree_latency_vs_concurrency.png')}")

    # --- 3. 峰值吞吐量对比 ---
    peak_throughput = df.loc[df.groupby('strategy_group')['throughput'].idxmax()]
    
    plt.figure()
    ax = sns.barplot(data=peak_throughput.sort_values('throughput', ascending=False), x='strategy_group', y='throughput', palette='rocket')
    
    for p in ax.patches:
        ax.annotate(f"{p.get_height():.2f} M", (p.get_x() + p.get_width() / 2., p.get_height()), 
                    ha='center', va='center', xytext=(0, 9), textcoords='offset points', fontweight='bold')

    plt.title('Peak Throughput Comparison by Strategy', fontweight='bold')
    plt.xlabel('Prefetch Strategy')
    plt.ylabel('Peak Throughput (M Ops/sec)')
    plt.xticks(rotation=15, ha='right')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'btree_peak_throughput_comparison.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'btree_peak_throughput_comparison.png')}")

    # --- 4. 峰值吞吐量下的延迟对比 ---
    plt.figure()
    ax = sns.barplot(data=peak_throughput.sort_values('latency'), x='strategy_group', y='latency', palette='mako')

    for p in ax.patches:
        ax.annotate(f"{p.get_height():.1f} ns", (p.get_x() + p.get_width() / 2., p.get_height()),
                    ha='center', va='center', xytext=(0, 9), textcoords='offset points', fontweight='bold')

    plt.title('Latency at Peak Throughput', fontweight='bold')
    plt.xlabel('Prefetch Strategy')
    plt.ylabel('Average Latency (ns/op)')
    plt.xticks(rotation=15, ha='right')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'btree_latency_at_peak_throughput.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'btree_latency_at_peak_throughput.png')}")
    
    # --- 打印总结 ---
    print("\n" + "="*25 + " Performance Summary " + "="*25)
    best_overall = peak_throughput.loc[peak_throughput['throughput'].idxmax()]
    print(f"\n Best Strategy: '{best_overall['strategy_group']}'")
    if best_overall['parameter'] > 0:
        print(f"  - Best Parameter: {best_overall['parameter']}")
    print(f"  - Peak Throughput: {best_overall['throughput']:.2f} M Ops/sec")
    print(f"  - Latency at Peak: {best_overall['latency']:.1f} ns/op")
    print("\n" + "="*62 + "\n")

if __name__ == '__main__':
    results_file = os.path.join(BASE_DIR, 'results', 'btree_benchmark.txt')
    
    if not os.path.exists(results_file):
        print(f"Error: Results file '{results_file}' not found.")
        print("Please run the benchmark first and save the output to this file.")
    else:
        dataframe = parse_benchmark_results(results_file)
        # 让图表输出到 analysis/plots（相对于项目根目录）
        plots_dir = os.path.join(BASE_DIR, 'analysis', 'plots')
        analyze_and_visualize(dataframe, output_dir=plots_dir)