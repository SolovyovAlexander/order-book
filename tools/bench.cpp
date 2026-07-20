// In-process micro-benchmark for the matching engine. Measures each path the
// assignment asks about in isolation, plus overall throughput on a mixed stream.
// Workloads are built in memory (no parsing / no I/O), the engine reports into a
// NullReporter, and each measurement is warmed up once then taken as the median
// of several runs.
//
//   bench [count] [runs]   (defaults: 1000000 5)

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "ob/messages.hpp"
#include "ob/null_reporter.hpp"
#include "ob/order_book.hpp"
#include "ob/price.hpp"
#include "ob/side.hpp"

using namespace ob;
using Clock = std::chrono::steady_clock;

namespace {

Price price(long p) {
    return Price::fromRaw(static_cast<std::int64_t>(p) * Price::SCALE);
}

long long nanos(Clock::duration d) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
}

long long median(std::vector<long long>& v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// Per-op latency percentiles. The tail (p99.9/max) reveals spikes the median
// hides - e.g. an unordered_map rehash copying the whole index in one op.
struct Dist {
    long long p50, p90, p99, p999, max;
};

Dist percentiles(std::vector<long long> lat) {
    std::sort(lat.begin(), lat.end());
    const auto at = [&](double p) {
        return lat[static_cast<std::size_t>(p * static_cast<double>(lat.size() - 1))];
    };
    return {at(0.50), at(0.90), at(0.99), at(0.999), lat.back()};
}

// All buys: with no sells in the book nothing ever crosses, so every add is the
// pure "no match -> insert" path (map insert + list node + index).
std::vector<AddOrderRequest> makeRestOnly(long n, std::mt19937_64& rng) {
    std::uniform_int_distribution<long> pd(1, 5'000'000), qd(1, 100);
    std::vector<AddOrderRequest> v;
    v.reserve(static_cast<std::size_t>(n));
    for (long i = 0; i < n; ++i)
        v.push_back({static_cast<OrderId>(i + 1), Side::Buy, static_cast<Quantity>(qd(rng)),
                     price(pd(rng))});
    return v;
}

std::vector<Request> makeMixed(long n, std::mt19937_64& rng) {
    std::uniform_int_distribution<long> coin(0, 99), off(0, 20), qd(1, 100);
    std::vector<Request> v;
    v.reserve(static_cast<std::size_t>(n));
    std::vector<OrderId> live;
    long mid = 1000;
    OrderId next = 1;
    for (long k = 0; k < n; ++k) {
        if (coin(rng) < 10 && !live.empty()) {
            const auto i = static_cast<std::size_t>(
                std::uniform_int_distribution<long>(0, static_cast<long>(live.size()) - 1)(rng));
            v.push_back(CancelOrderRequest{live[i]});
            live[i] = live.back();
            live.pop_back();
        } else {
            mid += (coin(rng) % 3) - 1;  // -1, 0, 1
            if (mid < 50) mid = 50;
            const int side = static_cast<int>(coin(rng) & 1);
            long p = (side == 0) ? mid - off(rng) : mid + off(rng);
            if (p < 1) p = 1;
            v.push_back(AddOrderRequest{next, side == 0 ? Side::Buy : Side::Sell,
                                        static_cast<Quantity>(qd(rng)), price(p)});
            live.push_back(next);
            ++next;
        }
    }
    return v;
}

long long benchAddNoCross(const std::vector<AddOrderRequest>& work, int runs) {
    std::vector<long long> s;
    for (int r = 0; r <= runs; ++r) {
        NullReporter rep;
        OrderBook book(rep);
        const auto t0 = Clock::now();
        for (const auto& req : work) book.add(req);
        const auto t1 = Clock::now();
        if (r > 0) s.push_back(nanos(t1 - t0));
    }
    return median(s);
}

long long benchAddMatch(long n, std::mt19937_64& rng, int runs) {
    // resting sells and aggressive buys, 1:1 at the same price -> every timed add
    // does one trade and removes one filled resting order.
    std::vector<AddOrderRequest> resting, aggressive;
    resting.reserve(static_cast<std::size_t>(n));
    aggressive.reserve(static_cast<std::size_t>(n));
    std::uniform_int_distribution<long> qd(1, 100);
    for (long i = 0; i < n; ++i) {
        const auto q = static_cast<Quantity>(qd(rng));
        resting.push_back({static_cast<OrderId>(i + 1), Side::Sell, q, price(1000)});
        aggressive.push_back({static_cast<OrderId>(n + i + 1), Side::Buy, q, price(1000)});
    }
    std::vector<long long> s;
    for (int r = 0; r <= runs; ++r) {
        NullReporter rep;
        OrderBook book(rep);
        for (const auto& req : resting) book.add(req);  // untimed setup
        const auto t0 = Clock::now();
        for (const auto& req : aggressive) book.add(req);  // timed
        const auto t1 = Clock::now();
        if (r > 0) s.push_back(nanos(t1 - t0));
    }
    return median(s);
}

long long benchCancel(long n, std::mt19937_64& rng, int runs) {
    const auto resting = makeRestOnly(n, rng);
    std::vector<CancelOrderRequest> cancels;
    cancels.reserve(static_cast<std::size_t>(n));
    for (const auto& a : resting) cancels.push_back({a.id});
    std::vector<long long> s;
    for (int r = 0; r <= runs; ++r) {
        NullReporter rep;
        OrderBook book(rep);
        for (const auto& req : resting) book.add(req);  // untimed setup
        const auto t0 = Clock::now();
        for (const auto& req : cancels) book.cancel(req);  // timed
        const auto t1 = Clock::now();
        if (r > 0) s.push_back(nanos(t1 - t0));
    }
    return median(s);
}

// Deep queues: n orders spread over L price levels, then cancel all but the last
// order in each level. No cancel empties its level, so the back-pointer reaches
// the order directly and skips the price-tree walk entirely - the case where it
// pays off (unlike the sparse `cancel` phase, where every cancel empties).
long long benchCancelDeep(long n, std::mt19937_64& rng, int runs, long& timed_ops) {
    constexpr long L = 10000;
    const long D = (n / L > 0) ? n / L : 1;
    std::vector<AddOrderRequest> resting;
    std::vector<CancelOrderRequest> cancels;
    std::uniform_int_distribution<long> qd(1, 100);
    OrderId id = 1;
    for (long lvl = 0; lvl < L; ++lvl) {
        for (long d = 0; d < D; ++d) {
            resting.push_back({id, Side::Buy, static_cast<Quantity>(qd(rng)), price(lvl + 1)});
            if (d < D - 1) cancels.push_back({id});  // leave the last -> level stays non-empty
            ++id;
        }
    }
    timed_ops = static_cast<long>(cancels.size());
    std::vector<long long> s;
    for (int r = 0; r <= runs; ++r) {
        NullReporter rep;
        OrderBook book(rep);
        for (const auto& req : resting) book.add(req);  // untimed setup
        const auto t0 = Clock::now();
        for (const auto& c : cancels) book.cancel(c);  // timed
        const auto t1 = Clock::now();
        if (r > 0) s.push_back(nanos(t1 - t0));
    }
    return median(s);
}

long long benchMixed(const std::vector<Request>& work, int runs) {
    std::vector<long long> s;
    for (int r = 0; r <= runs; ++r) {
        NullReporter rep;
        OrderBook book(rep);
        const auto t0 = Clock::now();
        for (const auto& req : work) {
            if (const auto* add = std::get_if<AddOrderRequest>(&req))
                book.add(*add);
            else
                book.cancel(std::get<CancelOrderRequest>(req));
        }
        const auto t1 = Clock::now();
        if (r > 0) s.push_back(nanos(t1 - t0));
    }
    return median(s);
}

std::vector<long long> measureClockFloor(long n) {
    std::vector<long long> lat;
    lat.reserve(static_cast<std::size_t>(n));
    for (long i = 0; i < n; ++i) {
        const auto t0 = Clock::now();
        const auto t1 = Clock::now();  // empty body: pure measurement floor
        lat.push_back(nanos(t1 - t0));
    }
    return lat;
}

std::vector<long long> measureAddLatency(const std::vector<AddOrderRequest>& work) {
    NullReporter rep;
    OrderBook book(rep);
    std::vector<long long> lat;
    lat.reserve(work.size());
    for (const auto& req : work) {
        const auto t0 = Clock::now();
        book.add(req);
        const auto t1 = Clock::now();
        lat.push_back(nanos(t1 - t0));
    }
    return lat;
}

std::vector<long long> measureMixedLatency(const std::vector<Request>& work) {
    NullReporter rep;
    OrderBook book(rep);
    std::vector<long long> lat;
    lat.reserve(work.size());
    for (const auto& req : work) {
        const auto t0 = Clock::now();
        if (const auto* a = std::get_if<AddOrderRequest>(&req))
            book.add(*a);
        else
            book.cancel(std::get<CancelOrderRequest>(req));
        const auto t1 = Clock::now();
        lat.push_back(nanos(t1 - t0));
    }
    return lat;
}

void reportDist(const char* name, const Dist& d) {
    std::printf("%-14s p50=%5lld  p90=%5lld  p99=%6lld  p99.9=%8lld  max=%9lld  (ns)\n", name,
                d.p50, d.p90, d.p99, d.p999, d.max);
}

void report(const char* name, long long ns, long ops) {
    const double per = static_cast<double>(ns) / static_cast<double>(ops);
    const double rate = static_cast<double>(ops) * 1e9 / static_cast<double>(ns);
    std::printf("%-16s %10ld ops  %8.1f ns/op  %12.0f ops/sec\n", name, ops, per, rate);
}

}  // namespace

int main(int argc, char** argv) {
    const long count = (argc >= 2) ? std::strtol(argv[1], nullptr, 10) : 1'000'000;
    const int runs = (argc >= 3) ? static_cast<int>(std::strtol(argv[2], nullptr, 10)) : 5;

    std::mt19937_64 rng(42);
    const auto restOnly = makeRestOnly(count, rng);

    std::printf("count=%ld runs=%d (median reported)\n", count, runs);
    report("add-no-cross", benchAddNoCross(restOnly, runs), count);
    report("add-match", benchAddMatch(count, rng, runs), count);
    report("cancel", benchCancel(count, rng, runs), count);

    long deep_ops = 0;
    const long long deep_ns = benchCancelDeep(count, rng, runs, deep_ops);
    report("cancel-deep", deep_ns, deep_ops);

    const auto mixed = makeMixed(count, rng);
    report("mixed", benchMixed(mixed, runs), static_cast<long>(mixed.size()));

    // Tail latency: per-op timing (single pass). Small percentiles include the
    // clock-floor overhead shown first; the tail exposes spikes the median hides.
    std::printf("\nper-op latency distribution:\n");
    reportDist("clock-floor", percentiles(measureClockFloor(count)));
    reportDist("add-no-cross", percentiles(measureAddLatency(restOnly)));
    reportDist("mixed", percentiles(measureMixedLatency(mixed)));
    return 0;
}
