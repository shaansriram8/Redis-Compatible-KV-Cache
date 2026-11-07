#include "store/flat_map.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kKeys = 1'000'000;
const std::string kVal(32, 'v');

using StdMap = std::unordered_map<std::string, std::string>;

// Keys are shuffled so lookups jump around the table the way real traffic
// does, instead of walking it in insertion order.
std::vector<std::string> make_keys(const char* prefix) {
  std::vector<std::string> keys;
  keys.reserve(kKeys);
  for (int i = 0; i < kKeys; i++)
    keys.push_back(prefix + std::to_string(i));
  std::shuffle(keys.begin(), keys.end(), std::mt19937(1));
  return keys;
}

const std::vector<std::string>& hit_keys() {
  static const auto keys = make_keys("memtier-");
  return keys;
}

const std::vector<std::string>& miss_keys() {
  static const auto keys = make_keys("absent-");
  return keys;
}

const kv::FlatMap& full_flat() {
  static const auto m = [] {
    kv::FlatMap m;
    for (const auto& k : hit_keys())
      m.set(k, kVal);
    return m;
  }();
  return m;
}

const StdMap& full_std() {
  static const auto m = [] {
    StdMap m;
    for (const auto& k : hit_keys())
      m.emplace(k, kVal);
    return m;
  }();
  return m;
}

template <class F>
void lookups(benchmark::State& state, const std::vector<std::string>& keys, F f) {
  size_t i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(f(keys[i]));
    if (++i == keys.size())
      i = 0;
  }
  state.SetItemsProcessed(state.iterations());
}

void BM_FlatGetHit(benchmark::State& state) {
  const auto& m = full_flat();
  lookups(state, hit_keys(), [&](const std::string& k) { return m.get(k); });
}

void BM_StdGetHit(benchmark::State& state) {
  const auto& m = full_std();
  lookups(state, hit_keys(), [&](const std::string& k) { return m.find(k); });
}

void BM_FlatGetMiss(benchmark::State& state) {
  const auto& m = full_flat();
  lookups(state, miss_keys(), [&](const std::string& k) { return m.get(k); });
}

void BM_StdGetMiss(benchmark::State& state) {
  const auto& m = full_std();
  lookups(state, miss_keys(), [&](const std::string& k) { return m.find(k); });
}

// Builds a 1M-key map from empty each iteration, so resizes are included.
void BM_FlatInsert(benchmark::State& state) {
  const auto& keys = hit_keys();
  for (auto _ : state) {
    kv::FlatMap m;
    for (const auto& k : keys)
      m.set(k, kVal);
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() * kKeys);
}

void BM_StdInsert(benchmark::State& state) {
  const auto& keys = hit_keys();
  for (auto _ : state) {
    StdMap m;
    for (const auto& k : keys)
      m.insert_or_assign(k, kVal);
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(state.iterations() * kKeys);
}

} // namespace

BENCHMARK(BM_FlatGetHit);
BENCHMARK(BM_StdGetHit);
BENCHMARK(BM_FlatGetMiss);
BENCHMARK(BM_StdGetMiss);
BENCHMARK(BM_FlatInsert)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StdInsert)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
