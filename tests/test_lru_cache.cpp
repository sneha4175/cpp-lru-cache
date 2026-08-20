// Core LRUCache behaviour: get/put, eviction order, promotion, boundaries.

#include <string>

#include "check.hpp"
#include "lru_cache.hpp"

static void test_basic_get_put() {
  LRUCache<int, std::string> cache(2);
  CHECK(cache.empty());
  CHECK(!cache.get(1).has_value());  // miss on empty cache

  cache.put(1, "one");
  cache.put(2, "two");
  CHECK(cache.size() == 2);
  CHECK(cache.get(1).value() == "one");
  CHECK(cache.get(2).value() == "two");
  CHECK(cache.contains(1));
  CHECK(!cache.contains(99));
}

static void test_eviction_order() {
  LRUCache<int, int> cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);  // capacity 2 -> key 1 (LRU) is evicted
  CHECK(cache.size() == 2);
  CHECK(!cache.contains(1));
  CHECK(cache.contains(2));
  CHECK(cache.contains(3));
}

static void test_get_promotes() {
  LRUCache<int, int> cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  CHECK(cache.get(1).value() == 10);  // 1 becomes MRU, 2 becomes LRU
  cache.put(3, 30);                   // evicts 2, not 1
  CHECK(cache.contains(1));
  CHECK(!cache.contains(2));
  CHECK(cache.contains(3));
}

static void test_update_existing_promotes() {
  LRUCache<int, int> cache(2);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(1, 100);  // update value AND promote 1 to MRU
  CHECK(cache.get(1).value() == 100);
  CHECK(cache.size() == 2);  // update must not grow the cache
  cache.put(3, 30);          // evicts 2 (now LRU)
  CHECK(cache.contains(1));
  CHECK(!cache.contains(2));
}

static void test_capacity_boundary() {
  // Capacity 1: every insert evicts the previous entry.
  LRUCache<int, int> one(1);
  one.put(1, 1);
  one.put(2, 2);
  CHECK(one.size() == 1);
  CHECK(!one.contains(1));
  CHECK(one.contains(2));

  // Capacity 0: nothing is ever retained.
  LRUCache<int, int> zero(0);
  zero.put(1, 1);
  CHECK(zero.size() == 0);
  CHECK(!zero.contains(1));
}

static void test_clear() {
  LRUCache<int, int> cache(3);
  cache.put(1, 1);
  cache.put(2, 2);
  cache.clear();
  CHECK(cache.empty());
  CHECK(!cache.contains(1));
  cache.put(5, 5);  // still usable after clear
  CHECK(cache.get(5).value() == 5);
}

static void test_get_or_compute() {
  LRUCache<int, int> cache(2);
  int computes = 0;

  // Miss: compute_fn runs exactly once and the result is cached.
  int v1 = cache.get_or_compute(1, [&] { ++computes; return 10; });
  CHECK(v1 == 10);
  CHECK(computes == 1);
  CHECK(cache.contains(1));

  // Hit: returns the cached value and does NOT call compute_fn. If it ran,
  // this lambda would return 999 and bump the counter.
  int v2 = cache.get_or_compute(1, [&] { ++computes; return 999; });
  CHECK(v2 == 10);       // cached value wins, not the would-be 999
  CHECK(computes == 1);  // counter unchanged -> compute_fn was not called

  // A distinct miss computes again.
  int v3 = cache.get_or_compute(2, [&] { ++computes; return 20; });
  CHECK(v3 == 20);
  CHECK(computes == 2);

  // The hit on key 1 promoted it, then inserting 2 made 2 the MRU, so recency
  // is now {2, 1} with 1 as LRU. A third get_or_compute must evict the LRU
  // entry (1) exactly like put would.
  int v4 = cache.get_or_compute(3, [&] { ++computes; return 30; });
  CHECK(v4 == 30);
  CHECK(computes == 3);
  CHECK(cache.size() == 2);
  CHECK(!cache.contains(1));  // evicted
  CHECK(cache.contains(2));
  CHECK(cache.contains(3));
}

int main() {
  test_basic_get_put();
  test_eviction_order();
  test_get_promotes();
  test_update_existing_promotes();
  test_capacity_boundary();
  test_clear();
  test_get_or_compute();
  return 0;
}
