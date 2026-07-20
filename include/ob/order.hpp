#pragma once

#include <cstdint>

namespace ob {

using OrderId = std::uint64_t;
using Quantity = std::uint64_t;

// A resting order in the book. Only what matching needs: price is the level's
// key and side is which book it's in, so neither is stored here (the full inbound
// order is AddOrderRequest). quantity is the remaining open amount, shrinking as
// the order fills.
struct Order {
    OrderId id = 0;
    Quantity quantity = 0;
};

}  // namespace ob
