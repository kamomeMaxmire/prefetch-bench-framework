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

            # 跳过 Part 1 的 Eps 缩放数据，我们只要 Part 2 和 No Prefetch
            if 'Eps=' in strategy:
                continue

            # 【核心修复点】：去掉死板的括号限制，直接寻找 "字母=数字" 的组合
            param_match = re.search(r'([A-Z])=(\d+)', strategy)
            if param_match:
                param_letter = param_match.group(1)
                param_value = int(param_match.group(2))
            else:
                param_letter = None
                param_value = 0

            # 提取括号里逗号前的真实策略名
            if strategy == 'No Prefetch':
                strategy_group = 'No Prefetch'
            else:
                algo_match = re.search(r'\(([^,]+),', strategy)
                if algo_match:
                    raw_name = algo_match.group(1).strip()
                    # 映射回漂亮的全称，保持图例统一
                    name_mapping = {
                        'GrpPref': 'Group Prefetch',
                        'StaticSPP': 'SPP',
                        'Vectorized': 'Vectorized',
                        'FSM AMAC': 'FSM AMAC'
                    }
                    strategy_group = name_mapping.get(raw_name, raw_name)
                else:
                    strategy_group = strategy

            # ========== SPP 物理并发度还原 ==========
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

def parse_epsilon_data(file_path):
    """
    解析 Epsilon 测试块，提取建树时间、索引大小和无预取查询延迟。
    """
    eps_data = []
    current_eps = None
    current_build = None
    current_size = None

    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            eps_match = re.search(r'Building PGM-Index \(Epsilon =\s*(\d+)\s*\)', line)
            if eps_match:
                current_eps = int(eps_match.group(1))
                continue

            build_match = re.search(r'Build Time:\s*([\d.]+)\s*ms', line)
            if build_match and current_eps is not None:
                current_build = float(build_match.group(1))
                continue

            size_match = re.search(r'Index Size:\s*\d+\s*Bytes\s*\(([\d.]+)\s*MB\)', line)
            if size_match and current_eps is not None:
                current_size = float(size_match.group(1))
                continue

            nopref_match = re.search(r'Testing PGM \(Eps=\d+, NoPref\).*?Done \(Avg:\s*([\d.]+)\s*ms\)', line)
            if nopref_match and current_eps is not None:
                latency_ns = float(nopref_match.group(1)) 
                eps_data.append({
                    'Epsilon': current_eps,
                    'Build Time (ms)': current_build,
                    'Index Size (MB)': current_size,
                    'Latency (ns/op)': latency_ns
                })
                current_eps = None 

    return pd.DataFrame(eps_data)

def visualize_epsilon_scaling(df_eps, output_dir):
    """
    生成 Epsilon 对 PGM 的影响分析图（1x3子图组合）
    """
    if df_eps.empty:
        return

    sns.set_theme(style="whitegrid")
    fig, axes = plt.subplots(1, 3, figsize=(18, 5.5))
    eps_values = sorted(df_eps['Epsilon'].unique())

    sns.lineplot(data=df_eps, x='Epsilon', y='Index Size (MB)', marker='s', color='teal', ax=axes[0], linewidth=2.5, markersize=8)
    axes[0].set_title('Index Size vs. Epsilon', fontweight='bold', fontsize=15)
    axes[0].set_ylabel('Index Size (MB)', fontsize=12)

    sns.lineplot(data=df_eps, x='Epsilon', y='Build Time (ms)', marker='o', color='coral', ax=axes[1], linewidth=2.5, markersize=8)
    axes[1].set_title('Build Time vs. Epsilon', fontweight='bold', fontsize=15)
    axes[1].set_ylabel('Build Time (ms)', fontsize=12)

    sns.lineplot(data=df_eps, x='Epsilon', y='Latency (ns/op)', marker='^', color='indigo', ax=axes[2], linewidth=2.5, markersize=8)
    axes[2].set_title('Query Latency vs. Epsilon (No Prefetch)', fontweight='bold', fontsize=15)
    axes[2].set_ylabel('Average Latency (ns/op)', fontsize=12)

    for ax in axes:
        ax.set_xscale('log', base=2)
        ax.set_xticks(eps_values)
        ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
        ax.set_xlabel('Epsilon (ε)', fontsize=13)
        ax.tick_params(axis='x', rotation=0)

    plt.tight_layout()
    file_path = os.path.join(output_dir, 'PGM_epsilon_scaling.png')
    plt.savefig(file_path, dpi=300)
    plt.close()
    print(f"Plot saved to: {file_path}")

def analyze_and_visualize(df, output_dir='analysis/plots'):
    """
    生成并发度可视化图表
    """
    if df.empty:
        print("Data is empty, cannot generate plots.")
        return

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    sns.set_theme(style="whitegrid", palette="viridis")
    plt.rcParams['figure.figsize'] = (12, 8)
    plt.rcParams['axes.titlesize'] = 18
    plt.rcParams['axes.labelsize'] = 14
    plt.rcParams['xtick.labelsize'] = 12
    plt.rcParams['ytick.labelsize'] = 12
    plt.rcParams['legend.fontsize'] = 13

    data_to_plot = df[~df['strategy_group'].str.contains('No Prefetch', na=False)]

    if data_to_plot.empty:
        print("Warning: No valid concurrency data to plot lines.")
        return

    # --- 1. 吞吐量 vs. 并发度 ---
    plt.figure()
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
    
    max_x = int(data_to_plot['parameter'].max())
    clean_ticks = [2**i for i in range(30) if 2**i <= max_x * 2]
    ax.set_xticks(clean_ticks)
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    plt.xticks(rotation=30)
    
    plt.title('Throughput vs. Concurrency Parameter (Log Scale X)', fontweight='bold')
    plt.xlabel('Total In-flight Queries (Concurrency Level)')
    plt.ylabel('Throughput (M Ops/sec)')
    plt.legend(title='Prefetch Strategy')
    plt.grid(True, which="both", ls="--")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'PGM_throughput_vs_concurrency.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'PGM_throughput_vs_concurrency.png')}")

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
    ax.set_xticks(clean_ticks)
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    plt.xticks(rotation=30)

    plt.title('Latency vs. Concurrency Parameter (Log Scale X)', fontweight='bold')
    plt.xlabel('Concurrency Parameter (G, D, V, P)')
    plt.ylabel('Average Latency (ns/op)')
    plt.legend(title='Prefetch Strategy')
    plt.grid(True, which="both", ls="--")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'PGM_latency_vs_concurrency.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'PGM_latency_vs_concurrency.png')}")

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
    plt.savefig(os.path.join(output_dir, 'PGM_peak_throughput_comparison.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'PGM_peak_throughput_comparison.png')}")

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
    plt.savefig(os.path.join(output_dir, 'PGM_latency_at_peak_throughput.png'), dpi=300)
    plt.close()
    print(f"Plot saved to: {os.path.join(output_dir, 'PGM_latency_at_peak_throughput.png')}")

if __name__ == '__main__':
    # 读取 PGM 的基准测试结果文件
    results_file = os.path.join(BASE_DIR, 'results', 'PGM_benchmark.txt')
    
    if not os.path.exists(results_file):
        print(f"Error: Results file '{results_file}' not found.")
    else:
        plots_dir = os.path.join(BASE_DIR, 'analysis', 'plots','PGM')
        if not os.path.exists(plots_dir):
            os.makedirs(plots_dir)

        # 1. 解析并画 Epsilon 图
        df_eps = parse_epsilon_data(results_file)
        if not df_eps.empty:
            visualize_epsilon_scaling(df_eps, plots_dir)

        # 2. 解析并画并发度基准图
        dataframe = parse_benchmark_results(results_file)
        if not dataframe.empty:
            analyze_and_visualize(dataframe, output_dir=plots_dir)