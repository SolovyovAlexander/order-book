#pragma once

#include <cstddef>

#include "ob/order.hpp"
#include "ob/pool.hpp"

namespace ob {

// One price level: a FIFO queue of resting orders, oldest at the front (price-
// time priority - match front() first, append newcomers at the back). The queue
// is an intrusive doubly-linked list threaded through the shared Pool, so a Level
// only holds head/tail indices. `Handle` is an opaque pool index that stays valid
// as other orders come and go, which is what gives the engine O(1) cancel.
class Level {
public:
    using Handle = Pool::Index;

    explicit Level(Pool& pool) : pool_(pool) {}

    Handle add(const Order& order) {
        const Handle h = pool_.allocate(order);  // may grow the pool; take refs after
        Pool::Node& node = pool_.at(h);
        node.prev = tail_;
        node.next = Pool::kNull;
        if (tail_ == Pool::kNull)
            head_ = h;
        else
            pool_.at(tail_).next = h;
        tail_ = h;
        ++size_;
        return h;
    }

    Order& front() { return pool_.at(head_).order; }  // caller ensures !empty()
    Order& at(Handle h) { return pool_.at(h).order; }

    void popFront() { unlink(head_); }
    void remove(Handle h) { unlink(h); }  // O(1)

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }

private:
    void unlink(Handle h) {
        const Pool::Node& node = pool_.at(h);
        const Handle prev = node.prev;
        const Handle next = node.next;
        if (prev == Pool::kNull)
            head_ = next;
        else
            pool_.at(prev).next = next;
        if (next == Pool::kNull)
            tail_ = prev;
        else
            pool_.at(next).prev = prev;
        pool_.deallocate(h);
        --size_;
    }

    Pool& pool_;
    Handle head_ = Pool::kNull;
    Handle tail_ = Pool::kNull;
    std::size_t size_ = 0;
};

}  // namespace ob
