#ifndef THREAD_SAFE_LRU_CACHE_HPP
#define THREAD_SAFE_LRU_CACHE_HPP

#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>

#include "lru_cache.hpp"

// Thread-safe wrapper around LRUCache guarded by a single std::mutex.
//
// Why a plain mutex and not a readers-writer lock (std::shared_mutex)?
// A shared_mutex pays off only when there are many operations that do not
// mutate shared state and can therefore run concurrently under a shared
// (read) lock. In an LRU cache, get() is NOT such an operation: it promotes
// the accessed entry to the front of the recency list, mutating both the
// list and the map. Every get() would still need the exclusive lock, so a
// shared_mutex buys no real concurrency here while costing more per
// acquisition. A single mutex is the honest, simpler, faster choice.
template <typename Key, typename Value>
class ThreadSafeLRUCache {
 public:
  explicit ThreadSafeLRUCache(std::size_t capacity) : cache_(capacity) {}

  std::optional<Value> get(const Key& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.get(key);
  }

  void put(const Key& key, Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.put(key, std::move(value));
  }

  bool contains(const Key& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.contains(key);
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
  }

  std::size_t capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.capacity();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
  }

 private:
  mutable std::mutex mutex_;
  LRUCache<Key, Value> cache_;
};

#endif  // THREAD_SAFE_LRU_CACHE_HPP
