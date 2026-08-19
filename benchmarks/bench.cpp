// Micro-benchmark for LRUCache put/get throughput.
//
// Honest, single-threaded timing with std::chrono::steady_clock. Numbers are
// machine-dependent and only meaningful relative to each other on the same box.
//
// Workload: a cache of capacity 10,000 driven by N = 1,000,000 operations whose
// keys are drawn from a fixed-seed PRNG over a key space of 20,000. Because the
// key space is twice the capacity, the get phase sees a mix of hits and misses
// rather than an all-hit best case.
//
// Build:
//   cmake -B build -DBUILD_BENCH=ON
//   cmake --build build
//   ./build/bench

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "lru_cache.hpp"

namespace {

constexpr std::size_t kCapacity = 10'000;
constexpr std::size_t kOps = 1'000'000;
constexpr int kKeySpace = 20'000;  // 2x capacity -> mixed hit/miss on get
constexpr std::uint32_t kSeed = 0x5EED;

// Precompute the key stream from a deterministic PRNG so the timed loops
// measure the cache, not the random-number generator.
std::vector<int> make_keys(std::size_t n) {
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> dist(0, kKeySpace - 1);
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = dist(rng);
  }
  return keys;
}

void report(const char* label, std::size_t ops, std::chrono::nanoseconds elapsed) {
  const double seconds = std::chrono::duration<double>(elapsed).count();
  const double ops_per_sec = static_cast<double>(ops) / seconds;
  const double ns_per_op = static_cast<double>(elapsed.count()) / static_cast<double>(ops);
  std::cout << label << ": " << ops << " ops in " << seconds << " s | "
            << static_cast<std::uint64_t>(ops_per_sec) << " ops/sec | " << ns_per_op
            << " ns/op\n";
}

}  // namespace

int main() {
  using clock = std::chrono::steady_clock;

  const std::vector<int> put_keys = make_keys(kOps);
  const std::vector<int> get_keys = make_keys(kOps);

  LRUCache<int, int> cache(kCapacity);

  // put throughput
  const auto put_start = clock::now();
  for (std::size_t i = 0; i < kOps; ++i) {
    cache.put(put_keys[i], static_cast<int>(i));
  }
  const auto put_end = clock::now();

  // get throughput (mixed hit/miss). Accumulate a checksum so the optimizer
  // cannot discard the loop.
  std::uint64_t hits = 0;
  std::uint64_t checksum = 0;
  const auto get_start = clock::now();
  for (std::size_t i = 0; i < kOps; ++i) {
    if (auto v = cache.get(get_keys[i])) {
      ++hits;
      checksum += static_cast<std::uint64_t>(*v);
    }
  }
  const auto get_end = clock::now();

  std::cout << "LRUCache<int,int>  capacity=" << kCapacity << "  ops=" << kOps
            << "  key_space=" << kKeySpace << "  seed=0x" << std::hex << kSeed << std::dec
            << "\n";
  std::cout << "(numbers are machine-dependent)\n";
  report("put", kOps, put_end - put_start);
  report("get", kOps, get_end - get_start);
  std::cout << "get hit rate: " << (100.0 * static_cast<double>(hits) / kOps) << "%  (checksum "
            << checksum << ")\n";
  return 0;
}
