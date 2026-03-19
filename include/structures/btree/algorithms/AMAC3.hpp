#pragma once
#include <vector>
#include <string>
#include <cstdint>

// =======================================================================
// AMAC.hpp  —  Asynchronous Memory Access Chaining
//
// 接口与 NoPrefetch / GroupPrefetch / SPP / Vectorized 完全一致：
//
//   algorithms::AMACSearch<BTreeType>::batch_lookup<POOL_SIZE>(
//       tree_, queries, results
//   );
//
// 核心机制（乱序调度）：
//   1. 双队列：pending（等 prefetch 生效）/ ready（可立即执行）
//   2. 就绪检测：RDTSC 测量读取目标 cache line 的耗时，
//      耗时 < L2_HIT_CYCLES 则认为数据已就绪；
//      同时用 tick 距离兜底，避免非 x86 平台或测量误差导致活锁。
//   3. 乱序提升：pending 里谁先就绪谁先进 ready，不按入队顺序。
//
// 扩展到其他结构（Hash Table / LSM-Tree / io_uring）：
//   只需修改 AMACTask 中的 step() 和 prefetch_next()，
//   调度引擎 AMACSearch 完全不用改。
// =======================================================================

namespace structures {
namespace btree {
namespace algorithms {

// -----------------------------------------------------------------------
// 平台相关：用 RDTSC 探测一个地址是否已进入 L1/L2 cache
// -----------------------------------------------------------------------
namespace detail {

static constexpr uint64_t L2_HIT_CYCLES = 50;  // 典型 L2 命中 ≤ 40 cycles

static inline bool is_cache_ready(const void* addr) {
#if defined(__x86_64__)
    uint32_t lo1, hi1, lo2, hi2;
    uint8_t  sink;          // 承接读取结果，防止编译器优化掉
    __asm__ volatile (
        "mfence\n\t"
        "rdtsc\n\t"
        "mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        "movb (%4), %3\n\t"
        "mfence\n\t"
        "rdtsc\n\t"
        "mov %%eax, %2\n\t"
        : "=r"(lo1), "=r"(hi1), "=r"(lo2), "=r"(sink)
        : "r"(addr)
        : "eax", "edx", "memory"
    );
    // hi2 还没读，用单独 rdtsc 补一次（避免 4 输出约束问题）
    __asm__ volatile ("rdtsc" : "=a"(lo2), "=d"(hi2));
    uint64_t t1 = ((uint64_t)hi1 << 32) | lo1;
    uint64_t t2 = ((uint64_t)hi2 << 32) | lo2;
    return (t2 - t1) < L2_HIT_CYCLES;
#else
    (void)addr;
    return false;   // 非 x86：保守返回 false，完全依赖 tick 兜底
#endif
}

} // namespace detail

// -----------------------------------------------------------------------
// AMACTask：单次查询的完整上下文
//
// 与具体 BTree 类型解耦：调度器只调用 step() 和 next_addr()，
// 不直接接触 BTree 内部。
// -----------------------------------------------------------------------
template<typename BTree>
struct AMACTask {
    using Key       = typename BTree::key_type;
    using Value     = typename BTree::data_type;
    using Node      = typename BTree::node;
    using InnerNode = typename BTree::inner_node;
    using LeafNode  = typename BTree::leaf_node;

    size_t      query_idx;
    Key         key;
    const Node* curr_node;  // 当前要处理的节点（数据已在 cache）
    const Node* next_node;  // 已发出 prefetch、等待就绪的下一节点
    bool        done;

    // ------------------------------------------------------------------
    // step()：在 curr_node 上执行一步计算
    //   - 若是叶子：收割结果，设 done=true，返回 true
    //   - 若是内部节点：计算 child，发 prefetch，设 next_node，返回 false
    //     调用方需等 next_node 就绪后调用 advance()，再调用下一次 step()
    // ------------------------------------------------------------------
    bool step(Value* results, std::vector<bool>& found) {
        if (curr_node->isleafnode()) {
            const LeafNode* leaf = static_cast<const LeafNode*>(curr_node);
            int slot = find_lower(leaf, key);
            if (slot < leaf->slotuse && key == leaf->slotkey[slot]) {
                if (!BTree::traits::selfverify)
                    results[query_idx] = leaf->slotdata[slot];
                found[query_idx] = true;
            }
            done = true;
            return true;
        }

        const InnerNode* inner = static_cast<const InnerNode*>(curr_node);
        int slot  = find_lower(inner, key);
        next_node = inner->childid[slot];

        // 异步发射 prefetch，立即返回，不等数据
        __builtin_prefetch(next_node, 0, 3);
        __builtin_prefetch(reinterpret_cast<const char*>(next_node) + 64, 0, 3);
        return false;
    }

    // next_node 就绪后，调用 advance() 滑动指针
    void advance() {
        curr_node = next_node;
        next_node = nullptr;
    }

    // next_node 的地址（供调度器做就绪检测）
    const void* next_addr() const { return next_node; }

private:
    template<typename NodeType>
    static inline int find_lower(const NodeType* n, const Key& k) {
        if (n->slotuse == 0) return 0;
        int lo = 0, hi = n->slotuse;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (k <= n->slotkey[mid]) hi = mid;
            else lo = mid + 1;
        }
        return lo;
    }
};

// -----------------------------------------------------------------------
// AMACSearch：乱序调度引擎
//
// 每个 tick 三阶段：
//   1. 扫描 pending → 把 cache 已就绪（或超时）的任务移入 ready
//   2. 执行 ready 中所有任务（step）
//      完成 → 记录结果，补充新任务进 pending
//      未完成（发出 prefetch）→ 放回 pending
//   3. 新任务填满 pending 空位
//
// POOL_SIZE  : 同时在途的查询数（建议 32~128）
// FALLBACK   : tick 兜底距离，防止 RDTSC 误判导致活锁（建议 16~32）
// -----------------------------------------------------------------------
template<typename BTree>
class AMACSearch {
public:
    using Key   = typename BTree::key_type;
    using Value = typename BTree::data_type;
    using Task  = AMACTask<BTree>;

    template<size_t POOL_SIZE = 64, size_t FALLBACK = 16>
    static std::vector<bool> batch_lookup(
        BTree&                     btree,
        const std::vector<Key>&    queries,
        std::vector<Value>&        results
    ) {
        static_assert(POOL_SIZE >= 4, "POOL_SIZE must be >= 4");
        static_assert(POOL_SIZE > FALLBACK, "POOL_SIZE must be > FALLBACK");

        const size_t total = queries.size();
        std::vector<bool> found(total, false);
        results.resize(total);

        const typename BTree::node* root = btree.get_root();
        if (!root || queries.empty()) return found;

        // ── 双队列（栈上分配，避免堆开销）──────────────────────────
        Task     pending[POOL_SIZE];
        uint64_t pending_tick[POOL_SIZE];   // 各任务进入 pending 时的 tick
        size_t   pending_count = 0;

        Task   ready[POOL_SIZE];
        size_t ready_count = 0;

        size_t   next_query = 0;
        size_t   finished   = 0;
        uint64_t tick       = 0;

        // ── 初始填充 pending ─────────────────────────────────────────
        while (pending_count < POOL_SIZE && next_query < total) {
            Task t;
            t.query_idx = next_query;
            t.key       = queries[next_query++];
            t.curr_node = root;
            t.next_node = nullptr;
            t.done      = false;
            __builtin_prefetch(root, 0, 3);

            pending[pending_count]      = t;
            pending_tick[pending_count] = tick;
            ++pending_count;
        }

        // ── 主调度循环 ───────────────────────────────────────────────
        while (finished < total) {
            ++tick;

            // ── 阶段 1：扫描 pending，乱序提取就绪任务 ───────────────
            // "乱序"体现在此：谁先就绪谁先出队，与入队顺序无关
            size_t new_pending = 0;
            for (size_t i = 0; i < pending_count; ++i) {
                Task& t = pending[i];

                // 刚入队、还没发过 prefetch 的任务（next_node==nullptr）
                // 直接视为就绪，让它先 step 一次发出第一个 prefetch
                bool no_wait = (t.next_node == nullptr);

                bool cache_ok = !no_wait &&
                                detail::is_cache_ready(t.next_addr());
                bool tick_ok  = !no_wait &&
                                (tick - pending_tick[i] >= FALLBACK);

                if (no_wait || cache_ok || tick_ok) {
                    if (!no_wait) t.advance();   // 滑动到 next_node
                    ready[ready_count++] = t;
                } else {
                    // 还没就绪，留在 pending（紧缩写回）
                    pending[new_pending]      = t;
                    pending_tick[new_pending] = pending_tick[i];
                    ++new_pending;
                }
            }
            pending_count = new_pending;

            // ── 阶段 2：执行 ready 队列 ───────────────────────────────
            for (size_t i = 0; i < ready_count; ++i) {
                Task t = ready[i];
                bool completed = t.step(results.data(), found);

                if (completed) {
                    ++finished;
                    // 补充新任务
                    if (next_query < total) {
                        t.query_idx = next_query;
                        t.key       = queries[next_query++];
                        t.curr_node = root;
                        t.next_node = nullptr;
                        t.done      = false;
                        __builtin_prefetch(root, 0, 3);

                        pending[pending_count]      = t;
                        pending_tick[pending_count] = tick;
                        ++pending_count;
                    }
                } else {
                    // 已发 prefetch，进 pending 等就绪
                    pending[pending_count]      = t;
                    pending_tick[pending_count] = tick;
                    ++pending_count;
                }
            }
            ready_count = 0;

            // ── 阶段 3：继续填满 pending 空位 ────────────────────────
            while (pending_count < POOL_SIZE && next_query < total) {
                Task t;
                t.query_idx = next_query;
                t.key       = queries[next_query++];
                t.curr_node = root;
                t.next_node = nullptr;
                t.done      = false;
                __builtin_prefetch(root, 0, 3);

                pending[pending_count]      = t;
                pending_tick[pending_count] = tick;
                ++pending_count;
            }

            // ── 防活锁：pending 非空但 ready 全空时，
            //    强制提升等待最久的任务 ──────────────────────────────
            if (ready_count == 0 && pending_count > 0 && finished < total) {
                size_t oldest = 0;
                for (size_t i = 1; i < pending_count; ++i)
                    if (pending_tick[i] < pending_tick[oldest]) oldest = i;

                Task t = pending[oldest];
                if (t.next_node != nullptr) t.advance();
                ready[ready_count++] = t;

                // 从 pending 移除（用末尾元素填空）
                --pending_count;
                pending[oldest]      = pending[pending_count];
                pending_tick[oldest] = pending_tick[pending_count];
            }
        }

        return found;
    }

    static std::string name() { return "AMAC"; }
};

} // namespace algorithms
} // namespace btree
} // namespace structures