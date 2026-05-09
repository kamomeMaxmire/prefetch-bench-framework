import os
import re
import sys

import matplotlib.pyplot as plt
import pandas as pd


BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _parse_strategy(strategy):
    strategy = strategy.strip()

    if strategy in {"LIPP (No Prefetch)", "No Prefetch"}:
        return "No Prefetch", None, 0

    match = re.search(r"LIPP\s*\(([^,]+),\s*([A-Z])=(\d+)\)", strategy)
    if not match:
        return strategy, None, 0

    raw_name = match.group(1).strip()
    param_letter = match.group(2)
    param_value = int(match.group(3))

    name_mapping = {
        "GrpPref": "Group Prefetch",
        "SPP": "SPP",
        "Vectorized": "Vectorized",
        "FSM AMAC": "FSM AMAC",
    }
    return name_mapping.get(raw_name, raw_name), param_letter, param_value


def parse_lipp_results(file_path):
    """
    Parse LIPP benchmark output.

    Supports both final table rows:
      ║ LIPP (...) 123.4 123.4 1.23 1.00x ║

    and progress lines:
      Testing LIPP (...) ... Done (Avg: 123.45 ms)

    The progress-line parser is useful when the benchmark output is incomplete
    and the final table was not printed.
    """
    rows = []
    seen = set()

    table_pattern = re.compile(
        r"║\s*(?P<strategy>.+?)\s+"
        r"(?P<avg_time>\d+\.?\d*)\s+"
        r"(?P<latency>\d+\.?\d*)\s+"
        r"(?P<throughput>\d+\.?\d*)\s+"
        r"(?P<speedup>\d+\.?\d*)x\s*║"
    )
    progress_pattern = re.compile(
        r"Testing\s+(?P<strategy>LIPP\s*\([^)]*\))\s+.*?"
        r"Done\s+\(Avg:\s*(?P<avg_time>\d+\.?\d*)\s*ms\)"
    )

    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            source = "table"
            match = table_pattern.search(line)
            if not match:
                source = "progress"
                match = progress_pattern.search(line)
            if not match:
                continue

            strategy = match.group("strategy").strip()
            if not strategy.startswith("LIPP") and strategy != "No Prefetch":
                continue

            avg_time = float(match.group("avg_time"))
            if source == "table":
                latency = float(match.group("latency"))
                throughput = float(match.group("throughput"))
            else:
                # This benchmark uses 1M queries, so ms and ns/op have the same
                # numeric value. Throughput is Mops/s = 1000 / ms.
                latency = avg_time
                throughput = 1000.0 / avg_time if avg_time > 0 else 0.0

            strategy_group, param_letter, param_value = _parse_strategy(strategy)
            key = (strategy_group, param_letter, param_value)
            if key in seen:
                # Prefer the first occurrence. In complete files, the progress
                # lines appear before the table but carry the same avg time.
                continue
            seen.add(key)

            rows.append({
                "strategy": strategy,
                "strategy_group": strategy_group,
                "param_letter": param_letter,
                "parameter": param_value,
                "avg_time_ms": avg_time,
                "latency": latency,
                "throughput": throughput,
            })

    df = pd.DataFrame(rows)
    if df.empty:
        return df

    order = {
        "No Prefetch": 0,
        "Group Prefetch": 1,
        "SPP": 2,
        "Vectorized": 3,
        "FSM AMAC": 4,
    }
    df["strategy_order"] = df["strategy_group"].map(order).fillna(99)
    return df.sort_values(["strategy_order", "parameter"]).reset_index(drop=True)


def parse_lipp_build_info(file_path):
    info = {}
    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            total_match = re.search(r"Total Keys:\s*(\d+)", line)
            if total_match:
                info["total_keys"] = int(total_match.group(1))
                continue

            build_match = re.search(r"Build Time:\s*([\d.]+)\s*ms", line)
            if build_match:
                info["build_time_ms"] = float(build_match.group(1))
                continue

            size_match = re.search(r"Index Size:\s*(\d+)\s*Bytes\s*\(([\d.]+)\s*MB\)", line)
            if size_match:
                info["index_size_bytes"] = int(size_match.group(1))
                info["index_size_mb"] = float(size_match.group(2))
                continue

            if "Verifying:" in line and "OK" in line:
                info["verify"] = "OK"

    return info


def analyze_and_visualize(df, build_info, output_dir):
    if df.empty:
        print("Data is empty, cannot generate plots.")
        return

    os.makedirs(output_dir, exist_ok=True)

    plt.style.use("seaborn-v0_8-whitegrid" if "seaborn-v0_8-whitegrid" in plt.style.available else "ggplot")
    plt.rcParams["figure.figsize"] = (12, 8)
    plt.rcParams["axes.titlesize"] = 18
    plt.rcParams["axes.labelsize"] = 14
    plt.rcParams["xtick.labelsize"] = 12
    plt.rcParams["ytick.labelsize"] = 12
    plt.rcParams["legend.fontsize"] = 13

    data_to_plot = df[df["strategy_group"] != "No Prefetch"]
    if data_to_plot.empty:
        print("No parameterized strategy data found.")
        return

    max_x = int(data_to_plot["parameter"].max())
    clean_ticks = [2 ** i for i in range(30) if 2 ** i <= max_x]
    line_colors = {
        "Group Prefetch": "#3b82f6",
        "SPP": "#10b981",
        "Vectorized": "#f59e0b",
        "FSM AMAC": "#ef4444",
    }

    def plot_strategy_lines(y_column, title, ylabel, output_name):
        plt.figure()
        ax = plt.gca()
        for group, group_df in data_to_plot.groupby("strategy_group"):
            group_df = group_df.sort_values("parameter")
            ax.plot(
                group_df["parameter"],
                group_df[y_column],
                marker="o",
                linewidth=2.5,
                markersize=7,
                label=group,
                color=line_colors.get(group),
            )
        ax.set_xscale("log", base=2)
        ax.set_xticks(clean_ticks)
        ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
        plt.xticks(rotation=30)
        plt.title(title, fontweight="bold")
        plt.xlabel("Concurrency Parameter (G, D, V, P)")
        plt.ylabel(ylabel)
        plt.legend(title="Strategy")
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.tight_layout()
        out_path = os.path.join(output_dir, output_name)
        plt.savefig(out_path, dpi=300)
        plt.close()
        print(f"Plot saved to: {out_path}")

    # Throughput vs parameter
    plot_strategy_lines(
        "throughput",
        "LIPP Throughput vs. Concurrency Parameter",
        "Throughput (M Ops/sec)",
        "LIPP_throughput_vs_concurrency.png",
    )

    # Latency vs parameter
    plot_strategy_lines(
        "latency",
        "LIPP Latency vs. Concurrency Parameter",
        "Average Latency (ns/op)",
        "LIPP_latency_vs_concurrency.png",
    )

    # Peak throughput by strategy
    peak = df.loc[df.groupby("strategy_group")["throughput"].idxmax()].copy()

    plt.figure()
    peak_by_throughput = peak.sort_values("throughput", ascending=False)
    ax = plt.gca()
    bars = ax.bar(
        peak_by_throughput["strategy_group"],
        peak_by_throughput["throughput"],
        color=["#f97316", "#0ea5e9", "#22c55e", "#a855f7", "#ef4444"][:len(peak_by_throughput)],
    )
    for p in bars:
        ax.annotate(
            f"{p.get_height():.2f} M",
            (p.get_x() + p.get_width() / 2.0, p.get_height()),
            ha="center",
            va="center",
            xytext=(0, 9),
            textcoords="offset points",
            fontweight="bold",
        )
    plt.title("LIPP Peak Throughput by Strategy", fontweight="bold")
    plt.xlabel("Strategy")
    plt.ylabel("Peak Throughput (M Ops/sec)")
    plt.xticks(rotation=15, ha="right")
    plt.tight_layout()
    out = os.path.join(output_dir, "LIPP_peak_throughput_comparison.png")
    plt.savefig(out, dpi=300)
    plt.close()
    print(f"Plot saved to: {out}")

    # Latency at peak throughput
    plt.figure()
    peak_by_latency = peak.sort_values("latency")
    ax = plt.gca()
    bars = ax.bar(
        peak_by_latency["strategy_group"],
        peak_by_latency["latency"],
        color=["#14b8a6", "#6366f1", "#f59e0b", "#64748b", "#ef4444"][:len(peak_by_latency)],
    )
    for p in bars:
        ax.annotate(
            f"{p.get_height():.1f} ns",
            (p.get_x() + p.get_width() / 2.0, p.get_height()),
            ha="center",
            va="center",
            xytext=(0, 9),
            textcoords="offset points",
            fontweight="bold",
        )
    plt.title("LIPP Latency at Peak Throughput", fontweight="bold")
    plt.xlabel("Strategy")
    plt.ylabel("Average Latency (ns/op)")
    plt.xticks(rotation=15, ha="right")
    plt.tight_layout()
    out = os.path.join(output_dir, "LIPP_latency_at_peak_throughput.png")
    plt.savefig(out, dpi=300)
    plt.close()
    print(f"Plot saved to: {out}")

    print("\n" + "=" * 25 + " LIPP Summary " + "=" * 25)
    if build_info:
        if "total_keys" in build_info:
            print(f"Total keys: {build_info['total_keys']:,}")
        if "build_time_ms" in build_info:
            print(f"Build time: {build_info['build_time_ms']:.2f} ms")
        if "index_size_mb" in build_info:
            print(f"Index size: {build_info['index_size_mb']:.2f} MB")
        if "verify" in build_info:
            print(f"Verify: {build_info['verify']}")

    best = peak.loc[peak["throughput"].idxmax()]
    print(f"\nBest strategy: {best['strategy_group']}")
    if best["parameter"] > 0:
        print(f"Best parameter: {int(best['parameter'])}")
    print(f"Peak throughput: {best['throughput']:.2f} M Ops/sec")
    print(f"Latency at peak: {best['latency']:.1f} ns/op")

    expected = {"No Prefetch", "Group Prefetch", "SPP", "Vectorized", "FSM AMAC"}
    present = set(df["strategy_group"].unique())
    missing = sorted(expected - present)
    if missing:
        print(f"\nWarning: missing strategy groups: {', '.join(missing)}")

    # Detect likely incomplete files by checking whether the last expected large
    # parameter appears for all parameterized strategies.
    incomplete = []
    for group in ["Group Prefetch", "SPP", "Vectorized", "FSM AMAC"]:
        values = set(df.loc[df["strategy_group"] == group, "parameter"].astype(int))
        if values and 8192 not in values:
            incomplete.append(group)
    if incomplete:
        print(f"Warning: result file may be incomplete for: {', '.join(incomplete)}")
    print("=" * 64 + "\n")


if __name__ == "__main__":
    results_file = os.path.join(BASE_DIR, "results", "LIPP_benchmark.txt")
    if len(sys.argv) > 1:
        results_file = sys.argv[1]

    if not os.path.exists(results_file):
        print(f"Error: Results file '{results_file}' not found.")
        sys.exit(1)

    dataframe = parse_lipp_results(results_file)
    info = parse_lipp_build_info(results_file)
    plots_dir = os.path.join(BASE_DIR, "analysis", "plots")
    analyze_and_visualize(dataframe, info, plots_dir)
