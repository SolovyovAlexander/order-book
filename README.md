You can find binary executable compatible with linux here `order-book/dist/order_book`

# Order Book / Matching Engine

A limit order book with **price-time priority** matching. It reads a stream of
order requests from stdin, matches crossing orders, and writes trade and
order-state messages to stdout. Errors go to stderr; no input crashes the program.

## Build & run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # run the test suite

./build/order_book < tests/golden/example.in  # run the engine
```

Requires a C++17 compiler and CMake >= 3.16. No third-party runtime dependencies.

A prebuilt **Linux x86-64 ELF** is in `dist/order_book` - fully static (no glibc /
libstdc++ version dependency, runs on any x86-64 Linux). It is built in Docker
(development is on macOS/Apple Silicon):

```bash
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src gcc:13 \
  g++ -std=c++17 -O2 -static -s -Iinclude \
  src/main.cpp src/price.cpp src/order_book.cpp src/parser.cpp src/stdout_reporter.cpp \
  -o dist/order_book
```

## Protocol

Input (stdin):

| Line | Meaning |
|---|---|
| `0,orderid,side,quantity,price` | AddOrderRequest (`side`: 0=buy, 1=sell) |
| `1,orderid` | CancelOrderRequest |

Output (stdout):

| Line | Meaning |
|---|---|
| `2,quantity,price` | TradeEvent |
| `3,orderid` | OrderFullyFilled |
| `4,orderid,quantity` | OrderPartiallyFilled (new remaining quantity) |

Per match, output is emitted in this order: **TradeEvent**, then the fill message
for the **aggressive** order, then for the **resting** order. A trade executes at
the **resting** order's price.

## Architecture

Layered bottom-up, each layer depending only on the ones below it:

1. **Value types** - `Price`, `Side`, `Order`, the request/output message structs.
2. **Contracts** - `Level` (one price level's storage) and `Reporter` (output sink).
3. **`OrderBook`** - the matching engine, built on `Level` and `Reporter`.

Parse, match, and report are separated: the parser builds requests, the engine
matches, the Reporter emits output. In tests the Reporter is a double that
collects the message stream for an exact comparison.

### Data structures

- The two sides are `std::map<Price, Level>`: asks ascending, bids descending
  (via `std::greater`). On both, the best price is `begin()` - O(1) to read, and
  matching sweeps the book in price order. A tree (not a hash) because matching
  needs the *best* price and ordered traversal, which a hash cannot give.
- A `Level` is a FIFO queue, oldest at the front (time priority). Orders live in a
  shared **`Pool`** (arena): one growing `std::vector<Node>` with an intrusive
  free list; each level threads its queue through the pool as a doubly-linked list
  of indices. No per-order `malloc`. The resting node is lean - `{id, quantity}`;
  price is the level's key and side is which book, so neither is duplicated.
- `std::unordered_map<OrderId, Location>` maps an id to its resting order's
  address: a direct back-pointer to the `Level` (plus the in-level handle, and
  side/price for the rare empty-level erase). Cancel reaches the order in O(1)
  without walking the price tree.

### Complexity

`P` = number of distinct price levels, `M` = resting orders consumed by an add.

| Operation | Complexity | Notes |
|---|---|---|
| Determine if an add matches | **O(1)** | compare against the best level (`begin()`) |
| Add, with matching | **O(M + log P)** | output-sensitive: M is unavoidable work |
| Remove a filled resting order | **O(1)** (+ O(log P) if its level empties) | pop front + index erase |
| Cancel, level survives | **O(1)** | back-pointer + arena unlink, no tree walk |
| Cancel, empties the level | + **O(log P)** | erase the now-empty level from the map |

## Key decisions & assumptions

- **C++17.** Uses `variant`/`optional`/`string_view`/`from_chars`; a broadly
  portable toolchain for the reviewer.
- **Price is fixed-point `int64`, never float.** 9 decimal places (`SCALE_DIGITS`).
  Exact price comparison is central (level grouping, cross checks, map keys) and
  float would break it; parsing goes string -> int64 directly.
- **Negative and zero prices are accepted.** The spec calls price a "decimal
  number" (unlike id/quantity, which are "positive"). Real markets go negative
  (WTI crude settled at −$37.63 on 2020-04-20; European power prices on
  oversupply). The engine is sign-agnostic (matching is just `int64` comparison),
  so this needed only sign handling in `Price` plus a two-sided overflow guard
  (a negative reaches one further, since `-INT64_MIN == INT64_MAX + 1`).
- **Order id and quantity must be positive** (`> 0`), per the spec. `uint64_t`
  rules out negatives by type; the `> 0` check rules out zero.
- **A trade executes at the resting order's price**, so the aggressor gets price
  improvement whenever the resting quote beats its limit.
- **`cancel` of an unknown id and a duplicate `add` are silent no-ops** (return
  `false`). The engine does no I/O; `main` decides whether to log them.
- **`Level` is a concrete class, not a virtual interface.** Swappability comes
  from a stable API plus an opaque `Level::Handle`, not a vtable - a vtable per
  level would force `map<Price, unique_ptr<Level>>` (a heap alloc and indirect
  call per level). `Reporter` *is* virtual: the one polymorphic seam, called per
  output event, where dispatch cost is dwarfed by formatting and I/O.

## Optimization journey

The engine was correct first, then optimized in measured steps. Each step kept the
matching logic and the golden test untouched - the abstractions (opaque `Handle`,
`Location`) were designed so storage could change underneath. Numbers are medians
over 5 runs of 1M ops, `-O2`, Apple M-series; reproduce with `./build/bench`.

**1. Baseline:** `std::map<Price, Level>` with `Level` as a `std::list<Order>`.
Correct and clear, but a `malloc`/`free` per order on the list nodes.

**2. Arena (`Pool`).** Replaced the per-level `std::list` with a shared
`std::vector<Node>` + intrusive free list; handles became pool indices.

| Phase | before | after | |
|---|---:|---:|---|
| add, matching | ~35 ns | ~24 ns | **-31%** |
| mixed stream | ~100 ns | ~85 ns | **-15%** |

The `wide` phases (add-no-cross, cancel) barely moved: there the `std::map` tree
depth and its per-level node allocation dominate, not the order nodes.

**3. Lean node + cancel back-pointer.** Shrank the resting `Order` to
`{id, quantity}` (price = level key, side = which book) and stored a direct
`Level*` in the index so cancel skips the price-tree walk. The benchmark gained a
`cancel-deep` phase (deep queues, cancels that don't empty the level) to isolate it:

| Phase | before | after | |
|---|---:|---:|---|
| cancel-deep | ~36 ns | ~18 ns | **-50%** |

Sparse `cancel` was unchanged - there every cancel empties its level, so it still
pays the `std::map` erase. The back-pointer helps when the level survives, which is
the normal case in a real book with many orders per level.

**4. Reserve the pool and index, and measure the tail.** Throughput medians hid a
spike: per-op timing showed a single `add` taking **~2.1 ms** (median ~334 ns) when
the index's `unordered_map` rehashed and copied the whole table. `reserve()`ing the
pool and the index up front dropped the max to **~50-70 µs** (~30-45x lower) with no
change to p50/p99. The remaining tail is the `std::map` allocating a node per new
level plus OS scheduler jitter (~15 µs even on an empty loop). Lesson: deterministic
latency comes from killing per-op allocations and rehashes, not from shaving the
median - which is why I report p99/p99.9/max, not just the median.

### Current numbers

| Phase | ns/op | ops/sec |
|---|---:|---:|
| add, no cross (rests) | ~405 | ~2.5M |
| add, matching | ~24 | ~42M |
| cancel (sparse, every cancel empties) | ~480 | ~2.1M |
| cancel-deep (level survives) | ~18 | ~55M |
| mixed stream | ~85 | ~12M |

The `rests` and sparse `cancel` phases spread prices over a 5M range, so the `map`
holds ~10^6 levels and cost is dominated by tree depth and cache misses - which the
ladder (below), not the arena, would address. `matching` keeps the book at one
level, and `mixed` keeps it small around a drifting mid (the realistic regime).

## Input / output

End-to-end (`./order_book < datasets/mixed.in`, 1M messages) runs at ~1M msg/sec -
about 10x slower than the in-process `mixed` figure. That gap is text parsing,
formatting, and stdout I/O: once the engine itself is cheap, **the text protocol is
the throughput ceiling**, not the matching.

Within the text protocol, the cheapest real win is `std::to_chars` into a reusable buffer on the output path - but the win is smaller
than it looks, because `std::string`'s small-string optimization already avoids the
heap for typical-length prices, and only `TradeEvent` carries a price. The genuine
production answer is a binary protocol over a kernel-bypass transport, which changes
the problem rather than tuning it. So for this deliverable the buffered text I/O
(`sync_with_stdio(false)`, `'\n'` not `std::endl`) is appropriate, and further I/O
work is noted but not pursued.

## Production improvements

- **Flat (open-addressing) hash for the id index.** `std::unordered_map` is
  node-based (a heap node per entry, pointer-chased per lookup); an open-addressing
  map (à la `absl::flat_hash_map`) stores entries contiguously - far fewer cache
  misses on cancel/add, no per-entry allocation. Left out only because it needs a
  third-party dependency or a hand-rolled table; the standard library has none.
- **Price ladder** (array indexed by tick) for O(1) level access - *applicable only*
  with a bounded price range and a known tick. The spec's price is an arbitrary
  decimal with no tick, so the code keeps `std::map`; the ladder is the tool to
  reach for when those assumptions hold.
- **Hybrid:** a ladder over the dense middle of the book + map/hash for the sparse
  tails.
- **Chunked / deque-style arena** so growth never copies (a fixed chunk is added,
  raw pointers stay stable) - removes the reallocation spike for unbounded books.
  Plus a generation counter on handles to catch use-after-free, and a pooled
  allocator for the `std::map` nodes (the remaining tail source).
- **Templated `Reporter`** (`OrderBook<ReporterT>`) to make output dispatch static
  and inlinable - removes even the per-event virtual call, at the cost of a
  header-only engine and worse compile errors. Unmeasurable gain here; kept virtual
  for clarity.
- **Build / codegen:** LTO (`-flto`), profile-guided optimization to lay out hot
  branches from real traffic, `-march=native` for the deployment CPU (traded off
  against a portable binary), and `__builtin_prefetch` of the next level during a
  sweep.
- **System tuning (deployment, not code):** pin to an isolated core
  (`isolcpus`/`taskset`), huge pages for the arena, NUMA-local allocation, the
  performance CPU governor (I literally saw battery throttling when it was low), and `mlock` /
  pre-faulting to avoid runtime page faults.

## Test data & benchmarking

A C++ generator produces deterministic datasets (no third-party dependency).
`gen <scenario> <count> [seed]` writes to stdout:

```bash
./build/gen mixed 1000000 42 > data.in   # one scenario to a file
tools/make_datasets.sh                    # all scenarios -> datasets/
```

Each scenario (deterministic given the seed) targets a different path:

| Scenario | What it generates | What it stresses |
|---|---|---|
| `mixed` | random buys/sells around a drifting mid, ~10% cancels of live orders | realistic blended stream; small, hot book |
| `cross` | every 5th order is aggressive and reaches deep into the other side | matching + remove-filled (large M) |
| `cancel` | half adds (wide spread, no cross), then cancels of those ids | cancel path + index; every cancel empties its level |
| `wide` | buys/sells over a 1-10M price range, never crossing | deep `std::map` (large P): tree depth, cache misses |
| `deep` | all orders at just two prices, never crossing | long FIFO queues per level; time priority |
| `fuzz` | ~30% malformed lines interleaved with valid ones | parser robustness / no-crash |

Small samples are committed under `datasets/`; regenerate large ones with
`make_datasets.sh`.

### Benchmarking

The benchmark builds workloads in memory and times the engine directly (no parsing
or I/O), so use a **Release** build for meaningful numbers:

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release
./build-release/bench            # defaults: 1,000,000 ops, 5 runs
./build-release/bench 2000000 7  # custom count and run count
```

It prints, per phase, the median ns/op and ops/sec, then a per-op latency
distribution (p50..max):

```
add-no-cross      1000000 ops    405.0 ns/op    2469135 ops/sec
add-match         1000000 ops     24.0 ns/op   41666666 ops/sec
cancel            1000000 ops    480.0 ns/op    2083333 ops/sec
cancel-deep        990000 ops     18.0 ns/op   55555555 ops/sec
mixed             1000000 ops     85.0 ns/op   11764705 ops/sec

per-op latency distribution:
clock-floor    p50=0    p90=42   p99=42    p99.9=42     max=...  (ns)
add-no-cross   p50=405  p90=750  p99=1050  p99.9=5000   max=...  (ns)
mixed          p50=42   p90=84   p99=300   p99.9=600    max=...  (ns)
```

Read p99/p99.9 as the stable tail signal; `max` is a single worst sample and is
noisy (OS scheduling). The `clock-floor` row is the measurement overhead itself
(an empty `now()`-`now()`), so per-op values near it are at the clock's resolution.
For end-to-end throughput including parsing and I/O:

```bash
time ./build-release/order_book < datasets/mixed.in > /dev/null
```

## Testing

A tiny self-contained harness (`tests/test_util.hpp`, `CHECK`/`CHECK_EQ`) keeps the
build dependency-free. Run the whole suite with CTest:

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

Coverage: per-component unit tests (price, parser, level, reporter, engine), a
**golden** end-to-end test (`golden_io`) that diffs the real binary's stdout
against the assignment's expected output byte-for-byte, and **stress tests** that
feed large generated inputs (including ~30% malformed lines) through the binary and
assert it never crashes.

Build with sanitizers to catch memory errors and UB across the whole suite:

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DOB_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```

Code is formatted with clang-format (`cmake --build build --target format`).
