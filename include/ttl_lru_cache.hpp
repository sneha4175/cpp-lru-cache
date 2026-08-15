#ifndef TTL_LRU_CACHE_HPP
#define TTL_LRU_CACHE_HPP

#include <chrono>
#include <cstddef>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

// LRU cache whose entries additionally expire after a fixed time-to-live.
//
// This is kept as a separate type rather than bolted onto LRUCache so the
// base cache stays free of any time/chrono concern. It reuses the same
// list + map design; each list node also stores an expiry time_point taken
// from std::chrono::steady_clock (a monotonic clock, unaffected by wall-clock
// adjustments).
//
// Expiry is handled lazily: an expired entry is not proactively swept, it is
// simply treated as a miss and erased the next time it is looked up. This
// keeps every operation O(1) and avoids a background thread. Capacity-based
// LRU eviction still applies independently of TTL.
//
// Not thread-safe; wrap externally if shared across threads.
template <typename Key, typename Value>
class TTLCache {
 public:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  TTLCache(std::size_t capacity, Duration ttl)
      : capacity_(capacity), ttl_(ttl) {}

  std::optional<Value> get(const Key& key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
      return std::nullopt;
    }
    if (is_expired(it->second, Clock::now())) {
      erase(it);
      return std::nullopt;
    }
    entries_.splice(entries_.begin(), entries_, it->second);
    return it->second->value;
  }

  void put(const Key& key, Value value) {
    const auto expiry = Clock::now() + ttl_;
    auto it = index_.find(key);
    if (it != index_.end()) {
      it->second->value = std::move(value);
      it->second->expiry = expiry;
      entries_.splice(entries_.begin(), entries_, it->second);
      return;
    }
    entries_.push_front(Entry{key, std::move(value), expiry});
    index_.emplace(key, entries_.begin());
    if (index_.size() > capacity_) {
      evict_lru();
    }
  }

  // Present AND not expired. Const, so it does not sweep the expired entry.
  bool contains(const Key& key) const {
    auto it = index_.find(key);
    return it != index_.end() && !is_expired(it->second, Clock::now());
  }

  // Includes not-yet-swept expired entries; use contains() for liveness.
  std::size_t size() const { return index_.size(); }
  std::size_t capacity() const { return capacity_; }
  bool empty() const { return index_.empty(); }

  void clear() {
    entries_.clear();
    index_.clear();
  }

 private:
  struct Entry {
    Key key;
    Value value;
    Clock::time_point expiry;
  };
  using List = std::list<Entry>;
  using Iter = typename List::iterator;

  static bool is_expired(Iter it, Clock::time_point now) {
    return now >= it->expiry;
  }

  void erase(typename std::unordered_map<Key, Iter>::iterator map_it) {
    entries_.erase(map_it->second);
    index_.erase(map_it);
  }

  void evict_lru() {
    const Key& victim = entries_.back().key;
    index_.erase(victim);
    entries_.pop_back();
  }

  std::size_t capacity_;
  Duration ttl_;
  List entries_;  // front = MRU, back = LRU
  std::unordered_map<Key, Iter> index_;
};

#endif  // TTL_LRU_CACHE_HPP
