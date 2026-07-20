#pragma once

#include <functional>
#include <map>
#include <unordered_map>

#include "ob/level.hpp"
#include "ob/messages.hpp"
#include "ob/order.hpp"
#include "ob/pool.hpp"
#include "ob/price.hpp"
#include "ob/reporter.hpp"
#include "ob/side.hpp"

namespace ob {

// The matching engine. Holds two price-ordered books and an id index, matches
// aggressive orders against resting ones with price-time priority, and reports
// every trade/fill through the Reporter. Does no I/O itself.
class OrderBook {
public:
    explicit OrderBook(Reporter& reporter) : reporter_(reporter) {
        index_.reserve(Pool::kDefaultCapacity);  // avoid rehash spikes, like the pool
    }

    // Add an order: match it against the opposite side, then rest any remainder.
    // Returns false if `id` is already a live resting order (duplicate - ignored).
    bool add(const AddOrderRequest& req);

    // Cancel a resting order by id. Returns false if no such order exists.
    bool cancel(const CancelOrderRequest& req);

private:
    // asks ascending  -> begin() is the lowest  (best) sell.
    // bids descending -> begin() is the highest (best) buy.
    using Asks = std::map<Price, Level>;
    using Bids = std::map<Price, Level, std::greater<Price>>;

    // Address of a resting order. `level` is a direct back-pointer (std::map nodes
    // are address-stable), so cancel reaches it in O(1) without walking the tree.
    // side/price are only needed to drop an emptied level from its map.
    struct Location {
        Level* level;
        Level::Handle handle;
        Side side;
        Price price;
    };

    // Templated on the book type so buy (vs asks) and sell (vs bids) share one
    // loop - both expose begin() == best.
    template <class SideBook>
    void matchAgainst(AddOrderRequest& order, SideBook& side_book);

    // Place the unfilled remainder of the aggressive order into its own-side book.
    void rest(const AddOrderRequest& order);

    // Does an aggressive order cross a resting price level?
    static bool crosses(const AddOrderRequest& order, Price resting_price) {
        return order.side == Side::Buy ? order.price >= resting_price
                                       : order.price <= resting_price;
    }

    Reporter& reporter_;
    // Declared before the books so it outlives the levels that reference it
    // (members are destroyed in reverse declaration order).
    Pool pool_;
    Asks asks_;
    Bids bids_;
    std::unordered_map<OrderId, Location> index_;
};

}  // namespace ob
