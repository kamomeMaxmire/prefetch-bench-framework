# Prefetch Benchmark Framework

This repository benchmarks batched point lookup strategies on learned and tree-based indexes.  The current focus is comparing how different query scheduling and memory-latency hiding techniques behave on:

- B+Tree
- PGM-index
- LIPP

The benchmark uses SOSD-style `uint64_t` datasets, currently mainly:

```text
data/osm_cellids_200M_uint64
```

The default workload samples 1,000,000 existing keys from the dataset and shuffles them to reduce locality and expose cache-miss behavior.

## Project Layout

```text
include/
  data/
    SOSDDataLoader.hpp
  structures/
    btree/
      BTree.hpp
      algorithms/
        NoPrefetch.hpp
        GroupPrefetch.hpp
        SPP.hpp
        Vectorized.hpp
        AMAC.hpp
    PGM/
      PGMWrapper.hpp
      PGM_index/
      algorithms/
        NoPrefetch.hpp
        GroupPrefetch.hpp
        SPP.hpp
        Vectorized.hpp
        AMAC.hpp
    LIPP/
      LIPPWrapper.hpp
      LIPP_index/
        lipp.h
        lipp_base.h
      algorithms/
        NoPrefetch.hpp
        GroupPrefetch.hpp
        SPP.hpp
        Vectorized.hpp
        AMAC.hpp
src/
  main.cpp
analysis/
  visualize_btree.py
  visualize_PGM.py
results/
```

`structures/LIPP/LIPP_index` contains a local copy of the LIPP source, so the benchmark does not depend on an external `code/lipp` include path.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target prefetch_bench -j 4
```

The Release flags are configured in `CMakeLists.txt`:

```text
-O3 -march=native -DNDEBUG -flto
```

## Run

```bash
./build/prefetch_bench
```

`main.cpp` is currently used as the active experiment driver. During development it has been switched between B+Tree, PGM, and LIPP experiments. Check the active `main()` before running a long benchmark.

For full-size `osm_cellids_200M`, LIPP can require very large memory. In one run, LIPP built successfully but reported about 32 GB of index structure.

## Timing Method

Each strategy is run multiple times. The current preferred metric is a trimmed mean:

```text
15 runs per strategy
discard fastest 2 runs
discard slowest 2 runs
average the middle 11 runs
```

This is more defensible than taking the best run or top-3 average. It still suppresses system noise, but it is less biased toward lucky fast runs.

## Query Strategies

All index types expose a similar batch lookup interface:

```cpp
batch_lookup_no_prefetch(...)
batch_lookup_group_prefetch<N>(...)
batch_lookup_spp<N>(...)
batch_lookup_vectorized<N>(...)
batch_lookup_fsm_amac<N>(...)
```

The parameter `N` means:

```text
GroupPrefetch: group size G
SPP:           pipeline/window depth D
Vectorized:    batch/vector size V
AMAC:          FSM pool size P
```

The tested values are usually:

```text
1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
```

## B+Tree Summary

B+Tree lookup follows:

```text
root -> inner nodes -> leaf node
```

At each node, the lookup finds the lower-bound slot and follows the child pointer until reaching a leaf.

Implemented methods:

- `NoPrefetch`: one query at a time, no software prefetch.
- `GroupPrefetch`: processes a group of queries together and prefetches child nodes.
- `SPP`: software-pipelined traversal across tree levels.
- `Vectorized`: batch traversal plus SIMD lower-bound inside B+Tree nodes.
- `AMAC`: finite-state-machine pool that advances many independent lookups.

Current B+Tree trimmed-mean result on `osm_cellids_200M`:

```text
NoPrefetch:       about 739 ns/query
GroupPrefetch:    best around G=64,  about 322 ns/query, 2.30x
SPP:              best around D=64,  about 269 ns/query, 2.74x
Vectorized:       best around V=32,  about 235 ns/query, 3.14x
AMAC:             best around P=128, about 282 ns/query, 2.62x
```

The B+Tree result is intuitive: pointer chasing is expensive, but node-local search is also important. After changing `Vectorized.hpp` to use SIMD lower-bound for `uint64_t` node keys, Vectorized became the strongest method. The middle parameter range often forms a wide plateau because enough memory-level parallelism has already been exposed.

## PGM-index Summary

PGM lookup follows:

```text
model search -> predicted range [lo, hi) -> lower_bound in physical key array
```

PGM's final search window is controlled by epsilon. For example, `Epsilon=64` gives a final window of roughly:

```text
2 * epsilon + 2 ~= 130 keys
```

Current PGM result on `osm_cellids_200M`:

```text
Epsilon scaling:
  Eps=32 gave the best no-prefetch result in the recorded run, about 316 ns/query.

Prefetch strategies at Eps=64:
  StaticSPP:   best around D=2,   about 205 ns/query
  Vectorized:  best around V=512, about 224 ns/query
  GroupPref:   about 407 ns/query at best
  AMAC:        about 446 ns/query at best
```

Unlike B+Tree, PGM does not necessarily favor Vectorized. StaticSPP is strong because it reuses `index.search()` to obtain the final bounded range, then pipelines the binary search over the physical key array. That directly targets PGM's main lookup cost.

GroupPrefetch and AMAC are weaker on PGM because their implementations manually traverse more of the PGM path and carry higher scheduling overhead.

## LIPP Summary

LIPP lookup follows:

```text
node.model.predict(key) -> slot
slot is empty: not found
slot is data: compare key and return value
slot is child: descend to child
```

The LIPP implementation was integrated locally under:

```text
include/structures/LIPP/
```

The benchmark wrapper is:

```cpp
structures::lipp::LIPPWrapper<uint64_t, uint64_t, true>
```

The `true` template argument enables FMCD in the copied LIPP implementation.

LIPP bulk-load test on `osm_cellids_200M`:

```text
Total keys:  200,000,000
Build time:  about 27 seconds
Index size:  about 34,226,170,400 bytes, or 32.6 GB
Verify:      OK
```

The LIPP index size is much larger than B+Tree in this implementation. This is expected. The large size is not caused by the linear model parameters themselves; it comes from LIPP's sparse slot arrays. LIPP allocates many empty `Item` slots to reduce collisions, and each slot for `uint64_t -> uint64_t` can occupy 16 bytes even when unused.

LIPP query algorithms were adapted to LIPP's node structure rather than copied directly from B+Tree or PGM:

- `NoPrefetch`: sequential model-prediction traversal.
- `GroupPrefetch`: advances a group of active queries and prefetches child nodes.
- `SPP`: uses a fixed-size task window, because LIPP path depth is variable.
- `Vectorized`: batch/vector-style traversal without explicit prefetch; it separates prediction and slot handling across active queries.
- `AMAC`: FSM pool with `INIT -> SEARCH -> DONE` states.

The current `results/LIPP_benchmark.txt` appears incomplete in the checked state; it stops around the Vectorized section. Partial results show:

```text
NoPrefetch:          about 215 ns/query
GroupPrefetch best:  about 164 ns/query at G=256
SPP best:            about 175 ns/query around D=16
Vectorized partial:  about 159 ns/query at V=64 before the file truncates
```

Re-run the LIPP benchmark to generate a complete result table before using it in final plots.

## Interpretation

The same strategy can behave differently across index structures because the bottleneck changes:

```text
B+Tree:
  pointer chasing + node-local search
  SIMD Vectorized works very well

PGM:
  model prediction + bounded physical-array binary search
  StaticSPP works very well

LIPP:
  model prediction + sparse slot traversal + possible child descent
  batch traversal and moderate prefetch windows can help, but memory footprint is large
```

Most curves should show:

```text
small parameter: too little parallelism
middle parameter: best or plateau
large parameter: cache/TLB/state overhead dominates
```

This means a wide plateau is not a failure. It usually means the method has reached enough memory-level parallelism and is not very sensitive to the exact parameter.

## Notes

- Use `Latency(ns)` or `Speedup` for plots.
- Use log-scale or `log2(parameter)` on the x-axis.
- For SPP, mention that the effective concurrency can differ from the displayed parameter depending on the index structure.
- Do not describe LIPP's 32 GB as "model size"; call it "index structure size" or "memory footprint".
- For final experiments, keep the timing method consistent across B+Tree, PGM, and LIPP.
