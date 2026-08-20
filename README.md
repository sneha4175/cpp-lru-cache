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
- `get_or_compute(key, compute_fn)` is the cache-through pattern: return the
  cached value on a hit; on a miss call `compute_fn()`, insert the result, and
  return it. `compute_fn` runs **exactly once per miss** and never on a hit.

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

### `get_or_compute` under the lock

`ThreadSafeLRUCache::get_or_compute` **holds the mutex for the whole operation,
including the call to `compute_fn`.** That is the simplest correct choice: a
missing key is computed and inserted atomically, so `compute_fn` runs exactly
once per miss with no window for two threads to race and both compute. The
honest costs are that concurrent computes are **serialized** — a slow
`compute_fn` blocks every other cache operation while it runs — and that
`compute_fn` must not call back into the same cache, or it will **deadlock** on
the non-recursive mutex. The alternative is a double-checked pattern (lock →
check → unlock → compute → lock → insert) that keeps the lock free during the
compute, but then a value can be **computed more than once** when several
threads miss the same key concurrently, and each caller may observe a different
computed instance. We chose the single-lock version because for the typical
cheap, side-effect-free `compute_fn` its simplicity and single-compute
guarantee outweigh the lost compute concurrency.

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

## Benchmark

A single-threaded throughput micro-benchmark lives in `benchmarks/bench.cpp`. It
is off by default so the normal build and CI stay fast; enable it with
`-DBUILD_BENCH=ON`:

```sh
cmake -B build -DBUILD_BENCH=ON
cmake --build build
./build/bench
```

It drives a capacity-10,000 `LRUCache<int,int>` with 1,000,000 operations whose
keys come from a fixed-seed PRNG over a key space of 20,000. Because the key
space is twice the capacity, the `get` phase sees a realistic mix of hits and
misses (~50%) rather than an all-hit best case. Timing uses
`std::chrono::steady_clock`; it reports ops/sec and average ns/op for `put` and
`get` separately.

**Numbers are machine-dependent** and only meaningful relative to each other on
the same box — do not compare across machines or compilers. Representative local
run (Apple clang `-O2`, single core; your results will differ):

```
put: 1000000 ops | ~14.2M ops/sec | ~70 ns/op
get: 1000000 ops | ~77.6M ops/sec | ~13 ns/op   (hit rate ~51%)
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
benchmarks/
  bench.cpp                 # put/get throughput micro-benchmark (opt-in)
CMakeLists.txt
```
