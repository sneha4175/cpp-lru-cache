# cpp-lru-cache

![CI](https://github.com/sneha4175/cpp-lru-cache/actions/workflows/ci.yml/badge.svg)

A header-only, generic **Least-Recently-Used (LRU) cache** in modern C++17, with
an optional thread-safe wrapper and an optional time-to-live (TTL) variant.

## Problem

A cache holds a bounded number of entries. When it is full and a new entry
arrives, something must be discarded. The LRU policy discards the entry that has
gone the longest without being accessed, on the assumption that recently used
data is the most likely to be used again (temporal locality). The interesting
part is doing both `get` and `put` in **O(1)** while maintaining exact recency
order.

## Design

Two data structures cooperate:

| Structure | Role |
|-----------|------|
| `std::list<std::pair<Key, Value>>` | Entries in recency order. Front = most-recently-used (MRU), back = least-recently-used (LRU) and thus the eviction victim. |
| `std::unordered_map<Key, list::iterator>` | Maps a key to its list node for O(1) lookup. |

On access, the node is promoted to the front with **`std::list::splice`**.
`splice` relinks the node in place: no element is reallocated, moved, or copied,
so **the iterators stored in the map stay valid**. That validity guarantee is
exactly what makes a map-of-iterators design correct — the same trick would be
unsound with a `std::vector`, whose iterators are invalidated by reallocation.

- `put` on an existing key updates the value and promotes the node.
- `put` on a new key inserts at the front, then evicts the back node if size now
  exceeds capacity.
- `get` returns `std::optional<Value>` (empty on miss) and promotes on hit.

### Complexity

| Operation | Time | Why |
|-----------|------|-----|
| `get` | O(1) | one hash lookup + one `splice` |
| `put` | O(1) | one hash lookup + front insert (+ possible O(1) eviction) |
| `contains` | O(1) | one hash lookup |
| `size` / `clear` | O(1) / O(n) | count is tracked; clear frees all nodes |

Space is O(capacity): every entry appears once in the list and once in the map.

## Thread safety: why a plain `std::mutex`, not `std::shared_mutex`

`ThreadSafeLRUCache` wraps the core cache behind a single `std::mutex`.

A readers-writer lock (`std::shared_mutex`) only helps when many operations are
pure reads that can safely run in parallel under a shared lock. **In an LRU cache
`get` is not a pure read** — a hit promotes the entry to the MRU position,
mutating both the list and the map. Under a `shared_mutex` every `get` would
still have to take the *exclusive* lock, so there is no concurrency to gain,
while each acquisition costs more than a plain mutex. A single `std::mutex` is
therefore the simpler, faster, and more honest choice.

(If the workload were dominated by true read-only queries — e.g. a `peek` that
does not reorder — a `shared_mutex` could pay off. The classic LRU `get` is not
that.)

## TTL variant

`TTLCache<Key, Value>` (in `include/ttl_lru_cache.hpp`) adds a fixed
time-to-live per entry using `std::chrono::steady_clock` (monotonic, immune to
wall-clock changes). Behavior:

- Each entry stores an expiry time set at `put`.
- Expiry is **lazy**: an expired entry is treated as a miss on `get` and erased
  at that point — no background sweeper thread, so every operation stays O(1).
- Capacity-based LRU eviction still applies independently of expiry.
- `contains` reports liveness (present and not expired) but, being `const`, does
  not sweep.

It is a separate type on purpose, so the base `LRUCache` carries no time
dependency.

## Building and testing

Requires CMake >= 3.16 and a C++17 compiler.

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests are dependency-free (no network, no test framework) — plain executables
registered with CTest that return non-zero on failure. Coverage: basic get/put,
eviction order, get/update promotion, capacity boundaries (including 0 and 1),
TTL expiry and refresh, and a multithreaded stress test.

### Optional: ThreadSanitizer

The concurrency test can be built under ThreadSanitizer to check for data races:

```sh
cmake -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure
```

## Layout

```
include/
  lru_cache.hpp             # core LRUCache<Key, Value>
  thread_safe_lru_cache.hpp # mutex-guarded wrapper
  ttl_lru_cache.hpp         # TTL variant
tests/
  check.hpp                 # always-on assertion macro
  test_lru_cache.cpp
  test_ttl_cache.cpp
  test_thread_safe.cpp
CMakeLists.txt
```
