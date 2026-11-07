# PERF

## 0.4 FlatMap vs std::unordered_map (baseline)

Apple M1 (8 cores, 8 GB), macOS, Apple Clang 17, release preset (`-O2 -march=native`).
1M keys (`memtier-N`, shuffled), 32-byte values. Median of 5 runs:
`./build/release/flat_map_bench --benchmark_repetitions=5`

| op             | FlatMap  | unordered_map |
|----------------|----------|---------------|
| get hit        | 41.2 ns  | 66.5 ns       |
| get miss       | 60.9 ns  | 66.9 ns       |
| insert 1M keys | 183 ms   | 343 ms        |

FlatMap is untuned: std::hash, max load 0.75, std::string key and value stored in
each slot (56-byte slots), no stored hash. Misses are slower than hits because a miss
walks the whole probe run to an empty slot, comparing keys on every full slot it passes.
Storing the hash in the slot (2.1) should help most there.

Laptop numbers, only useful relative to each other. Real benchmarks run on Linux.
