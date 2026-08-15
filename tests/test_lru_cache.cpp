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

int main() {
  test_basic_get_put();
  test_eviction_order();
  test_get_promotes();
  test_update_existing_promotes();
  test_capacity_boundary();
  test_clear();
  return 0;
}
