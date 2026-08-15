// Concurrency test for ThreadSafeLRUCache. Many threads hammer put/get on a
// shared cache. The goals are: no crash, no data corruption (checked by TSan
// when built with -fsanitize=thread), and the invariant size <= capacity is
// never violated. It also checks that concurrent writers to distinct keys all
// land, and that reads observe only values that were actually written.

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "check.hpp"
#include "thread_safe_lru_cache.hpp"

static void stress_shared_key_space() {
  constexpr std::size_t kCapacity = 128;
  constexpr int kThreads = 8;
  constexpr int kOpsPerThread = 50000;
  constexpr int kKeyRange = 512;  // > capacity, so eviction happens constantly

  ThreadSafeLRUCache<int, int> cache(kCapacity);
  std::atomic<bool> bad_size{false};
  std::atomic<bool> bad_value{false};

  auto worker = [&](int seed) {
    unsigned state = static_cast<unsigned>(seed) * 2654435761u + 1u;
    auto next = [&state]() {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      return state;
    };
    for (int i = 0; i < kOpsPerThread; ++i) {
      const int key = static_cast<int>(next() % kKeyRange);
      if (next() & 1u) {
        // Encode the key in the value so readers can verify consistency.
        cache.put(key, key * 7);
      } else {
        auto v = cache.get(key);
        if (v.has_value() && v.value() != key * 7) {
          bad_value.store(true);
        }
      }
      if (cache.size() > kCapacity) {
        bad_size.store(true);
      }
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(worker, t + 1);
  }
  for (auto& th : threads) {
    th.join();
  }

  CHECK(!bad_size.load());   // capacity invariant held under contention
  CHECK(!bad_value.load());  // reads never saw a torn / wrong value
  CHECK(cache.size() <= kCapacity);
}

static void stress_disjoint_keys_all_present() {
  // Each thread owns a disjoint key block; total keys fit in capacity, so
  // every write must survive with its exact value.
  constexpr int kThreads = 8;
  constexpr int kPerThread = 100;
  constexpr std::size_t kCapacity = kThreads * kPerThread;

  ThreadSafeLRUCache<int, int> cache(kCapacity);
  auto writer = [&](int block) {
    const int base = block * kPerThread;
    for (int i = 0; i < kPerThread; ++i) {
      cache.put(base + i, (base + i) * 3);
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back(writer, t);
  }
  for (auto& th : threads) {
    th.join();
  }

  CHECK(cache.size() == kCapacity);
  for (int k = 0; k < kThreads * kPerThread; ++k) {
    CHECK(cache.get(k).value() == k * 3);
  }
}

int main() {
  stress_shared_key_space();
  stress_disjoint_keys_all_present();
  return 0;
}
