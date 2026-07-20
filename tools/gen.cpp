// Test-data generator for the matching engine. Deterministic given a seed.
//
//   gen <scenario> <count> [seed]
//
// Scenarios (each stresses a different path):
//   mixed   - random adds around a drifting mid + ~10% cancels (general profile)
//   cross   - frequent aggressive orders sweeping levels (match + remove-filled)
//   cancel  - bulk adds, then bulk cancels (cancel path + index)
//   wide    - prices over a huge range, no crossing (deep map, large P)
//   deep    - prices from a tiny set (few levels, long FIFO queues)
//   fuzz    - ~30% malformed lines mixed in (parser robustness / no-crash)

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {

struct Generator {
    std::mt19937_64 rng;
    std::uint64_t next_id = 1;
    std::vector<std::uint64_t> live;  // ids added but not yet cancelled by us
    std::string out;

    explicit Generator(std::uint64_t seed) : rng(seed) { out.reserve(std::size_t{1} << 20); }

    long uni(long lo, long hi) { return std::uniform_int_distribution<long>(lo, hi)(rng); }

    void add(int side, long qty, long price) {
        const std::uint64_t id = next_id++;
        out += "0,";
        out += std::to_string(id);
        out += (side == 0) ? ",0," : ",1,";
        out += std::to_string(qty);
        out += ',';
        out += std::to_string(price);
        out += '\n';
        live.push_back(id);
    }

    void cancelRandom() {
        if (live.empty()) return;
        const auto i = static_cast<std::size_t>(uni(0, static_cast<long>(live.size()) - 1));
        const std::uint64_t id = live[i];
        live[i] = live.back();
        live.pop_back();
        out += "1,";
        out += std::to_string(id);
        out += '\n';
    }

    void raw(const char* line) {
        out += line;
        out += '\n';
    }

    void flush() { std::fwrite(out.data(), 1, out.size(), stdout); }
};

void mixed(Generator& g, long n) {
    long mid = 1000;
    for (long k = 0; k < n; ++k) {
        if (g.uni(0, 99) < 10) {
            g.cancelRandom();
            continue;
        }
        mid += g.uni(-1, 1);
        if (mid < 50) mid = 50;
        const int side = static_cast<int>(g.uni(0, 1));
        const long offset = g.uni(0, 20);
        long price = (side == 0) ? mid - offset : mid + offset;  // spread -> mostly rests
        if (price < 1) price = 1;
        g.add(side, g.uni(1, 100), price);
    }
}

void cross(Generator& g, long n) {
    const long mid = 1000;
    for (long k = 0; k < n; ++k) {
        if (k % 5 == 0) {
            // aggressive order reaching deep into the other side, large quantity
            const int side = static_cast<int>(g.uni(0, 1));
            const long price = (side == 0) ? mid + 50 : mid - 50;
            g.add(side, g.uni(50, 200), price < 1 ? 1 : price);
        } else {
            const int side = static_cast<int>(g.uni(0, 1));
            const long offset = g.uni(0, 10);
            long price = (side == 0) ? mid - offset : mid + offset;
            if (price < 1) price = 1;
            g.add(side, g.uni(1, 20), price);
        }
    }
}

void cancel(Generator& g, long n) {
    const long half = n / 2;
    const long mid = 1000;
    for (long k = 0; k < half; ++k) {
        const int side = static_cast<int>(g.uni(0, 1));
        const long offset = g.uni(1, 50);
        long price = (side == 0) ? mid - offset : mid + offset;  // wide spread, never crosses
        if (price < 1) price = 1;
        g.add(side, g.uni(1, 100), price);
    }
    for (long k = half; k < n; ++k) g.cancelRandom();
}

void wide(Generator& g, long n) {
    for (long k = 0; k < n; ++k) {
        if (g.uni(0, 99) < 5) {
            g.cancelRandom();
            continue;
        }
        const int side = static_cast<int>(g.uni(0, 1));
        // buys below the split, sells above it -> no crossing, maximal distinct levels
        const long price = (side == 0) ? g.uni(1, 5'000'000) : g.uni(5'000'001, 10'000'000);
        g.add(side, g.uni(1, 100), price);
    }
}

void deep(Generator& g, long n) {
    for (long k = 0; k < n; ++k) {
        const int side = static_cast<int>(g.uni(0, 1));
        const long price = (side == 0) ? 1000 : 1002;  // two levels, never cross -> deep queues
        g.add(side, g.uni(1, 100), price);
    }
}

void fuzz(Generator& g, long n) {
    static const char* const junk[] = {
        "BADMESSAGE", "0,1,2,3",  "1",   "0,abc,0,5,100", "0,1,0,0,100",
        "9,9,9",      "0,1,0,5,", ",,,", "0,1,0,5,1.2.3", "hello world",
    };
    const long junkCount = static_cast<long>(sizeof(junk) / sizeof(junk[0]));
    long mid = 1000;
    for (long k = 0; k < n; ++k) {
        if (g.uni(0, 99) < 30) {
            g.raw(junk[g.uni(0, junkCount - 1)]);
        } else if (g.uni(0, 99) < 10) {
            g.cancelRandom();
        } else {
            const int side = static_cast<int>(g.uni(0, 1));
            const long offset = g.uni(0, 20);
            long price = (side == 0) ? mid - offset : mid + offset;
            if (price < 1) price = 1;
            g.add(side, g.uni(1, 100), price);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <mixed|cross|cancel|wide|deep|fuzz> <count> [seed]\n",
                     argv[0]);
        return 2;
    }
    const std::string scenario = argv[1];
    const long count = std::strtol(argv[2], nullptr, 10);
    const std::uint64_t seed = (argc >= 4) ? std::strtoull(argv[3], nullptr, 10) : 42;

    Generator g(seed);
    if (scenario == "mixed")
        mixed(g, count);
    else if (scenario == "cross")
        cross(g, count);
    else if (scenario == "cancel")
        cancel(g, count);
    else if (scenario == "wide")
        wide(g, count);
    else if (scenario == "deep")
        deep(g, count);
    else if (scenario == "fuzz")
        fuzz(g, count);
    else {
        std::fprintf(stderr, "unknown scenario: %s\n", scenario.c_str());
        return 2;
    }
    g.flush();
    return 0;
}
