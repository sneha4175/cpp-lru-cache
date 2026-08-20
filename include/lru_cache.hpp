#ifndef LRU_CACHE_HPP
#define LRU_CACHE_HPP

#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

// Fixed-capacity Least-Recently-Used cache with O(1) get and put.
//
// Data structures:
//   - std::list<std::pair<Key, Value>> holds entries in recency order.
//     The front is the most-recently-used (MRU) entry, the back is the
//     least-recently-used (LRU) entry and therefore the eviction victim.
//   - std::unordered_map<Key, list-iterator> maps a key to the list node
//     that stores it, giving O(1) lookup.
//
// On access we promote a node to the front with std::list::splice, which
// relinks the node in place: no element is reallocated, moved, or copied,
// so the iterators stored in the map stay valid. That property is what
// makes the map-of-iterators design correct.
//
// This class is NOT thread-safe. See ThreadSafeLRUCache for a synchronized
// wrapper.
template <typename Key, typename Value>
class LRUCache {
 public:
  explicit LRUCache(std::size_t capacity) : capacity_(capacity) {}

  // Returns the value for key and marks it most-recently-used, or nullopt
  // if the key is absent. Mutates recency order, hence non-const.
  std::optional<Value> get(const Key& key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
      return std::nullopt;
    }
    // Relink the accessed node to the front. splice keeps the node (and the
    // iterator held in index_) valid.
    entries_.splice(entries_.begin(), entries_, it->second);
    return it->second->second;
  }

  // Inserts or updates key. An existing key is updated and promoted; a new
  // key is inserted at the front and the LRU entry is evicted if the cache
  // is over capacity.
  void put(const Key& key, Value value) {
    auto it = index_.find(key);
    if (it != index_.end()) {
      it->second->second = std::move(value);
      entries_.splice(entries_.begin(), entries_, it->second);
      return;
    }
    entries_.emplace_front(key, std::move(value));
    index_.emplace(key, entries_.begin());
    if (index_.size() > capacity_) {
      evict_lru();
    }
  }

  // Cache-through (read-through) lookup. On a hit the entry is promoted and
  // its value returned. On a miss compute_fn() is invoked exactly once, its
  // result is inserted (evicting the LRU entry if now over capacity), and a
  // copy is returned. compute_fn is any callable returning Value. Returns by
  // value to mirror get()'s value semantics. O(1) amortized: a hit is one
  // get, a miss is one lookup + one put plus the cost of compute_fn itself.
  template <typename Fn>
  Value get_or_compute(const Key& key, Fn&& compute_fn) {
    auto it = index_.find(key);
    if (it != index_.end()) {
      entries_.splice(entries_.begin(), entries_, it->second);
      return it->second->second;
    }
    Value value = compute_fn();
    put(key, value);  // may evict immediately at capacity 0; value still valid
    return value;
  }

  // Const membership test; does not affect recency order.
  bool contains(const Key& key) const {
    return index_.find(key) != index_.end();
  }

  std::size_t size() const { return index_.size(); }
  std::size_t capacity() const { return capacity_; }
  bool empty() const { return index_.empty(); }

  void clear() {
    entries_.clear();
    index_.clear();
  }

 private:
  using List = std::list<std::pair<Key, Value>>;

  void evict_lru() {
    const Key& victim = entries_.back().first;
    index_.erase(victim);
    entries_.pop_back();
  }

  std::size_t capacity_;
  List entries_;  // front = MRU, back = LRU
  std::unordered_map<Key, typename List::iterator> index_;
};

#endif  // LRU_CACHE_HPP
