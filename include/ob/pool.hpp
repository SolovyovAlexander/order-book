#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ob/order.hpp"

namespace ob {

// A global arena of order nodes shared by every price level. Nodes live in one
// growing vector; freed nodes go on an intrusive free list and are reused, so
// there is no per-order malloc and far better locality than a node-per-order
// std::list. Nodes are referenced by index, not pointer - an index survives the
// vector reallocating (a pointer would dangle), is smaller, and is bounds-checkable.
class Pool {
public:
    using Index = std::uint32_t;
    static constexpr Index kNull = std::numeric_limits<Index>::max();

    struct Node {
        Order order;
        Index prev = kNull;
        Index next = kNull;  // while free, links to the next free node
    };

    // Reserve for the expected peak of concurrent live orders so the steady state
    // never reallocates (a realloc copies the whole arena - a tail-latency spike).
    // The free list reuses slots, so growth is bounded by the peak, not volume.
    static constexpr std::size_t kDefaultCapacity = 1'000'000;
    explicit Pool(std::size_t capacity = kDefaultCapacity) { nodes_.reserve(capacity); }

    // The reference returned by at() is invalidated by a later allocate() (the
    // vector may reallocate), so never hold an at() reference across an add.
    Index allocate(const Order& o) {
        if (free_ != kNull) {
            const Index idx = free_;
            free_ = nodes_[idx].next;
            nodes_[idx].order = o;
            return idx;
        }
        const Index idx = static_cast<Index>(nodes_.size());
        nodes_.push_back(Node{o, kNull, kNull});
        return idx;
    }

    void deallocate(Index idx) {
        nodes_[idx].next = free_;
        free_ = idx;
    }

    Node& at(Index idx) { return nodes_[idx]; }
    const Node& at(Index idx) const { return nodes_[idx]; }

private:
    std::vector<Node> nodes_;
    Index free_ = kNull;
};

}  // namespace ob
