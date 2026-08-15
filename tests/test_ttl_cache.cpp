// TTLCache behaviour: live hits, lazy expiry as a miss, refresh on put.

#include <chrono>
#include <thread>

#include "check.hpp"
#include "ttl_lru_cache.hpp"

using namespace std::chrono_literals;

static void test_live_entry_hits() {
  TTLCache<int, int> cache(4, 1s);
  cache.put(1, 100);
  CHECK(cache.contains(1));
  CHECK(cache.get(1).value() == 100);
}

static void test_expired_entry_is_miss() {
  TTLCache<int, int> cache(4, 20ms);
  cache.put(1, 100);
  CHECK(cache.contains(1));
  std::this_thread::sleep_for(40ms);
  CHECK(!cache.contains(1));         // expired: no longer live
  CHECK(!cache.get(1).has_value());  // expired: treated as a miss
}

static void test_expired_entry_is_swept_on_get() {
  TTLCache<int, int> cache(4, 20ms);
  cache.put(1, 100);
  std::this_thread::sleep_for(40ms);
  CHECK(cache.size() == 1);          // not yet swept
  CHECK(!cache.get(1).has_value());  // get sweeps the expired entry
  CHECK(cache.size() == 0);
}

static void test_put_refreshes_ttl() {
  TTLCache<int, int> cache(4, 60ms);
  cache.put(1, 1);
  std::this_thread::sleep_for(40ms);
  cache.put(1, 2);  // refresh: new value and new expiry window
  std::this_thread::sleep_for(40ms);
  // 80ms since first put but only 40ms since refresh -> still live.
  CHECK(cache.get(1).value() == 2);
}

static void test_ttl_still_does_lru_eviction() {
  TTLCache<int, int> cache(2, 10s);  // long TTL so nothing expires here
  cache.put(1, 1);
  cache.put(2, 2);
  cache.put(3, 3);  // over capacity -> LRU (key 1) evicted
  CHECK(cache.size() == 2);
  CHECK(!cache.contains(1));
  CHECK(cache.contains(2));
  CHECK(cache.contains(3));
}

int main() {
  test_live_entry_hits();
  test_expired_entry_is_miss();
  test_expired_entry_is_swept_on_get();
  test_put_refreshes_ttl();
  test_ttl_still_does_lru_eviction();
  return 0;
}
